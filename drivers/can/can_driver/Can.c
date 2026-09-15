#include "Can.h"
#include "S32K144.h"
#include <stddef.h>

/* Temporary CAN0 profile; the BSP owns oscillator, pins and transceiver setup. */
#define CAN_TX_MB_INDEX             (8U)
#define CAN_RX_MB_INDEX             (9U)
#define CAN_MB_WORD_COUNT           (4U)
#define CAN_TX_MB_BASE              (CAN_TX_MB_INDEX * CAN_MB_WORD_COUNT)
#define CAN_RX_MB_BASE              (CAN_RX_MB_INDEX * CAN_MB_WORD_COUNT)
#define CAN_TX_FLAG                 (1UL << CAN_TX_MB_INDEX)
#define CAN_RX_FLAG                 (1UL << CAN_RX_MB_INDEX)
#define CAN_STANDARD_ID_MAX         (0x7FFUL)
#define CAN_STANDARD_ID_SHIFT       (18U)
#define CAN_MAX_DATA_LENGTH         (8U)
#define CAN_MB_CODE_SHIFT           (24U)
#define CAN_MB_CODE_MASK            (0x0FUL << CAN_MB_CODE_SHIFT)
#define CAN_MB_CODE_RX_FULL         (2U)
#define CAN_MB_CODE_RX_EMPTY        (4U)
#define CAN_MB_CODE_RX_OVERRUN      (6U)
#define CAN_MB_CODE_TX_INACTIVE     (8U)
#define CAN_MB_CODE_TX_DATA         (12U)
#define CAN_MB_EDL_MASK             (1UL << 31U)
#define CAN_WAIT_LIMIT              (100000UL)
#define CAN_RX_BUSY_LIMIT           (32U)
#define CAN_LOG_CAPACITY            (16U)

/* CanIf must provide these callbacks when the communication stack is linked. */
/** Notify CanIf of the software PDU handle whose accepted request completed. */
extern void CanIf_TxConfirmation(PduIdType TxPduId);
/** Deliver one frame; RxPdu and its bytes are valid only during this call. */
extern void CanIf_RxIndication(Can_HwHandleType Hrh, const Can_RxPduType *RxPdu);

typedef enum
{
    CAN_STATE_UNINIT = 0,
    CAN_STATE_READY,
    CAN_STATE_ERROR
} Can_StateType;

/* Keep timeout stage values stable for existing diagnostic traces. */
typedef enum
{
    CAN_INIT_DISABLE_ACK = 1,
    CAN_INIT_ENABLE_ACK = 2,
    CAN_INIT_FREEZE_ENTRY = 3,
    CAN_INIT_RESET_COMPLETE = 4,
    CAN_INIT_RESET_FREEZE_ACK = 5,
    CAN_INIT_FREEZE_EXIT = 6,
    CAN_INIT_READY_ACK = 7,
    CAN_INIT_PRE_DISABLE_FREEZE = 8
} Can_InitStageType;

typedef enum
{
    CAN_LOG_INIT_OK = 0,
    CAN_LOG_REJECTED,
    CAN_LOG_CLOCK_ERROR,
    CAN_LOG_INIT_TIMEOUT,
    CAN_LOG_TX_ACCEPTED,
    CAN_LOG_TX_BUSY,
    CAN_LOG_TX_COMPLETE,
    CAN_LOG_HW_ERROR,
    CAN_LOG_BUS_OFF,
    CAN_LOG_RX_FRAME,
    CAN_LOG_RX_INVALID,
    CAN_LOG_RX_OVERRUN,
    CAN_LOG_RX_BUSY
} Can_LogEventType;

typedef struct
{
    uint32_t sequence;
    Can_LogEventType event;
    uint32_t detail;
    PduIdType swPduHandle;
    Can_IdType canId;
} Can_LogRecordType;

static Can_StateType Can_State = CAN_STATE_UNINIT;
static uint8_t Can_TxBusy = 0U;
static PduIdType Can_TxHandle = 0U;
static Can_IdType Can_TxId = 0U;

/* Inspect these volatile structured records in the debugger; no UART required. */
static volatile Can_LogRecordType Can_LogRecords[CAN_LOG_CAPACITY];
static volatile uint32_t Can_LogSequence = 0U;

/** Store a bounded diagnostic event; detail holds a reason, stage or HW snapshot. */
static void Can_Log(Can_LogEventType event, uint32_t detail)
{
    uint32_t sequence = Can_LogSequence;
    uint32_t index = sequence % CAN_LOG_CAPACITY;

    Can_LogRecords[index].event = event;
    Can_LogRecords[index].detail = detail;
    Can_LogRecords[index].swPduHandle = Can_TxHandle;
    Can_LogRecords[index].canId = Can_TxId;
    Can_LogRecords[index].sequence = sequence;
    Can_LogSequence = sequence + 1U;
}

