#include "com_cfg.h"
#include "../comm_matrix_cfg.h"

#define COM_ARRAY_COUNT(array) ((uint16_t)(sizeof(array) / sizeof((array)[0])))

#if ((COM_VEHICLE_STATUS_PERIOD_TICKS % COMM_MATRIX_TX_PHASE_WINDOW_TICKS) != 0U)
#error "VehicleStatus period must be a multiple of the 10 ms phase window"
#endif

#if ((COM_VEHICLE_STATUS_OFFSET_TICKS < COMM_MATRIX_TX_PHASE_OFFSET_MIN_TICKS) || \
     (COM_VEHICLE_STATUS_OFFSET_TICKS > COMM_MATRIX_TX_PHASE_OFFSET_MAX_TICKS))
#error "VehicleStatus offset must be in the configured phase range"
#endif

/*
 * VehicleStatusGroup chứa ba Signal.
 * Đây là quan hệ Signal → Signal Group.
 */
static const Com_SignalIdType VehicleStatusSignalRefs[] =
{
    COM_SIGNAL_VEHICLE_SPEED,
    COM_SIGNAL_GEAR,
    COM_SIGNAL_ALIVE_COUNTER
};

/*
 * Cấu hình các Signal Slot trong VehicleStatusPdu.
 *
 * Bit 0 của mỗi slot được COM dùng làm Update Bit.
 * initialValue chỉ chứa payload logic, không chứa Update Bit.
 */
static const Com_SignalConfigType Com_SignalConfigs[] =
{
    {
        .signalId = COM_SIGNAL_VEHICLE_SPEED,
        .valueType = COM_UINT16,
        .slotStartBit = 0U,
        .slotLengthBits = 16U,
        .byteOrder = COM_BYTE_ORDER_LITTLE_ENDIAN,
        .initialValue = 0U
    },
    {
        .signalId = COM_SIGNAL_GEAR,
        .valueType = COM_UINT8,
        .slotStartBit = 16U,
        .slotLengthBits = 8U,
        .byteOrder = COM_BYTE_ORDER_LITTLE_ENDIAN,
        .initialValue = 0U
    },
    {
        .signalId = COM_SIGNAL_ALIVE_COUNTER,
        .valueType = COM_UINT8,
        .slotStartBit = 24U,
        .slotLengthBits = 8U,
        .byteOrder = COM_BYTE_ORDER_LITTLE_ENDIAN,
        .initialValue = 0U
    }
};

/*
 * Một Signal Group tham chiếu danh sách Signal của nó.
 */
static const Com_SignalGroupConfigType Com_SignalGroupConfigs[] =
{
    {
        .groupId = COM_GROUP_VEHICLE_STATUS,
        .signalRefs = VehicleStatusSignalRefs,
        .signalCount = COM_ARRAY_COUNT(VehicleStatusSignalRefs)
    }
};

/*
 * I-PDU là scheduling/transmission unit.
 *
 * CAN ID và HTH không thuộc cấu hình COM; chúng sẽ nằm trong CanIf.
 */
static const Com_IPduConfigType Com_IPduConfigs[] =
{
    {
        .ipduId = COM_IPDU_VEHICLE_STATUS,
        .globalPduId = COMM_MATRIX_GLOBAL_PDU_VEHICLE_STATUS,
        .direction = COM_IPDU_TX,
        .lengthBytes = 8U,
        .signalGroupRef = COM_GROUP_VEHICLE_STATUS,
        .periodTicks = COM_VEHICLE_STATUS_PERIOD_TICKS,
        .initialOffsetTicks = COM_VEHICLE_STATUS_OFFSET_TICKS,
        .maxRetries = COM_VEHICLE_STATUS_MAX_RETRIES
    }
};

/*
 * Root configuration được truyền vào Com_Init().
 *
 * Các array bên trên là static const và tồn tại trong toàn bộ
 * thời gian COM hoạt động.
 */
const Com_ConfigType Com_Config =
{
    .signals = Com_SignalConfigs,
    .signalCount = COM_ARRAY_COUNT(Com_SignalConfigs),

    .signalGroups = Com_SignalGroupConfigs,
    .signalGroupCount = COM_ARRAY_COUNT(Com_SignalGroupConfigs),

    .ipdus = Com_IPduConfigs,
    .ipduCount = COM_ARRAY_COUNT(Com_IPduConfigs)
};
