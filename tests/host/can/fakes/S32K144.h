#ifndef TEST_FAKE_S32K144_H_
#define TEST_FAKE_S32K144_H_

#include <stdint.h>

typedef struct
{
    volatile uint32_t MCR;
    volatile uint32_t CTRL1;
    volatile uint32_t TIMER;
    volatile uint32_t ECR;
    volatile uint32_t ESR1;
    volatile uint32_t IMASK1;
    volatile uint32_t IFLAG1;
    volatile uint32_t RAMn[64];
    volatile uint32_t RXIMR[16];
} CAN_Type;

typedef struct
{
    volatile uint32_t PCCn[128];
} PCC_Type;

extern CAN_Type g_fakeCan0;
extern PCC_Type g_fakePcc;

#define CAN0                                    (&g_fakeCan0)
#define PCC                                     (&g_fakePcc)
#define PCC_FlexCAN0_INDEX                      (36U)
#define PCC_PCCn_CGC_MASK                       (0x40000000UL)

#define CAN_MCR_MAXMB_MASK                      (0x7FUL)
#define CAN_MCR_MAXMB(x)                        ((uint32_t)(x) & 0x7FUL)
#define CAN_MCR_FDEN_MASK                       (0x800UL)
#define CAN_MCR_IRMQ_MASK                       (0x10000UL)
#define CAN_MCR_FRZACK_MASK                     (0x1000000UL)
#define CAN_MCR_NOTRDY_MASK                     (0x8000000UL)
#define CAN_MCR_HALT_MASK                       (0x10000000UL)
#define CAN_MCR_RFEN_MASK                       (0x20000000UL)
#define CAN_MCR_FRZ_MASK                        (0x40000000UL)
#define CAN_MCR_MDIS_MASK                       (0x80000000UL)

#define CAN_CTRL1_PROPSEG_MASK                  (0x7UL)
#define CAN_CTRL1_PROPSEG(x)                    ((uint32_t)(x) & 0x7UL)
#define CAN_CTRL1_LPB_MASK                      (0x1000UL)
#define CAN_CTRL1_CLKSRC_MASK                   (0x2000UL)
#define CAN_CTRL1_PSEG2_MASK                    (0x70000UL)
#define CAN_CTRL1_PSEG2(x)                      (((uint32_t)(x) << 16U) & CAN_CTRL1_PSEG2_MASK)
#define CAN_CTRL1_PSEG1_MASK                    (0x380000UL)
#define CAN_CTRL1_PSEG1(x)                      (((uint32_t)(x) << 19U) & CAN_CTRL1_PSEG1_MASK)
#define CAN_CTRL1_RJW_MASK                      (0xC00000UL)
#define CAN_CTRL1_RJW(x)                        (((uint32_t)(x) << 22U) & CAN_CTRL1_RJW_MASK)
#define CAN_CTRL1_PRESDIV_MASK                  (0xFF000000UL)
#define CAN_CTRL1_PRESDIV(x)                    (((uint32_t)(x) << 24U) & CAN_CTRL1_PRESDIV_MASK)

#define CAN_ECR_TXERRCNT_MASK                   (0xFFUL)
#define CAN_ECR_TXERRCNT_SHIFT                  (0U)
#define CAN_ECR_RXERRCNT_MASK                   (0xFF00UL)
#define CAN_ECR_RXERRCNT_SHIFT                  (8U)

#define CAN_ESR1_ERRINT_MASK                    (0x2UL)
#define CAN_ESR1_BOFFINT_MASK                   (0x4UL)
#define CAN_ESR1_FLTCONF_MASK                   (0x30UL)
#define CAN_ESR1_FLTCONF_SHIFT                  (4U)

#endif /* TEST_FAKE_S32K144_H_ */
