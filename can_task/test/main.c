#if 0

#include "S32K144.h"
#include "../../bsp/LED.h"
#include "../driver/inc/Can.h"
#include "../driver/inc/Can_Cfg.h"
#include "../upper/inc/CanUpper.h"
#include "../upper/inc/CanUpper_Cfg.h"

#define TEST_TC001_ID      1U
#define TEST_TC002_ID      2U
#define TEST_TC003_TX_ID   3U
#define TEST_TC003_RX_ID   4U
#define TEST_TC004_ID      5U

/* Select which test case to execute */
#define ACTIVE_TEST_CASE   TEST_TC003_TX_ID

/* Unlock and disable watchdog timer */
static void disable_WDOG(void)
{
    WDOG->CNT = 0xD928C520U;
    while ((WDOG->CS & WDOG_CS_ULK_MASK) == 0U)
    {
    }
    WDOG->TOVAL = 0x0000FFFFU;
    WDOG->CS = (WDOG->CS & ~WDOG_CS_EN_MASK) | WDOG_CS_UPDATE_MASK;
}

/* Initialize external 8 MHz oscillator, CAN pin routing, and board peripherals */
static void init_MCU(void)
{
    /* Disable SOSC prior to configuration */
    SCG->SOSCCSR &= ~SCG_SOSCCSR_SOSCEN_MASK;

    /* Set up external 8 MHz crystal oscillator */
    SCG->SOSCCFG = SCG_SOSCCFG_EREFS_MASK | SCG_SOSCCFG_HGO_MASK | SCG_SOSCCFG_RANGE(3);

    /* Output 8 MHz clock onto SOSCDIV2 */
    SCG->SOSCDIV = SCG_SOSCDIV_SOSCDIV1(1) | SCG_SOSCDIV_SOSCDIV2(1);

    /* Enable SOSC and wait for lock */
    SCG->SOSCCSR |= SCG_SOSCCSR_SOSCEN_MASK;
    while ((SCG->SOSCCSR & SCG_SOSCCSR_SOSCVLD_MASK) == 0U)
    {
    }

    /* Enable clock gates for PORTC, PORTE, and FlexCAN0 */
	PCC->PCCn[PCC_PORTC_INDEX]    |= PCC_PCCn_CGC_MASK;
	PCC->PCCn[PCC_PORTE_INDEX]    |= PCC_PCCn_CGC_MASK;
	PCC->PCCn[PCC_FlexCAN0_INDEX] |= PCC_PCCn_CGC_MASK;

    /* Mux PORTE[4] as CAN0_RX and PORTE[5] as CAN0_TX (ALT5) */
    PORTE->PCR[4] = PORT_PCR_MUX(5);
    PORTE->PCR[5] = PORT_PCR_MUX(5);

    PORTC->PCR[14] = PORT_PCR_MUX(1);
	PTC->PDDR |= (1UL << 14U);
	PTC->PCOR = (1UL << 14U);  /* Clear = 0 -> Normal Operation (Wake Up) */

	PORTE->PCR[11] = PORT_PCR_MUX(1);
	PTE->PDDR |= (1UL << 11U);
	PTE->PCOR = (1UL << 11U);

    /* Initialize Green LED using BSP driver */
    LED_Init(LED_GREEN);
}

/* Initialize Button SW2 (PTC12) on Board A */
static void init_Button(void)
{
    /* Enable clock for PORTC */
    PCC->PCCn[PCC_PORTC_INDEX] |= PCC_PCCn_CGC_MASK;

    /* PTC12 as GPIO (MUX=1) with Internal Pull-up enabled (PE=1, PS=1) */
    PORTC->PCR[12] = PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;

    /* Set direction as Input */
    PTC->PDDR &= ~(1UL << 12U);
}

