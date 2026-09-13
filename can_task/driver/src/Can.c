#include "../inc/Can.h"
#include "S32K144.h"
#include "../../../drivers/common/Driver_Common.h"
#include "../../upper/inc/CanUpper.h"

#define CAN_MB_CS_CODE_SHIFT          24U
#define CAN_MB_CODE_RX_INACTIVE       0x00U
#define CAN_MB_CODE_RX_EMPTY          0x04U
#define CAN_MB_CODE_TX_INACTIVE       0x08U

#define CAN_TOTAL_MB_COUNT            16U

#define CAN_MB_CS_DLC_SHIFT           16U
#define CAN_MB_CODE_TX_DATA           0x0CU
#define CAN_MB_CS_CODE_MASK           (0x0FUL << CAN_MB_CS_CODE_SHIFT)

#define CAN0_ORed_Message_buffer_IRQHandler CAN0_ORed_0_15_MB_IRQHandler

/* Pointer to hold active configuration */
static const Can_ConfigType *Can_CurrentConfigPtr = (void *)0;

void Can_Init(const Can_ConfigType *Config)
{
    uint8_t i;

    /* Validate input pointers */
    if ((Config == (void *)0) || (Config->hohList == (void *)0))
    {
        return;
    }
    Can_CurrentConfigPtr = Config;

    /* Enable FlexCAN0 module clock */
    PCC->PCCn[PCC_FlexCAN0_INDEX] |= PCC_PCCn_CGC_MASK;

    /* Request Freeze Mode */
    CAN0->MCR &= ~CAN_MCR_MDIS_MASK;
    CAN0->MCR |= CAN_MCR_FRZ(1) | CAN_MCR_HALT(1);

    /* Wait until Freeze Mode is acknowledged */
    while ((CAN0->MCR & CAN_MCR_FRZACK_MASK) == 0U)
    {
        /* Wait loop */
    }

    /* Select 8 MHz SOSCDIV2 clock source */
    CAN0->CTRL1 &= ~CAN_CTRL1_CLKSRC_MASK;

    /* Clear prior bit timing settings */
    CAN0->CTRL1 &= ~(CAN_CTRL1_PRESDIV_MASK |
                     CAN_CTRL1_PROPSEG_MASK |
                     CAN_CTRL1_PSEG1_MASK   |
                     CAN_CTRL1_PSEG2_MASK   |
                     CAN_CTRL1_RJW_MASK     |
                     CAN_CTRL1_LPB_MASK);

    /* Configure bit timing for 500 kbps (16 TQ/bit) */
    CAN0->CTRL1 |= (CAN_CTRL1_PRESDIV(0U) |
                    CAN_CTRL1_PROPSEG(6U) |
                    CAN_CTRL1_PSEG1(5U)   |
                    CAN_CTRL1_PSEG2(1U)   |
                    CAN_CTRL1_RJW(0U));

    /* Enable internal loopback mode if configured */
    if (Config->loopbackEnable == 1U)
    {
        CAN0->CTRL1 |= CAN_CTRL1_LPB_MASK;
    }

    /* Enable individual masking and restrict to 16 MBs */
    CAN0->MCR |= (CAN_MCR_IRMQ_MASK | CAN_MCR_MAXMB(CAN_TOTAL_MB_COUNT - 1U));

    /* Disable all 16 MBs */
    for (i = 0U; i < CAN_TOTAL_MB_COUNT; i++)
    {
        CAN0->RAMn[i * 4U] = (CAN_MB_CODE_RX_INACTIVE << CAN_MB_CS_CODE_SHIFT);
    }

    /* Configure MBs based on HOH entries */
    for (i = 0U; i < Config->hohCount; i++)
    {
        const Can_HohConfigType *hoh = &Config->hohList[i];
        uint8_t mb = hoh->mbIndex;

        #if defined(CAN_USE_INTERRUPT)
            /* Enable MB interrupt mask in IMASK1 */
            CAN0->IMASK1 |= (1UL << mb);
        #endif

        if (mb < CAN_TOTAL_MB_COUNT)
        {
            if (hoh->type == CAN_HOH_TYPE_TX)
            {
                /* Initialize TX MB as inactive */
                CAN0->RAMn[mb * 4U] = (CAN_MB_CODE_TX_INACTIVE << CAN_MB_CS_CODE_SHIFT);
            }
            else if (hoh->type == CAN_HOH_TYPE_RX)
            {
                /* Set standard RX ID filter */
                CAN0->RAMn[mb * 4U + 1U] = ((hoh->rxCanId & CAN_STANDARD_ID_MAX) << CAN_STANDARD_ID_SHIFT);

                /* Set individual RX mask */
                CAN0->RXIMR[mb] = ((hoh->rxIdMask & CAN_STANDARD_ID_MAX) << CAN_STANDARD_ID_SHIFT);

                /* Set RX MB ready to receive */
                CAN0->RAMn[mb * 4U] = (CAN_MB_CODE_RX_EMPTY << CAN_MB_CS_CODE_SHIFT);
            }
            else
            {
                /* Reserved for unhandled HOH types */
            }
        }
    }

    /* Exit Freeze Mode */
    CAN0->MCR &= ~(CAN_MCR_HALT_MASK | CAN_MCR_FRZ_MASK);

    /* Wait until Freeze Mode is exited */
    while ((CAN0->MCR & CAN_MCR_FRZACK_MASK) != 0U) 
    {
        /* Wait loop */
    }

    /* Wait until module is ready for bus operations */
    while ((CAN0->MCR & CAN_MCR_NOTRDY_MASK) != 0U)
    {
        /* Wait loop */
    }
}