/** Wait a bounded number of reads for masked bits; return zero on timeout. */
static uint8_t Can_WaitRegister(const volatile uint32_t *address,
                                uint32_t mask, uint32_t expected)
{
    uint32_t remaining;

    for (remaining = CAN_WAIT_LIMIT; remaining > 0U; remaining--)
    {
        if (((*address) & mask) == expected)
        {
            return 1U;
        }
    }
    return 0U;
}

/** Latch a fatal fault and request Freeze without confirming a pending Tx. */
static void Can_Stop(Can_LogEventType event, uint32_t detail)
{
    Can_State = CAN_STATE_ERROR;
    /* Disable requires a prior Freeze acknowledgement; Init performs that wait. */
    CAN0->MCR |= CAN_MCR_FRZ_MASK | CAN_MCR_HALT_MASK;
    Can_Log(event, detail);
}

/** Check HW health; log transient CAN errors and latch bus-off or lost readiness. */
static uint8_t Can_CheckController(void)
{
    uint32_t status = CAN0->ESR1;
    uint32_t errors = status & (CAN_ESR1_ERRINT_MASK | CAN_ESR1_BOFFINT_MASK);

    if (errors != 0U)
    {
        CAN0->ESR1 = errors;
    }
    if (((status & CAN_ESR1_FLTCONF_MASK) >= CAN_ESR1_FLTCONF(2U)) ||
        ((status & CAN_ESR1_BOFFINT_MASK) != 0U))
    {
        Can_Stop(CAN_LOG_BUS_OFF, status);
        return 0U;
    }
    if ((status & CAN_ESR1_ERRINT_MASK) != 0U)
    {
        /* Hardware may retry an accepted request; an error is not completion. */
        Can_Log(CAN_LOG_HW_ERROR, status);
    }
    status = CAN0->MCR;
    if ((status & (CAN_MCR_MDIS_MASK | CAN_MCR_HALT_MASK |
                   CAN_MCR_FRZACK_MASK | CAN_MCR_NOTRDY_MASK)) != 0U)
    {
        Can_Stop(CAN_LOG_HW_ERROR, status);
        return 0U;
    }
    return 1U;
}

/** Wait for an init handshake; latch ERROR and record its stage on timeout. */
static Can_ReturnType Can_WaitInitStatus(uint32_t mask, uint32_t expected,
                                       Can_InitStageType stage)
{
    if (Can_WaitRegister(&CAN0->MCR, mask, expected) == 0U)
    {
        Can_Stop(CAN_LOG_INIT_TIMEOUT, (uint32_t)stage);
        return CAN_NOT_OK;
    }
    return CAN_OK;
}

/** Request Freeze on an enabled CAN0 and wait for FRZACK with a bounded timeout. */
static Can_ReturnType Can_EnterFreezeMode(Can_InitStageType stage)
{
    CAN0->MCR |= CAN_MCR_FRZ_MASK | CAN_MCR_HALT_MASK;
    return Can_WaitInitStatus(CAN_MCR_FRZACK_MASK, CAN_MCR_FRZACK_MASK, stage);
}

/** Enter Disable safely, wait for LPMACK, then select the 8 MHz oscillator clock. */
static Can_ReturnType Can_DisableController(void)
{
    if ((CAN0->MCR & CAN_MCR_MDIS_MASK) == 0U)
    {
        if (Can_EnterFreezeMode(CAN_INIT_PRE_DISABLE_FREEZE) != CAN_OK)
        {
            return CAN_NOT_OK;
        }
    }
    CAN0->MCR |= CAN_MCR_MDIS_MASK;
    if (Can_WaitInitStatus(CAN_MCR_LPMACK_MASK, CAN_MCR_LPMACK_MASK,
                          CAN_INIT_DISABLE_ACK) != CAN_OK)
    {
        return CAN_NOT_OK;
    }
    /* CLKSRC may only be selected while the module is disabled. */
    CAN0->CTRL1 &= ~CAN_CTRL1_CLKSRC_MASK;
    return CAN_OK;
}

/** Enable CAN0 with Freeze requested and wait until Disable mode is exited. */
static Can_ReturnType Can_EnableController(void)
{
    CAN0->MCR = (CAN0->MCR & ~CAN_MCR_MDIS_MASK) |
                 CAN_MCR_FRZ_MASK | CAN_MCR_HALT_MASK;
    return Can_WaitInitStatus(CAN_MCR_LPMACK_MASK, 0U, CAN_INIT_ENABLE_ACK);
}