/* Initialize all 3 RGB LEDs on Board B */
static void init_All_LEDs(void)
{
    LED_Init(LED_BLUE);
    LED_Init(LED_RED);
    LED_Init(LED_GREEN);

    /* Turn off all LEDs initially */
    LED_Off(LED_BLUE);
    LED_Off(LED_RED);
    LED_Off(LED_GREEN);
}

/* TC-001: Initialization and Parameter Check */
static uint8_t Test_TC001(void)
{
    uint32_t mb1_mask;
    uint8_t dummy[1] = {0xAAU};
    Can_PduType invalidPdu;

    /* Initialize CAN driver */
    Can_Init(&Can_Config);

    /* Verify controller successfully exited Freeze Mode */
    if ((CAN0->MCR & CAN_MCR_FRZACK_MASK) != 0U)
    {
        return 0U;
    }

    /* Verify MB0 is initialized to TX_INACTIVE (0x08) */
    if (((CAN0->RAMn[0] >> 24U) & 0x0FU) != 0x08U)
    {
        return 0U;
    }

    /* Verify MB1 is initialized to RX_EMPTY (0x04) */
    if (((CAN0->RAMn[4] >> 24U) & 0x0FU) != 0x04U)
    {
        return 0U;
    }

    /* Verify MB1 standard ID filter matches 0x123 */
    if (CAN0->RAMn[5] != (0x123UL << CAN_STANDARD_ID_SHIFT))
    {
        return 0U;
    }

    /* Safely verify MB1 individual mask in Freeze Mode */
    CAN0->MCR |= CAN_MCR_HALT(1) | CAN_MCR_FRZ(1);
    while ((CAN0->MCR & CAN_MCR_FRZACK_MASK) == 0U)
    {
    }

    mb1_mask = CAN0->RXIMR[1];

    CAN0->MCR &= ~(CAN_MCR_HALT_MASK | CAN_MCR_FRZ_MASK);
    while ((CAN0->MCR & CAN_MCR_FRZACK_MASK) != 0U)
    {
    }

    if (mb1_mask != (0x7FFUL << CAN_STANDARD_ID_SHIFT))
    {
        return 0U;
    }

    /* Verify rejection of NULL PDU pointer */
    if (Can_Write(CAN_HTH_0, (void *)0) != CAN_NOT_OK)
    {
        return 0U;
    }

    /* Verify rejection of standard ID exceeding 0x7FF */
    invalidPdu.id = 0x800U;
    invalidPdu.length = 1U;
    invalidPdu.sdu = dummy;
    if (Can_Write(CAN_HTH_0, &invalidPdu) != CAN_NOT_OK)
    {
        return 0U;
    }

    return 1U;
}

/* TC-002: Loopback Self-Test TX -> RX */
static uint8_t Test_TC002(void)
{
    uint8_t txData[4] = {0xDEU, 0xADU, 0xBEU, 0xEFU};
    uint8_t rxData[8] = {0U};
    uint8_t rxLen = 0U;
    uint32_t timeout = 1000000U;

    /* Initialize stack with Loopback configuration */
    CanUpper_Init(&Can_Config_Loopback);

    /* Verify initial state is IDLE */
    if (CanUpper_GetTxStatus(PDU_TX_LED_COMMAND) != PDU_TX_IDLE)
    {
        return 0U;
    }

    /* Steps 2 & 3: Transmit payload and verify return status */
    if (CanUpper_Transmit(PDU_TX_LED_COMMAND, txData, 4U) != CAN_OK)
    {
        return 0U;
    }

    /* Verify status transitions to PENDING immediately */
    if (CanUpper_GetTxStatus(PDU_TX_LED_COMMAND) != PDU_TX_PENDING)
    {
        return 0U;
    }

    /* Poll stack until transfer completes or timeout expires */
    while (timeout > 0U)
    {
        CanUpper_MainFunction();

        /*6: Check for PDU_TX_DONE */
        if (CanUpper_GetTxStatus(PDU_TX_LED_COMMAND) == PDU_TX_DONE)
        {
            break;
        }
        timeout--;
    }

    if (timeout == 0U)
    {
        return 0U; /* Timeout waiting for TX DONE */
    }

    /* Retrieve RX data and verify return code is CAN_OK */
    if (CanUpper_GetRxData(PDU_RX_LED_COMMAND, rxData, &rxLen) != CAN_OK)
    {
        return 0U;
    }

    /* Verify received length equals 4 */
    if (rxLen != 4U)
    {
        return 0U;
    }

    /* Verify payload matches transmitted data byte-for-byte */
    if ((rxData[0] != txData[0]) ||
        (rxData[1] != txData[1]) ||
        (rxData[2] != txData[2]) ||
        (rxData[3] != txData[3]))
    {
        return 0U;
    }

    return 1U; /* ALL 10 STEPS PASSED */
}

