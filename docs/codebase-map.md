# Codebase Map

Tài liệu kiến trúc đầy đủ, gồm sơ đồ tầng, sequence và PDU mapping, nằm tại
`Architecture.md` ở thư mục gốc.

For handover diagrams, see `Architecture.md` sections 7.1-7.6 for COM
Signal/Group/I-PDU, Update Bit, retry and GlobalPduId traces, and sections
8.3-8.5 for the implemented CanTp states and 62-byte segmented sequence.

## Composition and scheduler

| Area | Main files | Responsibility |
| --- | --- | --- |
| Entrypoint | `src/main.c` | Calls `System_Init()` once and continuously calls `System_RunTask()` |
| System integration | `system/System.c`, `System.h`, `System_Cfg.h` | Ordered boot, stack init, SysTick, failure policy, test switches and 1 ms scheduler |
| Application policy | `app/app.c`, `app/app.h` | Compile-time Master/Slave1/Slave2 role, ADC, LED, UART, COM use, liveness and image chunks |
| CanTp application buffers | `app/node_app.c`, `app/node_app.h` | Stable Tx source, two-slot Rx queue and PduR callbacks |
| Future gateway | `app/gateway_app.c`, `app/gateway_app.h` | Reserved; no active behavior |

The scheduler flow is:

```text
Can Write -> Can Read -> CanTp -> App -> COM Tx
```

## Communication stack

| Layer | Main files | Active boundary |
| --- | --- | --- |
| COM | `drivers/can/com/Com.c`, `Com.h`, `Com_Cfg.c/h` | Eight Signals, six groups and six I-PDUs; runtime enables exactly one Tx I-PDU per role |
| PduR | `drivers/can/pdur/PduR.c`, `PduR_Cfg.c/h` | Three COM routes plus the CanTp application route |
| CanTp | `drivers/can/cantp/Cantp.c`, `Cantp_Cfg.c/h`, `Cantp_Types.h` | Phase 1-3 transport; Data/FC bind to CanIf macros so local handles cannot drift |
| CanIf | `drivers/can/canif/CanIf.c`, `CanIf_Cfg.c/h` | Five Tx/Rx L-PDUs and standard CAN-ID mapping |
| CanDrv | `drivers/can/can_driver/Can.c`, `Can_Cfg.c/h`, `Can_Types.h` | CAN0 polling, Tx MB8, Rx MB9, configuration validation and callbacks |
| Shared types/IDs | `drivers/can/common/CanStack_Types.h`, `CanStack_Cfg.h` | PDU types and GlobalPduId values |

Active mapping:

```text
Master KeepAlive COM Tx 0 -> PduR -> CanIf 0 -> CAN 0x100
Slave1 Status COM Tx 2   -> PduR -> CanIf 1 -> CAN 0x201
Slave2 Status COM Tx 4   -> PduR -> CanIf 2 -> CAN 0x202
CanTp Data N-PDU         -> CanIf 3          -> CAN 0x650
CanTp FC N-PDU           -> CanIf 4          -> CAN 0x658
```

## Board support and peripherals

| Area | Main files | Use |
| --- | --- | --- |
| CAN board setup | `bsp/can/board_can.c/h` | Clock, pins, watchdog and transceiver setup |
| LEDs | `bsp/LED.c/h` | Master and Slave blue-LED KeepAlive-rate indication |
| ADC | `drivers/adc/Driver_ADC.c/h` | Master potentiometer on ADC0_SE12 |
| UART | `drivers/uart/Driver_UART.c/h` | Master image input/status text and Slave1 image output |
| Byte queues | `middlewares/ring_buffer.c/h` | Interrupt-safe bounded UART Rx/Tx storage |
| Timebase | `drivers/systick/Driver_SysTick.c/h` | 1 ms scheduler tick |

## Application data flows

KeepAlive and status:

```text
Master ADC -> rate level -> Master LED + AliveCounter update -> COM 0x100
  -> both Slaves -> new-counter filter -> matching LED period + MASTER_LOST timer
  -> Slave status COM 0x201/0x202 -> Master online/offline monitor -> UART
```

Each occupied COM byte slot reserves bit 0 for its Update Bit. KeepAlive
`AliveCounter` and rate level are encoded as `(value << 1) | updateBit`;
`AliveCounter` remains `uint8` in the application and wraps after 127.

Image transfer:

```text
Master PC UART -> bounded ring -> <=62-byte stable chunk
  -> NodeApp -> PduR -> CanTp -> CanIf 3 -> CAN 0x650
  -> Slave1 CanIf 3 -> CanTp -> PduR -> NodeApp queue -> UART -> PC
  <- Slave1 flow control through CAN 0x658 / CanIf 4
```

Slave2 disables CanTp Data Rx at initialization and therefore does not reserve
NodeApp buffers or transmit Flow Control for image traffic.

## Tests and evidence

| Suite | Entry | Evidence |
| --- | --- | --- |
| Three-role application | `tests/board_demo/run_tests.ps1` | `build/uart_cantp_echo/verification.log` |
| CanTp Phase 1-3 | `tests/cantp/run_tests.ps1` | `build/cantp_phase3/verification.log` |
| CanTp submission package | `tests/cantp/generate_evidence.ps1` | `evidence/cantp_phase3/` |
| CanIf | `tests/canif/run_tests.ps1` | `build/canif/verification.log` |
| CanDrv | `tests/can_driver/run_tests.ps1` | `build/can_driver/verification.log` |
| COM | `tests/board_demo/test_tx.c`, `test_rx.c` | Included in the three-role runner; additional config fixtures are under `tests/com/` |

The S32DS `Debug_FLASH` target contains the production linked image. Physical
three-board CAN behavior still requires external-bus validation.