/** Soft-reset stale protocol state and wait for reset completion and Freeze ACK. */
static Can_ReturnType Can_ResetController(void)
{
    CAN0->MCR |= CAN_MCR_SOFTRST_MASK;
    if (Can_WaitInitStatus(CAN_MCR_SOFTRST_MASK, 0U,
                          CAN_INIT_RESET_COMPLETE) != CAN_OK)
    {
        return CAN_NOT_OK;
    }
    return Can_WaitInitStatus(CAN_MCR_FRZACK_MASK, CAN_MCR_FRZACK_MASK,
                             CAN_INIT_RESET_FREEZE_ACK);
}

/** Configure the fixed 500 kbit/s Classical CAN profile while CAN0 is frozen. */
static void Can_ConfigureController(void)
{
    /* Preserve reserved bits; disable FIFO, FD, DMA, priority and PN modes. */
    CAN0->MCR = (CAN0->MCR &
                 ~(CAN_MCR_RFEN_MASK | CAN_MCR_FDEN_MASK | CAN_MCR_AEN_MASK |
                   CAN_MCR_LPRIOEN_MASK | CAN_MCR_PNET_EN_MASK | CAN_MCR_DMA_MASK |
                   CAN_MCR_IDAM_MASK | CAN_MCR_MAXMB_MASK | CAN_MCR_WRNEN_MASK)) |
                 CAN_MCR_FRZ_MASK | CAN_MCR_HALT_MASK | CAN_MCR_IRMQ_MASK |
                 CAN_MCR_SRXDIS_MASK | CAN_MCR_MAXMB(15U);
    CAN0->CBT = 0U; /* BTF=0 selects CTRL1 bit timing. */
    CAN0->FDCTRL = 0U;
    /* 8 MHz / (1 prescaler * (1 sync + 7 prop + 4 phase1 + 4 phase2)) = 500k.
     * Sample point = 75%, SJW = 4 TQ, single sampling, normal bus operation. */
    CAN0->CTRL1 = CAN_CTRL1_PRESDIV(0U) | CAN_CTRL1_PROPSEG(6U) |
                  CAN_CTRL1_PSEG1(3U) | CAN_CTRL1_PSEG2(3U) | CAN_CTRL1_RJW(3U);
    /* EACEN=0 always compares IDE; RRS=1 stores remote frames for rejection. */
    CAN0->CTRL2 = (CAN0->CTRL2 & ~CAN_CTRL2_EACEN_MASK) | CAN_CTRL2_RRS_MASK;
    CAN0->IMASK1 = 0U;
}

/** Initialize embedded RAM and masks, arm Tx MB8/Rx MB9 and clear stale flags. */
static void Can_InitMessageBuffers(void)
{
    uint32_t index;

    /* Clear the entire embedded RAM, including inactive MB data words. */
    for (index = 0U; index < CAN_RAMn_COUNT; index++)
    {
        CAN0->RAMn[index] = 0U;
    }
    for (index = 0U; index < CAN_RXIMR_COUNT; index++)
    {
        CAN0->RXIMR[index] = UINT32_MAX;
    }
    CAN0->RXMGMASK = UINT32_MAX;
    CAN0->RXIMR[CAN_RX_MB_INDEX] = 0U;
    CAN0->RAMn[CAN_TX_MB_BASE] = CAN_MB_CODE_TX_INACTIVE << CAN_MB_CODE_SHIFT;
    CAN0->RAMn[CAN_RX_MB_BASE] = CAN_MB_CODE_RX_EMPTY << CAN_MB_CODE_SHIFT;
    CAN0->IFLAG1 = UINT32_MAX;
    CAN0->ESR1 = CAN_ESR1_ERRINT_MASK | CAN_ESR1_BOFFINT_MASK;
}

/** Exit Freeze and wait for both FRZACK and NOTRDY to clear before bus use. */
static Can_ReturnType Can_ExitFreezeMode(void)
{
    CAN0->MCR &= ~(CAN_MCR_HALT_MASK | CAN_MCR_FRZ_MASK);
    if (Can_WaitInitStatus(CAN_MCR_FRZACK_MASK, 0U, CAN_INIT_FREEZE_EXIT) != CAN_OK)
    {
        return CAN_NOT_OK;
    }
    return Can_WaitInitStatus(CAN_MCR_NOTRDY_MASK, 0U, CAN_INIT_READY_ACK);
}