/* TC-003: Physical CAN Bus Communication - Board A (Transmitter) */
static uint8_t Test_TC003_TX(void)
{
    uint8_t txData[3];
    uint8_t press_count = 0U;
    uint8_t seq_num = 0U;
    uint32_t debounce;

    init_Button();
    CanUpper_Init(&Can_Config);

    while (1)
    {
        CanUpper_MainFunction();

        /* Detect button SW2 press on PTC12 (Active LOW) */
        if ((PTC->PDIR & (1UL << 12U)) == 0U)
        {
            /* Debounce delay */
            for (debounce = 0U; debounce < 300000U; debounce++)
            {
                __asm("nop");
            }

            /* Confirm button is still pressed */
            if ((PTC->PDIR & (1UL << 12U)) == 0U)
            {
                /* Increment press count: 1 -> 2 -> 3 -> 4 -> 1 ... */
                press_count++;
                if (press_count > 4U)
                {
                    press_count = 1U;
                }
                seq_num++;

                /* Construct packet payload per specification */
                txData[0] = 0xCAU;    /* Magic byte */
                txData[2] = seq_num;  /* Sequence number */

                switch (press_count)
                {
                    case 1U: txData[1] = 0x01U; break; /* Blue LED */
                    case 2U: txData[1] = 0x02U; break; /* Red LED */
                    case 3U: txData[1] = 0x03U; break; /* Green/Yellow LED */
                    case 4U: txData[1] = 0x00U; break; /* Turn off all */
                    default: txData[1] = 0x00U; break;
                }

                /* Transmit 3-byte frame */
                (void)CanUpper_Transmit(PDU_TX_LED_COMMAND, txData, 3U);

                /* Wait for transmission complete */
                while (CanUpper_GetTxStatus(PDU_TX_LED_COMMAND) != PDU_TX_DONE)
                {
                    CanUpper_MainFunction();
                }

                /* Wait for button release (PTC12 back to HIGH) */
                while ((PTC->PDIR & (1UL << 12U)) == 0U)
                {
                    CanUpper_MainFunction();
                }
            }
        }
    }

    return 1U;
}

/* TC-003: Physical CAN Bus Communication - Board B (Receiver) */
static uint8_t Test_TC003_RX(void)
{
    uint8_t rxBuf[8] = {0U};
    uint8_t rxLen = 0U;

    init_All_LEDs();
    CanUpper_Init(&Can_Config);

    while (1)
    {
        CanUpper_MainFunction();

        if (CanUpper_GetRxData(PDU_RX_LED_COMMAND, rxBuf, &rxLen) == CAN_OK)
        {
            /* Verify magic byte */
            if (rxBuf[0] == 0xCAU)
            {
                /* Always turn off all LEDs prior to setting new state */
                LED_Off(LED_BLUE);
                LED_Off(LED_RED);
                LED_Off(LED_GREEN);

                switch (rxBuf[1])
                {
                    case 0x01U:
                        LED_On(LED_BLUE);   /* Turn on Blue (PTD0) */
                        break;
                    case 0x02U:
                        LED_On(LED_RED);    /* Turn on Red (PTD15) */
                        break;
                    case 0x03U:
                        LED_On(LED_GREEN);  /* Turn on Green/Yellow (PTD16) */
                        break;
                    case 0x00U:
                    default:
                        /* All LEDs remain OFF */
                        break;
                }
            }
        }
    }

    return 1U;
}

