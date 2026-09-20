#include "Cantp.h"
#include "Cantp_Cfg.h"

/*
 * @brief Keep the unfinished module disabled until validation and state setup exist.
 */
Std_ReturnType CanTp_Init(void)
{
    return E_NOT_OK;
}

/*
 * @brief Reject all requests until snapshot and segmentation logic is implemented.
 */
Std_ReturnType CanTp_Transmit(PduIdType TxNSduId,
                              const PduInfoType *PduInfoPtr)
{
    (void)TxNSduId;
    (void)PduInfoPtr;
    return E_NOT_OK;
}

/*
 * @brief Placeholder for future SF/FF/CF/FC decoding and Rx reassembly.
 */
void CanTp_RxIndication(PduIdType RxNPduId,
                        const PduInfoType *PduInfoPtr)
{
    (void)RxNPduId;
    (void)PduInfoPtr;
}

/*
 * @brief Placeholder for future Data and Flow Control confirmation dispatch.
 */
void CanTp_TxConfirmation(PduIdType TxNPduId)
{
    (void)TxNPduId;
}

/*
 * @brief Placeholder for the future 1 ms retry, timeout and STmin scheduler.
 */
void CanTp_MainFunction(void)
{
    /* Intentionally empty: transport scheduling is not implemented yet. */
}