Can_ReturnType Can_Write(uint8_t HthId, const Can_PduType *PduInfo)
{
    uint8_t i;
    uint8_t mbIndex;
    uint32_t mbCode;
    uint32_t dataWord0 = 0U;
    uint32_t dataWord1 = 0U;
    const Can_HohConfigType *hoh = (void *)0;

    /* Check if driver has been initialized */
    if (Can_CurrentConfigPtr == (void *)0)
    {
        return CAN_NOT_OK;
    }

    /* Find matching TX HOH */
    for (i = 0U; i < Can_CurrentConfigPtr->hohCount; i++)
    {
        if ((Can_CurrentConfigPtr->hohList[i].hohId == HthId) &&
            (Can_CurrentConfigPtr->hohList[i].type == CAN_HOH_TYPE_TX))
        {
            hoh = &Can_CurrentConfigPtr->hohList[i];
            break;
        }
    }

    /* Validate HTH ID */
    if (hoh == (void *)0)
    {
        return CAN_NOT_OK;
    }

    /* Validate PDU pointer */
    if (PduInfo == (void *)0)
    {
        return CAN_NOT_OK;
    }

    /* Validate CAN ID */
    if (PduInfo->id > CAN_STANDARD_ID_MAX)
    {
        return CAN_NOT_OK;
    }

    /* Validate length and data pointer */
    if (PduInfo->length > 8U)
    {
        return CAN_NOT_OK;
    }
    if ((PduInfo->length > 0U) && (PduInfo->sdu == (void *)0))
    {
        return CAN_NOT_OK;
    }

    mbIndex = hoh->mbIndex;

    /* Check if MB is busy */
    mbCode = (CAN0->RAMn[mbIndex * 4U] & CAN_MB_CS_CODE_MASK) >> CAN_MB_CS_CODE_SHIFT;
    if (mbCode == CAN_MB_CODE_TX_DATA)
    {
        return CAN_BUSY;
    }

    /* Hold MB by setting inactive */
    CAN0->RAMn[mbIndex * 4U] = (CAN_MB_CODE_TX_INACTIVE << CAN_MB_CS_CODE_SHIFT);

    /* Write 11-bit standard ID */
    CAN0->RAMn[mbIndex * 4U + 1U] = ((PduInfo->id & CAN_STANDARD_ID_MAX) << CAN_STANDARD_ID_SHIFT);

    /* Pack payload into big-endian words */
    for (i = 0U; i < PduInfo->length; i++)
    {
        if (i < 4U)
        {
            dataWord0 |= ((uint32_t)PduInfo->sdu[i] << ((3U - i) * 8U));
        }
        else
        {
            dataWord1 |= ((uint32_t)PduInfo->sdu[i] << ((7U - i) * 8U));
        }
    }

    CAN0->RAMn[mbIndex * 4U + 2U] = dataWord0;
    CAN0->RAMn[mbIndex * 4U + 3U] = dataWord1;

    /* Trigger TX with length and DATA code */
    CAN0->RAMn[mbIndex * 4U] = (CAN_MB_CODE_TX_DATA << CAN_MB_CS_CODE_SHIFT) |
                              ((uint32_t)PduInfo->length << CAN_MB_CS_DLC_SHIFT);

    return CAN_OK;
}

