#include "Can.h"
#include "Can_Internal.h"

#include "S32K144.h"

#include <stddef.h>

#define CAN_MB_WORD_COUNT             (4U)
#define CAN_MB_CS_CODE_SHIFT          (24U)
#define CAN_MB_CS_DLC_SHIFT           (16U)
#define CAN_MB_CS_IDE_MASK            (1UL << 21U)
#define CAN_MB_CS_RTR_MASK            (1UL << 20U)
#define CAN_MB_CODE_RX_INACTIVE       (0x00UL)
#define CAN_MB_CODE_RX_EMPTY          (0x04UL)
#define CAN_MB_CODE_TX_INACTIVE       (0x08UL)
#define CAN_MB_CODE_TX_DATA           (0x0CUL)
#define CAN_MB_CS_CODE_MASK           (0x0FUL << CAN_MB_CS_CODE_SHIFT)
#define CAN_STANDARD_ID_SHIFT         (18U)
#define CAN_ESR1_FLTCONF_BUS_OFF      (2UL << CAN_ESR1_FLTCONF_SHIFT)
#define CAN_ESR1_CLEARABLE_ERROR_MASK (CAN_ESR1_ERRINT_MASK | \
                                       CAN_ESR1_BOFFINT_MASK)

typedef struct
{
    bool hardwarePending;
    Can_SwPduHandleType swPduHandle;
    bool terminalPending;
    Can_ReturnType terminalResult;
} Can_TxMbStateType;

static const Can_ConfigType *s_config;
static Can_CallbacksType s_callbacks;
static Can_ControllerModeType s_controllerMode = CAN_CONTROLLER_UNINIT;
static Can_ReturnType s_lastError = CAN_OK;
static bool s_busOff;
static bool s_inCallback;
static Can_TxMbStateType s_txState[CAN_SUPPORTED_MB_COUNT];
static Can_StatsType s_stats;

/** Store the most recent driver-level error for status diagnostics. */
static void Can_SetLastError(Can_ReturnType error)
{
    s_lastError = error;
}

/**
 * Poll a hardware register until all requested mask bits reach the expected
 * set/clear state, or until the bounded iteration count expires.
 */
static bool Can_WaitForMask(const volatile uint32_t *reg,
                            uint32_t mask,
                            bool waitForSet,
                            uint32_t timeoutCount)
{
    while (timeoutCount > 0U)
    {
        const bool isSet = ((*reg & mask) != 0U);

        if (isSet == waitForSet)
        {
            return true;
        }

        timeoutCount--;
    }

    return false;
}

/** Reset callbacks, bus state, callback guard, and all Tx mailbox tracking. */
static void Can_ClearSoftwareState(void)
{
    uint8_t mbIndex;

    s_callbacks.txConfirmation = NULL;
    s_callbacks.rxIndication = NULL;
    s_callbacks.controllerBusOff = NULL;
    s_busOff = false;
    s_inCallback = false;

    for (mbIndex = 0U; mbIndex < CAN_SUPPORTED_MB_COUNT; mbIndex++)
    {
        s_txState[mbIndex].hardwarePending = false;
        s_txState[mbIndex].swPduHandle = 0U;
        s_txState[mbIndex].terminalPending = false;
        s_txState[mbIndex].terminalResult = CAN_OK;
    }
}

/** Find a configured hardware object by local handle and Tx/Rx direction. */
static const Can_HohConfigType *Can_FindHoh(Can_HwHandleType hohId,
                                            Can_HohType type)
{
    uint8_t hohIndex;

    if (s_config == NULL)
    {
        return NULL;
    }

    for (hohIndex = 0U; hohIndex < s_config->hohCount; hohIndex++)
    {
        const Can_HohConfigType *hoh = &s_config->hohList[hohIndex];

        if ((hoh->hohId == hohId) && (hoh->type == type))
        {
            return hoh;
        }
    }

    return NULL;
}

