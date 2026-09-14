#ifndef COMM_MATRIX_CFG_H_
#define COMM_MATRIX_CFG_H_

#include "../comm/common/comm_types.h"

/* System-owned identity and Direct CAN Binding for VehicleStatus. */
#define COMM_MATRIX_GLOBAL_PDU_VEHICLE_STATUS      ((GlobalPduIdType)0x0010U)
#define COMM_MATRIX_CAN_ID_VEHICLE_STATUS          (0x321U)
#define COMM_MATRIX_VEHICLE_STATUS_LENGTH_BYTES    (8U)

/* Instructor timing profile: 1 ms COM tick and one 10 ms phase window. */
#define COMM_MATRIX_COM_MAIN_FUNCTION_PERIOD_MS    (1U)
#define COMM_MATRIX_TX_PHASE_WINDOW_TICKS          (10U)
#define COMM_MATRIX_TX_PHASE_OFFSET_MIN_TICKS      (1U)
#define COMM_MATRIX_TX_PHASE_OFFSET_MAX_TICKS      (9U)

#endif /* COMM_MATRIX_CFG_H_ */