/**
 * Initialize CAN0 for 500 kbit/s standard Classical CAN with the 8 MHz SOSC.
 * MB8 transmits and MB9 accepts all standard IDs; CanIf filters logical PDUs.
 * BSP setup must precede this call. Static helpers implement each hardware stage.
 * Return CAN_NOT_OK on clock/handshake failure or an already initialized driver.
 */
Can_ReturnType Can_Init(void)
{
    if (Can_State == CAN_STATE_READY)
    {
        Can_Log(CAN_LOG_REJECTED, 1U);
        return CAN_NOT_OK;
    }
    if ((SCG->SOSCCSR & SCG_SOSCCSR_SOSCVLD_MASK) == 0U)
    {
        Can_Log(CAN_LOG_CLOCK_ERROR, 0U);
        return CAN_NOT_OK;
    }

    PCC->PCCn[PCC_FlexCAN0_INDEX] |= PCC_PCCn_CGC_MASK;
    if ((Can_DisableController() != CAN_OK) ||
        (Can_EnableController() != CAN_OK) ||
        (Can_EnterFreezeMode(CAN_INIT_FREEZE_ENTRY) != CAN_OK) ||
        (Can_ResetController() != CAN_OK))
    {
        return CAN_NOT_OK;
    }
    Can_ConfigureController();
    Can_InitMessageBuffers();
    if (Can_ExitFreezeMode() != CAN_OK)
    {
        return CAN_NOT_OK;
    }

    Can_TxBusy = 0U;
    Can_TxHandle = 0U;
    Can_TxId = 0U;
    Can_State = CAN_STATE_READY;
    Can_Log(CAN_LOG_INIT_OK, 500000U);
    return CAN_OK;
}

/**
 * Accept one standard CAN0 HTH0/MB8 data frame without waiting for the bus.
 * Copy all bytes before CAN_OK; retain the software PDU handle for completion.
 * Return CAN_BUSY until polling frees MB8, or CAN_NOT_OK for invalid input/fault.
 */
Can_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType *PduInfo)
{
    uint32_t words[2] = {0U, 0U};
    uint32_t code;
    uint8_t index;

    if ((Can_State != CAN_STATE_READY) || (Hth != CAN_HTH_CAN0_TX) ||
        (PduInfo == NULL))
    {
        Can_Log(CAN_LOG_REJECTED, 2U);
        return CAN_NOT_OK;
    }
    if ((PduInfo->id > CAN_STANDARD_ID_MAX) ||
        (PduInfo->length > CAN_MAX_DATA_LENGTH) ||
        ((PduInfo->length > 0U) && (PduInfo->sdu == NULL)))
    {
        Can_Log(CAN_LOG_REJECTED, 3U);
        return CAN_NOT_OK;
    }
    if (Can_CheckController() == 0U)
    {
        return CAN_NOT_OK;
    }
    if (Can_TxBusy != 0U)
    {
        Can_Log(CAN_LOG_TX_BUSY, CAN_TX_MB_INDEX);
        return CAN_BUSY;
    }
    code = (CAN0->RAMn[CAN_TX_MB_BASE] & CAN_MB_CODE_MASK)
           >> CAN_MB_CODE_SHIFT;
    if (code != CAN_MB_CODE_TX_INACTIVE)
    {
        /* MB8 is exclusively owned; another code without a request is a fault. */
        Can_Stop(CAN_LOG_HW_ERROR, code);
        return CAN_NOT_OK;
    }
    for (index = 0U; index < PduInfo->length; index++)
    {
        words[index / 4U] |= (uint32_t)PduInfo->sdu[index] <<
                             (24U - ((index % 4U) * 8U));
    }
    /* Clear only MB8's stale completion flag, preserving pending Rx. */
    CAN0->IFLAG1 = CAN_TX_FLAG;
    CAN0->RAMn[CAN_TX_MB_BASE + 1U] = PduInfo->id << CAN_STANDARD_ID_SHIFT;
    CAN0->RAMn[CAN_TX_MB_BASE + 2U] = words[0];
    CAN0->RAMn[CAN_TX_MB_BASE + 3U] = words[1];
    Can_TxHandle = PduInfo->swPduHandle;
    Can_TxId = PduInfo->id;
    Can_TxBusy = 1U;
    /* CS is written last so hardware sees a complete frame snapshot. */
    CAN0->RAMn[CAN_TX_MB_BASE] = (CAN_MB_CODE_TX_DATA << CAN_MB_CODE_SHIFT) |
                                CAN_WMBn_CS_DLC(PduInfo->length);
    Can_Log(CAN_LOG_TX_ACCEPTED, PduInfo->length);
    return CAN_OK;
}