/** Return whether the active configuration contains at least one HOH type. */
static bool Can_ConfigHasHohType(Can_HohType type)
{
    uint8_t hohIndex;

    for (hohIndex = 0U; hohIndex < s_config->hohCount; hohIndex++)
    {
        if (s_config->hohList[hohIndex].type == type)
        {
            return true;
        }
    }

    return false;
}

/** Return whether a Tx result is waiting to be dispatched to the upper layer. */
static bool Can_HasTerminalEvent(void)
{
    uint8_t mbIndex;

    for (mbIndex = 0U; mbIndex < CAN_SUPPORTED_MB_COUNT; mbIndex++)
    {
        if (s_txState[mbIndex].terminalPending)
        {
            return true;
        }
    }

    return false;
}

/**
 * Convert a hardware-pending Tx request into one terminal software event.
 * Duplicate terminal events are counted as stale and are not dispatched.
 */
static void Can_QueueTerminalEvent(uint8_t mbIndex, Can_ReturnType result)
{
    Can_TxMbStateType *state = &s_txState[mbIndex];

    if (state->terminalPending)
    {
        s_stats.staleEvent++;
        return;
    }

    state->hardwarePending = false;
    state->terminalPending = true;
    state->terminalResult = result;
}

/**
 * Finalize every outstanding Tx request during stop or fault handling.
 * A completion already latched by hardware keeps CAN_OK; other requests use
 * fallbackResult and may have their mailbox aborted when hardware is frozen.
 */
static void Can_FinalizePendingTx(Can_ReturnType fallbackResult,
                                  bool abortHardware)
{
    uint8_t hohIndex;

    for (hohIndex = 0U; hohIndex < s_config->hohCount; hohIndex++)
    {
        const Can_HohConfigType *hoh = &s_config->hohList[hohIndex];
        const uint32_t flagMask = 1UL << hoh->mbIndex;
        Can_TxMbStateType *state;

        if (hoh->type != CAN_HOH_TYPE_TX)
        {
            continue;
        }

        state = &s_txState[hoh->mbIndex];
        if (!state->hardwarePending)
        {
            continue;
        }

        if ((CAN0->IFLAG1 & flagMask) != 0U)
        {
            CAN0->IFLAG1 = flagMask;
            Can_QueueTerminalEvent(hoh->mbIndex, CAN_OK);
        }
        else
        {
            if (abortHardware)
            {
                CAN0->RAMn[hoh->mbIndex * CAN_MB_WORD_COUNT] =
                    CAN_MB_CODE_TX_INACTIVE << CAN_MB_CS_CODE_SHIFT;
            }
            Can_QueueTerminalEvent(hoh->mbIndex, fallbackResult);
        }
    }
}

/** Request FlexCAN freeze mode and fail with CAN_TIMEOUT if FRZACK is late. */
static Can_ReturnType Can_EnterFreezeMode(void)
{
    CAN0->MCR |= CAN_MCR_FRZ_MASK | CAN_MCR_HALT_MASK;

    if (!Can_WaitForMask(&CAN0->MCR,
                         CAN_MCR_FRZACK_MASK,
                         true,
                         s_config->hardwareTimeoutCount))
    {
        s_controllerMode = CAN_CONTROLLER_FAULT;
        s_stats.hardwareTimeoutCount++;
        Can_SetLastError(CAN_TIMEOUT);
        return CAN_TIMEOUT;
    }

    return CAN_OK;
}

/** Leave FlexCAN freeze mode and wait until the controller becomes ready. */
static Can_ReturnType Can_ExitFreezeMode(void)
{
    CAN0->MCR &= ~(CAN_MCR_HALT_MASK | CAN_MCR_FRZ_MASK);

    if (!Can_WaitForMask(&CAN0->MCR,
                         CAN_MCR_FRZACK_MASK,
                         false,
                         s_config->hardwareTimeoutCount) ||
        !Can_WaitForMask(&CAN0->MCR,
                         CAN_MCR_NOTRDY_MASK,
                         false,
                         s_config->hardwareTimeoutCount))
    {
        s_controllerMode = CAN_CONTROLLER_FAULT;
        s_stats.hardwareTimeoutCount++;
        Can_SetLastError(CAN_TIMEOUT);
        return CAN_TIMEOUT;
    }

    return CAN_OK;
}

