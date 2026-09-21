# Two-board COM LED application

`src/main.c` is the application entry point. It contains no legacy test
dispatch. At boot it runs one 62-byte CanTp internal-loopback self-test,
restores CAN0 normal mode, then starts the two-board application. Inspect
`g_CanTpLoopbackTestResult`; PASS is 2 and FAIL is 3. UART prints
`CANTP LOOPBACK PASS` after a successful self-test. Flash the same firmware
on both S32K144 boards. Both start in Rx with all LEDs off. SW2/PTC12 changes
the role between Rx and Tx. In Tx,
the red LED stays on and each SW3/PTC13 press updates the COM LED command
through 0 (off), 1 (green), 2 (blue), 3 (green and blue). In Rx, the board
waits for a valid COM indication and applies the received command to its LEDs.
Both switches are active-low with a 20 ms debounce.

The 1 ms application loop calls `Can_MainFunction_Write()`, then
`Can_MainFunction_Read()`, then `CanTp_MainFunction()`, and finally
`Com_MainFunctionTx()` while in Tx. COM sends
the configured I-PDU periodically (10 ms period, 1 ms initial offset), so a
SW3 press updates the next scheduled transmission; it does not send one frame
immediately. No COM Tx scheduling runs in Rx. An already accepted CAN request
may finish after switching to Rx.

The production stack uses CAN0 at 500 kbit/s, standard CAN ID `0x100`, DLC 8.
The application uses the existing COM Gear signal slot for the LED command.
With the current COM configuration the encoded command is in payload byte 2:
`(command << 1) | update_bit`. The Update Bit is set when the command changes
and cleared once the lower layer accepts that transmission. Bytes 0, 1 and
3 through 7 are zero when other VehicleStatus signals stay at their defaults.
The logical GlobalPduId `0x0010` is selected by the static PduR/CanIf route;
it is not a payload byte. A peer board must agree on this COM packing.

Connect the boards' CAN transceivers on CAN_H/CAN_L with common ground and
appropriate termination. The BSP uses CAN0 on PTE4/PTE5 and wakes its
transceiver. For each PC log, connect a 3.3 V USB-UART adapter's RX to PTC7
(LPUART1_TX) and its ground to board ground. Use 115200 baud, 8N1. UART prints
mode and logical LED commands; it does not print the raw CAN frame. Debugger
variables `g_AppStatus`, `g_AppModeTx`, `g_AppProcessedTicks`, and the app
counters show application state.

Host tests in this directory cover button behavior, COM Tx/Rx packing and
production CanIf ID. `tests/com_stack/test_main_scheduler.c` covers tick
ordering. These tests do not establish physical bus behavior or timing jitter.
