#ifndef CAN_H_
#define CAN_H_

#include "Can_Types.h"
#include "Can_Cfg.h"

/*
 * @brief Initialize the temporary CAN0-only polling driver.
 * @pre Initialize the board's 8 MHz SOSC, CAN0 pins and transceiver first
 *      (disable_WDOG() and init_MCU() in the current BSP).
 * @details Fixed profile: 500 kbit/s, standard 11-bit Classical CAN data frames,
 *          DLC 0..8, Tx MB8 and BasicCAN Rx MB9; CAN1 is unsupported.
 *          Validates all controller and HOH configuration entries before HW access.
 *          HOH IDs share one unique Tx/Rx namespace; each references one controller.
 *          Only one instance-0 controller at 500 kbit/s and these two MBs are supported.
 * @return CAN_OK when ready; CAN_NOT_OK on failure or repeated initialization.
 * @note May be called again after a latched fault once its cause is corrected.
 *       This resets CAN0 and abandons the failed request without confirmation.
 */
Can_ReturnType Can_Init(void);

/*
 * @brief Accept one CAN0 transmission without waiting for physical completion.
 * @param Hth Configured Tx HOH ID; resolved with its controller, independent of index.
 * @param PduInfo Standard CAN ID, DLC 0..8, software PDU handle and payload.
 *        sdu may be NULL only for a zero-length frame.
 * @return CAN_OK if copied and accepted; CAN_BUSY if MB8 is still reserved;
 *         CAN_NOT_OK for invalid input, uninitialized state or hardware fault.
 * @note Input pointers are not retained. CAN_OK does not mean physical Tx done.
 *       CanIf must provide CanIf_TxConfirmation(PduIdType TxPduId).
 */
Can_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType *PduInfo);

/*
 * @brief Poll CAN0 Tx completion and confirm each accepted request once.
 * @details Frees MB8 before CanIf_TxConfirmation(swPduHandle). Hardware faults
 *          and aborts never produce success confirmations. Bus-off is latched;
 *          correct the cause and call Can_Init() to resume operation.
 * @note Call before COM Tx scheduling, preferably on each 1 ms tick.
 */
void Can_MainFunction_Write(void);

/*
 * @brief Poll one CAN0 frame and deliver the configured Rx HOH plus CAN ID to CanIf.
 * @details CanIf must supply CanIf_RxIndication(Can_HwHandleType Hrh,
 *          const Can_RxPduType *RxPdu). RxPdu and dataPtr are valid only during
 *          this synchronous callback; the receiver must copy retained data.
 *          Invalid formats/DLC are dropped; overrun delivers the latest frame.
 * @note Run Write/Read polling and Can_Write sequentially in main context.
 *       Do not service the same MBs from IRQs or concurrent tasks. Structured
 *       events are available in Can_LogRecords/Can_LogSequence in the debugger.
 *       The driver has no software Tx queue.
 */
void Can_MainFunction_Read(void);

#endif /* CAN_H_ */