/**
 * Validate and apply the CAN0 static configuration.
 *
 * The BSP must enable the FlexCAN0 peripheral clock first.  Successful init
 * configures bit timing and mailboxes, then leaves the controller STOPPED in
 * freeze mode so callbacks can be registered before communication starts.
 */
Can_ReturnType Can_Init(const Can_ConfigType *config)
{
    uint8_t mbIndex;
    uint8_t hohIndex;
    Can_ReturnType validationResult;

    if (s_inCallback)
    {
        return CAN_INVALID_STATE;
    }

    validationResult = Can_InternalValidateConfig(config);
    if (validationResult != CAN_OK)
    {
        s_stats.invalidInput++;
        Can_SetLastError(validationResult);
        return validationResult;
    }

    if (s_controllerMode != CAN_CONTROLLER_UNINIT)
    {
        Can_SetLastError(CAN_INVALID_STATE);
        return CAN_INVALID_STATE;
    }

    if ((PCC->PCCn[PCC_FlexCAN0_INDEX] & PCC_PCCn_CGC_MASK) == 0U)
    {
        Can_SetLastError(CAN_INVALID_STATE);
        return CAN_INVALID_STATE;
    }

    s_config = config;
    Can_ClearSoftwareState();
    Can_ResetStats();

    CAN0->MCR &= ~CAN_MCR_MDIS_MASK;
    CAN0->MCR |= CAN_MCR_FRZ_MASK | CAN_MCR_HALT_MASK;

    if (!Can_WaitForMask(&CAN0->MCR,
                         CAN_MCR_FRZACK_MASK,
                         true,
                         config->hardwareTimeoutCount))
    {
        s_controllerMode = CAN_CONTROLLER_FAULT;
        s_stats.hardwareTimeoutCount++;
        Can_SetLastError(CAN_TIMEOUT);
        return CAN_TIMEOUT;
    }

    CAN0->MCR &= ~(CAN_MCR_MAXMB_MASK | CAN_MCR_RFEN_MASK | CAN_MCR_FDEN_MASK);
    CAN0->MCR |= CAN_MCR_IRMQ_MASK |
                 CAN_MCR_MAXMB(CAN_SUPPORTED_MB_COUNT - 1U);

    CAN0->CTRL1 &= ~(CAN_CTRL1_PRESDIV_MASK |
                     CAN_CTRL1_PROPSEG_MASK |
                     CAN_CTRL1_PSEG1_MASK |
                     CAN_CTRL1_PSEG2_MASK |
                     CAN_CTRL1_RJW_MASK |
                     CAN_CTRL1_LPB_MASK |
                     CAN_CTRL1_CLKSRC_MASK);
    CAN0->CTRL1 |= CAN_CTRL1_PRESDIV(0U) |
                   CAN_CTRL1_PROPSEG(6U) |
                   CAN_CTRL1_PSEG1(5U) |
                   CAN_CTRL1_PSEG2(1U) |
                   CAN_CTRL1_RJW(0U);

    if (config->loopbackEnable)
    {
        CAN0->CTRL1 |= CAN_CTRL1_LPB_MASK;
    }

    CAN0->IMASK1 = 0U;
    CAN0->IFLAG1 = 0xFFFFFFFFUL;

    for (mbIndex = 0U; mbIndex < CAN_SUPPORTED_MB_COUNT; mbIndex++)
    {
        const uint32_t wordIndex = (uint32_t)mbIndex * CAN_MB_WORD_COUNT;

        CAN0->RAMn[wordIndex] =
            CAN_MB_CODE_RX_INACTIVE << CAN_MB_CS_CODE_SHIFT;
        CAN0->RAMn[wordIndex + 1U] = 0U;
        CAN0->RAMn[wordIndex + 2U] = 0U;
        CAN0->RAMn[wordIndex + 3U] = 0U;
        CAN0->RXIMR[mbIndex] = 0U;
    }

    for (hohIndex = 0U; hohIndex < config->hohCount; hohIndex++)
    {
        const Can_HohConfigType *hoh = &config->hohList[hohIndex];
        const uint32_t wordIndex = (uint32_t)hoh->mbIndex * CAN_MB_WORD_COUNT;

        if (hoh->type == CAN_HOH_TYPE_TX)
        {
            CAN0->RAMn[wordIndex] =
                CAN_MB_CODE_TX_INACTIVE << CAN_MB_CS_CODE_SHIFT;
        }
        else
        {
            CAN0->RAMn[wordIndex + 1U] =
                (hoh->rxCanId & CAN_STANDARD_ID_MAX) << CAN_STANDARD_ID_SHIFT;
            CAN0->RXIMR[hoh->mbIndex] =
                (hoh->rxIdMask & CAN_STANDARD_ID_MAX) << CAN_STANDARD_ID_SHIFT;
            CAN0->RAMn[wordIndex] =
                CAN_MB_CODE_RX_EMPTY << CAN_MB_CS_CODE_SHIFT;
        }
    }

    s_controllerMode = CAN_CONTROLLER_STOPPED;
    Can_SetLastError(CAN_OK);
    return CAN_OK;
}