void Can_MainFunction_Write(void)
{
    if (Can_CurrentConfigPtr == (void*)0)
    {
        return;
    }

    uint8_t i;
    uint8_t mbIndex;

    for (i = 0U; i < Can_CurrentConfigPtr->hohCount; i++)
    {
        const Can_HohConfigType *hoh = &Can_CurrentConfigPtr->hohList[i];

        /* Check only transmit handles */
        if (hoh->type == CAN_HOH_TYPE_TX)
        {
            mbIndex = hoh->mbIndex;

            /* Check if TX completed flag is asserted */
            if ((CAN0->IFLAG1 & (1UL << mbIndex)) != 0U)
            {
                /* Clear interrupt flag using write-1-to-clear */
                CAN0->IFLAG1 = (1UL << mbIndex);

                /* Notify upper layer that transmission completed */
                CanUpper_TxConfirmation(hoh->hohId);
            }
        }
    }   
}

void Can_MainFunction_Read(void)
{
    uint8_t i;
    uint8_t byteIdx;
    uint8_t mbIndex;
    uint8_t dlc;
    uint8_t rxData[8];
    uint32_t csWord;
    uint32_t idWord;
    uint32_t dataWord0;
    uint32_t dataWord1;
    Can_HwType hwInfo;
    Can_PduType pduInfo;

    /* Verify driver initialization state */
    if (Can_CurrentConfigPtr == (void *)0)
    {
        return;
    }

    /* Iterate through configured HOHs */
    for (i = 0U; i < Can_CurrentConfigPtr->hohCount; i++)
    {
        const Can_HohConfigType *hoh = &Can_CurrentConfigPtr->hohList[i];

        /* Check only receive handles */
        if (hoh->type == CAN_HOH_TYPE_RX)
        {
            mbIndex = hoh->mbIndex;

            /* Check if new frame is received */
            if ((CAN0->IFLAG1 & (1UL << mbIndex)) != 0U)
            {
                /* Read CS word to extract DLC and lock MB */
                csWord = CAN0->RAMn[mbIndex * 4U];
                dlc = (uint8_t)((csWord >> CAN_MB_CS_DLC_SHIFT) & 0x0FU);
                if (dlc > 8U)
                {
                    dlc = 8U;
                }

                /* Read ID word and extract standard CAN ID */
                idWord = CAN0->RAMn[mbIndex * 4U + 1U];
                hwInfo.canId = (idWord >> CAN_STANDARD_ID_SHIFT) & CAN_STANDARD_ID_MAX;
                hwInfo.hohId = hoh->hohId;

                /* Read payload data words */
                dataWord0 = CAN0->RAMn[mbIndex * 4U + 2U];
                dataWord1 = CAN0->RAMn[mbIndex * 4U + 3U];

                /* Unpack big-endian payload bytes */
                for (byteIdx = 0U; byteIdx < dlc; byteIdx++)
                {
                    if (byteIdx < 4U)
                    {
                        rxData[byteIdx] = (uint8_t)((dataWord0 >> ((3U - byteIdx) * 8U)) & 0xFFU);
                    }
                    else
                    {
                        rxData[byteIdx] = (uint8_t)((dataWord1 >> ((7U - byteIdx) * 8U)) & 0xFFU);
                    }
                }

                /* Read TIMER register to unlock MB */
                (void)CAN0->TIMER;

                /* Clear interrupt flag using write-1-to-clear */
                CAN0->IFLAG1 = (1UL << mbIndex);

                /* Populate PDU structure */
                pduInfo.id = hwInfo.canId;
                pduInfo.length = dlc;
                pduInfo.sdu = rxData;

                /* Forward frame to upper layer */
                CanUpper_RxIndication(&hwInfo, &pduInfo);
            }
        }
    }
}