/** Poll MB8 completion once; release it before confirming the saved PDU handle. */
void Can_MainFunction_Write(void)
{
    uint32_t code;
    PduIdType completedHandle;

    if ((Can_State != CAN_STATE_READY) || (Can_CheckController() == 0U))
    {
        return;
    }
    if ((Can_TxBusy == 0U) ||
        ((CAN0->IFLAG1 & CAN_TX_FLAG) == 0U))
    {
        return;
    }
    code = (CAN0->RAMn[CAN_TX_MB_BASE] & CAN_MB_CODE_MASK)
           >> CAN_MB_CODE_SHIFT;
    if (code != CAN_MB_CODE_TX_INACTIVE)
    {
        /* Aborted/unexpected completion must not produce a success callback. */
        Can_Stop(CAN_LOG_HW_ERROR, code);
        return;
    }
    completedHandle = Can_TxHandle;
    CAN0->IFLAG1 = CAN_TX_FLAG;
    Can_TxBusy = 0U;
    Can_Log(CAN_LOG_TX_COMPLETE, CAN_TX_MB_INDEX);
    CanIf_TxConfirmation(completedHandle);
}

/**
 * Poll MB9 once and deliver a standard Classical CAN data frame with HRH1.
 * Clear IFLAG before reading TIMER to unlock the MB, as required by the RM.
 * Do not force RX_EMPTY after servicing: hardware keeps the MB receivable.
 * The callback must consume/copy the local payload before returning.
 */
void Can_MainFunction_Read(void)
{
    uint32_t cs = 0U;
    uint32_t code;
    uint32_t id;
    uint32_t words[2];
    uint8_t data[CAN_MAX_DATA_LENGTH];
    uint8_t index;
    Can_RxPduType rxPdu;

    if ((Can_State != CAN_STATE_READY) || (Can_CheckController() == 0U))
    {
        return;
    }
    if ((CAN0->IFLAG1 & CAN_RX_FLAG) == 0U)
    {
        return;
    }
    /* CODE[0] is BUSY during move-in; bound retries to keep polling responsive. */
    for (index = 0U; index < CAN_RX_BUSY_LIMIT; index++)
    {
        cs = CAN0->RAMn[CAN_RX_MB_BASE];
        if ((cs & (1UL << CAN_MB_CODE_SHIFT)) == 0U)
        {
            break;
        }
    }
    if (index == CAN_RX_BUSY_LIMIT)
    {
        (void)CAN0->TIMER;
        Can_Log(CAN_LOG_RX_BUSY, cs);
        return; /* Keep IFLAG pending for the next invocation. */
    }

    id = CAN0->RAMn[CAN_RX_MB_BASE + 1U];
    words[0] = CAN0->RAMn[CAN_RX_MB_BASE + 2U];
    words[1] = CAN0->RAMn[CAN_RX_MB_BASE + 3U];
    CAN0->IFLAG1 = CAN_RX_FLAG;
    (void)CAN0->TIMER;

    code = (cs & CAN_MB_CODE_MASK) >> CAN_MB_CODE_SHIFT;
    rxPdu.canId = (id >> CAN_STANDARD_ID_SHIFT) & CAN_STANDARD_ID_MAX;
    rxPdu.length = (PduLengthType)((cs & CAN_WMBn_CS_DLC_MASK) >>
                                   CAN_WMBn_CS_DLC_SHIFT);
    rxPdu.dataPtr = data;
    if (((code != CAN_MB_CODE_RX_FULL) && (code != CAN_MB_CODE_RX_OVERRUN)) ||
        ((cs & (CAN_WMBn_CS_IDE_MASK | CAN_WMBn_CS_RTR_MASK | CAN_MB_EDL_MASK)) != 0U) ||
        (rxPdu.length > CAN_MAX_DATA_LENGTH))
    {
        Can_Log(CAN_LOG_RX_INVALID, cs);
        return;
    }
    if (code == CAN_MB_CODE_RX_OVERRUN)
    {
        /* The previous frame was lost; deliver the latest valid frame. */
        Can_Log(CAN_LOG_RX_OVERRUN, rxPdu.canId);
    }
    for (index = 0U; index < rxPdu.length; index++)
    {
        data[index] = (uint8_t)(words[index / 4U] >>
                                (24U - ((index % 4U) * 8U)));
    }
    Can_Log(CAN_LOG_RX_FRAME, rxPdu.canId);
    CanIf_RxIndication(CAN_HRH_CAN0_RX, &rxPdu);
}