/**
 * Register upper-layer Tx, Rx, and optional bus-off callbacks.
 * Registration is accepted only while the initialized controller is STOPPED
 * and no terminal Tx result is still waiting for delivery.
 */
Can_ReturnType Can_RegisterCallbacks(const Can_CallbacksType *callbacks)
{
    if ((s_config == NULL) ||
        (s_controllerMode == CAN_CONTROLLER_UNINIT))
    {
        return CAN_NOT_INITIALIZED;
    }

    if (s_inCallback || (s_controllerMode != CAN_CONTROLLER_STOPPED) ||
        Can_HasTerminalEvent())
    {
        return CAN_INVALID_STATE;
    }

    if ((callbacks == NULL) ||
        (Can_ConfigHasHohType(CAN_HOH_TYPE_TX) &&
         (callbacks->txConfirmation == NULL)) ||
        (Can_ConfigHasHohType(CAN_HOH_TYPE_RX) &&
         (callbacks->rxIndication == NULL)))
    {
        s_stats.invalidInput++;
        return CAN_INVALID_PARAM;
    }

    s_callbacks = *callbacks;
    return CAN_OK;
}

/**
 * Start or stop one configured controller using bounded freeze transitions.
 * Stopping converts every accepted unfinished Tx request into one terminal
 * confirmation before allowing a later restart.
 */
Can_ReturnType Can_SetControllerMode(uint8_t controllerId,
                                     Can_ControllerModeType mode)
{
    Can_ReturnType result;

    if ((s_config == NULL) ||
        (s_controllerMode == CAN_CONTROLLER_UNINIT))
    {
        return CAN_NOT_INITIALIZED;
    }

    if ((controllerId != s_config->controllerId) ||
        ((mode != CAN_CONTROLLER_STOPPED) &&
         (mode != CAN_CONTROLLER_STARTED)))
    {
        s_stats.invalidInput++;
        return CAN_INVALID_PARAM;
    }

    if (s_inCallback)
    {
        return CAN_INVALID_STATE;
    }

    if (mode == CAN_CONTROLLER_STARTED)
    {
        if (s_controllerMode == CAN_CONTROLLER_STARTED)
        {
            return CAN_OK;
        }

        if ((s_controllerMode != CAN_CONTROLLER_STOPPED) || s_busOff ||
            Can_HasTerminalEvent() ||
            (Can_ConfigHasHohType(CAN_HOH_TYPE_TX) &&
             (s_callbacks.txConfirmation == NULL)) ||
            (Can_ConfigHasHohType(CAN_HOH_TYPE_RX) &&
             (s_callbacks.rxIndication == NULL)))
        {
            return CAN_INVALID_STATE;
        }

        result = Can_ExitFreezeMode();
        if (result == CAN_OK)
        {
            s_controllerMode = CAN_CONTROLLER_STARTED;
            Can_SetLastError(CAN_OK);
        }

        return result;
    }

    if (s_controllerMode == CAN_CONTROLLER_STOPPED)
    {
        return CAN_OK;
    }

    result = Can_EnterFreezeMode();
    if (result != CAN_OK)
    {
        return result;
    }

    Can_FinalizePendingTx(s_busOff ? CAN_BUS_OFF : CAN_CANCELLED, true);
    s_controllerMode = CAN_CONTROLLER_STOPPED;
    s_busOff = false;
    Can_SetLastError(CAN_OK);
    return CAN_OK;
}