/* TC-004: Robustness Invalid Parameters */
static uint8_t Test_TC004(void)
{
    uint8_t dummy[10] = {0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U, 0x08U, 0x09U, 0x0AU};
    Can_PduType pdu;
    uint32_t timeout;

    /* Initialize CAN stack using Loopback configuration */
    CanUpper_Init(&Can_Config_Loopback);

    /* Reject NULL PDU pointer */
    if (Can_Write(CAN_HTH_0, (void *)0) != CAN_NOT_OK)
    {
        return 0U;
    }

    /* Reject length > 8 */
    pdu.id = 0x123U;
    pdu.sdu = dummy;
    pdu.length = 9U;
    if (Can_Write(CAN_HTH_0, &pdu) != CAN_NOT_OK)
    {
        return 0U;
    }

    /* Reject standard ID exceeding 0x7FF */
    pdu.length = 8U;
    pdu.id = 0x800U;
    if (Can_Write(CAN_HTH_0, &pdu) != CAN_NOT_OK)
    {
        return 0U;
    }

    /* Reject non-existent HTH ID */
    pdu.id = 0x123U;
    if (Can_Write(99U, &pdu) != CAN_NOT_OK)
    {
        return 0U;
    }

    /* Can_Init(NULL) must not crash driver */
    Can_Init((void *)0);

    /* Reject non-existent TxPduId */
    if (CanUpper_Transmit(99U, dummy, 4U) != CAN_NOT_OK)
    {
        return 0U;
    }

    /* Reject NULL data pointer with length > 0 */
    if (CanUpper_Transmit(PDU_TX_LED_COMMAND, (void *)0, 4U) != CAN_NOT_OK)
    {
        return 0U;
    }

    /* Accept length = 0 */
    pdu.id = 0x123U;
    pdu.length = 0U;
    pdu.sdu = (void *)0;
    if (Can_Write(CAN_HTH_0, &pdu) != CAN_OK)
    {
        return 0U;
    }

    /* Wait for transmission completion before9 */
    timeout = 1000000U;
    while (((CAN0->IFLAG1 & (1UL << 0U)) == 0U) && (timeout > 0U))
    {
        timeout--;
    }
    CanUpper_MainFunction();

    /* Accept length = 8 with valid payload */
    pdu.id = 0x123U;
    pdu.length = 8U;
    pdu.sdu = dummy;
    if (Can_Write(CAN_HTH_0, &pdu) != CAN_OK)
    {
        return 0U;
    }

    /* Wait for completion of 8-byte frame transmission */
    timeout = 1000000U;
    while (((CAN0->IFLAG1 & (1UL << 0U)) == 0U) && (timeout > 0U))
    {
        timeout--;
    }
    CanUpper_MainFunction();

    return 1U;
}

int main(void)
{
    volatile uint8_t test_result = 0U;

    /* Disable watchdog timer to avoid resets during debug */
    disable_WDOG();

    /* Initialize system clocks and pin routing */
    init_MCU();

#if (ACTIVE_TEST_CASE == TEST_TC001_ID)
    test_result = Test_TC001();
#elif (ACTIVE_TEST_CASE == TEST_TC002_ID)
    test_result = Test_TC002();
#elif (ACTIVE_TEST_CASE == TEST_TC003_TX_ID)
    test_result = Test_TC003_TX();
#elif (ACTIVE_TEST_CASE == TEST_TC003_RX_ID)
    test_result = Test_TC003_RX();
#elif (ACTIVE_TEST_CASE == TEST_TC004_ID)
    test_result = Test_TC004();
#endif

    (void)test_result;

    /* Breakpoint target: inspect test_result == 1 */
    while (1);
}

#endif