void CAN0_ORed_0_15_MB_IRQHandler(void)
{
    uint8_t i;
    uint8_t byteIdx;
    uint8_t mbIndex;
    uint8_t dlc;
    uint8_t rxData[8];
    uint32_t csWord;
    uint32_t idWord;
    uint32_t dataWord0;
    uint32_t dataWord1;
    Can_HwType hwInfo;
    Can_PduType pduInfo;

    /* Verify driver initialization state */
    if (Can_CurrentConfigPtr == (void *)0)
    {
        return;
    }

    /* Process TX and RX events across configured HOHs */
    for (i = 0U; i < Can_CurrentConfigPtr->hohCount; i++)
    {
        const Can_HohConfigType *hoh = &Can_CurrentConfigPtr->hohList[i];
        mbIndex = hoh->mbIndex;

        /* Check if MB interrupt flag is set */
        if ((CAN0->IFLAG1 & (1UL << mbIndex)) != 0U)
        {
            if (hoh->type == CAN_HOH_TYPE_TX)
            {
                /* Clear TX flag before callback to prevent re-trigger */
                CAN0->IFLAG1 = (1UL << mbIndex);

                /* Notify upper layer of TX completion */
                CanUpper_TxConfirmation(hoh->hohId);
            }
            else if (hoh->type == CAN_HOH_TYPE_RX)
            {
                /* Read CS word to extract DLC and lock MB */
                csWord = CAN0->RAMn[mbIndex * 4U];
                dlc = (uint8_t)((csWord >> CAN_MB_CS_DLC_SHIFT) & 0x0FU);
                if (dlc > 8U)
                {
                    dlc = 8U;
                }

                /* Read ID word and extract standard CAN ID */
                idWord = CAN0->RAMn[mbIndex * 4U + 1U];
                hwInfo.canId = (idWord >> CAN_STANDARD_ID_SHIFT) & CAN_STANDARD_ID_MAX;
                hwInfo.hohId = hoh->hohId;

                /* Read payload data words */
                dataWord0 = CAN0->RAMn[mbIndex * 4U + 2U];
                dataWord1 = CAN0->RAMn[mbIndex * 4U + 3U];

                /* Unpack big-endian payload bytes */
                for (byteIdx = 0U; byteIdx < dlc; byteIdx++)
                {
                    if (byteIdx < 4U)
                    {
                        rxData[byteIdx] = (uint8_t)((dataWord0 >> ((3U - byteIdx) * 8U)) & 0xFFU);
                    }
                    else
                    {
                        rxData[byteIdx] = (uint8_t)((dataWord1 >> ((7U - byteIdx) * 8U)) & 0xFFU);
                    }
                }

                /* Read TIMER register to unlock MB */
                (void)CAN0->TIMER;

                /* Clear RX flag before callback to prevent re-trigger */
                CAN0->IFLAG1 = (1UL << mbIndex);

                /* Populate PDU structure */
                pduInfo.id = hwInfo.canId;
                pduInfo.length = dlc;
                pduInfo.sdu = rxData;

                /* Forward received frame to upper layer */
                CanUpper_RxIndication(&hwInfo, &pduInfo);
            }
            else
            {
                /* Reserved for unhandled HOH types */
            }
        }
    }
}