/**
 * Copy one Classical CAN frame into the mailbox mapped by the given HTH.
 *
 * The request is accepted only in STARTED state with a free mailbox.  Payload
 * bytes are copied before return and swPduHandle is retained for confirmation.
 */
Can_ReturnType Can_Write(Can_HwHandleType hth,
                         const Can_PduType *pduInfo)
{
    const Can_HohConfigType *hoh;
    Can_TxMbStateType *txState;
    uint32_t dataWord0;
    uint32_t dataWord1;
    uint32_t mbCode;
    uint32_t wordIndex;
    Can_ReturnType packResult;

    if (s_config == NULL)
    {
        return CAN_NOT_INITIALIZED;
    }

    if (s_inCallback || (s_controllerMode != CAN_CONTROLLER_STARTED))
    {
        return s_busOff ? CAN_BUS_OFF : CAN_INVALID_STATE;
    }

    if ((pduInfo == NULL) || (pduInfo->id > CAN_STANDARD_ID_MAX) ||
        (pduInfo->length > CAN_CLASSIC_MAX_DLC) ||
        ((pduInfo->length > 0U) && (pduInfo->sdu == NULL)))
    {
        s_stats.invalidInput++;
        return CAN_INVALID_PARAM;
    }

    hoh = Can_FindHoh(hth, CAN_HOH_TYPE_TX);
    if (hoh == NULL)
    {
        s_stats.invalidInput++;
        return CAN_INVALID_PARAM;
    }

    txState = &s_txState[hoh->mbIndex];
    wordIndex = (uint32_t)hoh->mbIndex * CAN_MB_WORD_COUNT;
    mbCode = (CAN0->RAMn[wordIndex] & CAN_MB_CS_CODE_MASK) >>
             CAN_MB_CS_CODE_SHIFT;

    if (txState->hardwarePending || txState->terminalPending ||
        (mbCode == CAN_MB_CODE_TX_DATA))
    {
        s_stats.txBusy++;
        return CAN_BUSY;
    }

    packResult = Can_InternalPackPayload(pduInfo->sdu,
                                         pduInfo->length,
                                         &dataWord0,
                                         &dataWord1);
    if (packResult != CAN_OK)
    {
        s_stats.invalidInput++;
        return packResult;
    }

    CAN0->IFLAG1 = 1UL << hoh->mbIndex;
    CAN0->RAMn[wordIndex] =
        CAN_MB_CODE_TX_INACTIVE << CAN_MB_CS_CODE_SHIFT;
    CAN0->RAMn[wordIndex + 1U] =
        (pduInfo->id & CAN_STANDARD_ID_MAX) << CAN_STANDARD_ID_SHIFT;
    CAN0->RAMn[wordIndex + 2U] = dataWord0;
    CAN0->RAMn[wordIndex + 3U] = dataWord1;

    txState->swPduHandle = pduInfo->swPduHandle;
    txState->hardwarePending = true;
    CAN0->RAMn[wordIndex] =
        (CAN_MB_CODE_TX_DATA << CAN_MB_CS_CODE_SHIFT) |
        ((uint32_t)pduInfo->length << CAN_MB_CS_DLC_SHIFT);

    s_stats.txAccepted++;
    return CAN_OK;
}

/**
 * Poll Tx completion flags and dispatch queued terminal confirmations.
 * Upper-layer callbacks execute from this main-context function exactly once
 * for each accepted Tx request.
 */
void Can_MainFunction_Write(void)
{
    uint8_t hohIndex;
    uint8_t mbIndex;

    if ((s_config == NULL) || s_inCallback)
    {
        return;
    }

    if (s_controllerMode == CAN_CONTROLLER_STARTED)
    {
        for (hohIndex = 0U; hohIndex < s_config->hohCount; hohIndex++)
        {
            const Can_HohConfigType *hoh = &s_config->hohList[hohIndex];
            const uint32_t flagMask = 1UL << hoh->mbIndex;
            Can_TxMbStateType *state;

            if ((hoh->type != CAN_HOH_TYPE_TX) ||
                ((CAN0->IFLAG1 & flagMask) == 0U))
            {
                continue;
            }

            CAN0->IFLAG1 = flagMask;
            state = &s_txState[hoh->mbIndex];
            if (!state->hardwarePending)
            {
                s_stats.staleEvent++;
                continue;
            }

            Can_QueueTerminalEvent(hoh->mbIndex, CAN_OK);
        }
    }

    for (mbIndex = 0U; mbIndex < CAN_SUPPORTED_MB_COUNT; mbIndex++)
    {
        Can_TxMbStateType *state = &s_txState[mbIndex];
        Can_ReturnType terminalResult;
        Can_SwPduHandleType swPduHandle;

        if (!state->terminalPending)
        {
            continue;
        }

        terminalResult = state->terminalResult;
        swPduHandle = state->swPduHandle;
        state->terminalPending = false;
        state->swPduHandle = 0U;

        if (terminalResult == CAN_OK)
        {
            s_stats.txCompleted++;
        }
        else
        {
            s_stats.txFailed++;
        }

        s_inCallback = true;
        s_callbacks.txConfirmation(swPduHandle, terminalResult);
        s_inCallback = false;
    }
}

/**
 * Poll configured Rx mailboxes, validate Classical CAN frames, unpack their
 * payload, and deliver each valid frame to the registered upper-layer callback.
 */
void Can_MainFunction_Read(void)
{
    uint8_t hohIndex;

    if ((s_config == NULL) ||
        (s_controllerMode != CAN_CONTROLLER_STARTED) ||
        s_inCallback)
    {
        return;
    }

    for (hohIndex = 0U; hohIndex < s_config->hohCount; hohIndex++)
    {
        const Can_HohConfigType *hoh = &s_config->hohList[hohIndex];
        const uint32_t flagMask = 1UL << hoh->mbIndex;
        uint8_t rxData[CAN_CLASSIC_MAX_DLC];
        uint32_t csWord;
        uint32_t wordIndex;
        uint8_t dlc;
        Can_HwType mailbox;
        Can_PduType pduInfo;

        if ((hoh->type != CAN_HOH_TYPE_RX) ||
            ((CAN0->IFLAG1 & flagMask) == 0U))
        {
            continue;
        }

        wordIndex = (uint32_t)hoh->mbIndex * CAN_MB_WORD_COUNT;
        csWord = CAN0->RAMn[wordIndex];
        dlc = (uint8_t)((csWord >> CAN_MB_CS_DLC_SHIFT) & 0x0FU);

        mailbox.canId =
            (CAN0->RAMn[wordIndex + 1U] >> CAN_STANDARD_ID_SHIFT) &
            CAN_STANDARD_ID_MAX;
        mailbox.hohId = hoh->hohId;
        mailbox.controllerId = hoh->controllerId;

        if ((dlc > CAN_CLASSIC_MAX_DLC) ||
            ((csWord & (CAN_MB_CS_IDE_MASK | CAN_MB_CS_RTR_MASK)) != 0U) ||
            (Can_InternalUnpackPayload(CAN0->RAMn[wordIndex + 2U],
                                       CAN0->RAMn[wordIndex + 3U],
                                       dlc,
                                       rxData) != CAN_OK))
        {
            (void)CAN0->TIMER;
            CAN0->IFLAG1 = flagMask;
            s_stats.rxDropped++;
            continue;
        }

        (void)CAN0->TIMER;
        CAN0->IFLAG1 = flagMask;

        pduInfo.id = mailbox.canId;
        pduInfo.swPduHandle = 0U;
        pduInfo.length = dlc;
        pduInfo.sdu = rxData;

        s_stats.rxDelivered++;
        s_inCallback = true;
        s_callbacks.rxIndication(&mailbox, &pduInfo);
        s_inCallback = false;
    }
}

/**
 * Poll FlexCAN error state, latch the first bus-off event, finalize pending Tx
 * requests, isolate the controller in FAULT, and notify the upper layer.
 */
void Can_MainFunction_Error(void)
{
    uint32_t errorStatus;
    bool isBusOff;
    const bool wasStarted = (s_controllerMode == CAN_CONTROLLER_STARTED);

    if ((s_config == NULL) || s_inCallback)
    {
        return;
    }

    errorStatus = CAN0->ESR1;
    isBusOff = ((errorStatus & CAN_ESR1_BOFFINT_MASK) != 0U) ||
               ((errorStatus & CAN_ESR1_FLTCONF_MASK) ==
                CAN_ESR1_FLTCONF_BUS_OFF);

    CAN0->ESR1 = errorStatus & CAN_ESR1_CLEARABLE_ERROR_MASK;

    if (!isBusOff || s_busOff)
    {
        return;
    }

    s_busOff = true;
    s_stats.busOffCount++;
    Can_SetLastError(CAN_BUS_OFF);

    if (wasStarted && (Can_EnterFreezeMode() == CAN_OK))
    {
        Can_FinalizePendingTx(CAN_BUS_OFF, true);
    }
    else
    {
        Can_FinalizePendingTx(CAN_BUS_OFF, false);
    }
    s_controllerMode = CAN_CONTROLLER_FAULT;

    if (s_callbacks.controllerBusOff != NULL)
    {
        s_inCallback = true;
        s_callbacks.controllerBusOff(s_config->controllerId);
        s_inCallback = false;
    }
}

/** Copy the current lifecycle, bus-off, error-counter, and last-error status. */
Can_ReturnType Can_GetControllerStatus(uint8_t controllerId,
                                       Can_ControllerStatusType *statusOut)
{
    uint32_t errorCounters;

    if ((statusOut == NULL) || (controllerId != 0U))
    {
        return CAN_INVALID_PARAM;
    }

    errorCounters = (s_controllerMode == CAN_CONTROLLER_UNINIT) ? 0U : CAN0->ECR;
    statusOut->mode = s_controllerMode;
    statusOut->busOff = s_busOff;
    statusOut->txErrorCounter =
        (uint8_t)((errorCounters & CAN_ECR_TXERRCNT_MASK) >>
                  CAN_ECR_TXERRCNT_SHIFT);
    statusOut->rxErrorCounter =
        (uint8_t)((errorCounters & CAN_ECR_RXERRCNT_MASK) >>
                  CAN_ECR_RXERRCNT_SHIFT);
    statusOut->lastError = s_lastError;
    return CAN_OK;
}

/** Copy the accumulated CAN diagnostic counters to caller-owned storage. */
Can_ReturnType Can_GetStats(Can_StatsType *statsOut)
{
    if (statsOut == NULL)
    {
        return CAN_INVALID_PARAM;
    }

    *statsOut = s_stats;
    return CAN_OK;
}

/** Reset all diagnostic counters without changing controller or mailbox state. */
void Can_ResetStats(void)
{
    s_stats.txAccepted = 0U;
    s_stats.txCompleted = 0U;
    s_stats.txFailed = 0U;
    s_stats.txBusy = 0U;
    s_stats.rxDelivered = 0U;
    s_stats.rxDropped = 0U;
    s_stats.invalidInput = 0U;
    s_stats.staleEvent = 0U;
    s_stats.busOffCount = 0U;
    s_stats.hardwareTimeoutCount = 0U;
}
