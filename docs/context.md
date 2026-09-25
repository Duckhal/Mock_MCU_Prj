# Project Context

Kiến trúc tổng thể và các luồng COM/CanTp được trình bày tại
`Architecture.md` ở thư mục gốc.

## UART image-size diagnosis (2026-09-24)

- `ascii_cat_512B_showcase.txt` is 512 raw bytes and
  `ascii_owl_2KB.txt` is 1984 raw bytes. Neither file contains the required
  uint16-LE image-length prefix; their first two ASCII bytes decode as 8224
  and 11822 instead of 512 and 1984.
- The Master UART ring is configured with capacity 1024 and the ring-buffer
  implementation reserves one slot, so usable capacity is 1023 bytes. A host
  reproduction that injects the raw cat then raw owl records 975 UART
  overflows and accepts only 1009 owl bytes because 14 cat bytes were still in
  the ring.
- Evidence is in `build/uart_overflow_diagnosis/repro.log`. Production source
  was not changed. Correct PC input must prepend `00 02` for the 512-byte cat
  or `C0 07` for the 1984-byte owl and pace blocks so CAN can drain the bounded
  UART queue.
- A 20 KiB Rx ring would hold the 16384-byte Mona Lisa plus its `00 40`
  uint16-LE header, and it fits the current FLASH linker layout. It would add
  19456 B to `.bss` and leave about 2344 B between the configured heap end and
  stack limit. A 17 KiB ring is sufficient for this fixture and leaves about
  5416 B. Either size is only a bounded workaround and still requires the
  two-byte header.

## Current application baseline (2026-09-24)

The active target is the three-ECU application in
`requirements/app+test/Mock_COMStack_App_Assignment_Draft_v0.7.md`.
`APP_BOARD_ROLE` in `app/app.h` fixes one role in each firmware image:

```c
#define APP_BOARD_ROLE APP_ROLE_MASTER
/* or APP_ROLE_SLAVE1 */
/* or APP_ROLE_SLAVE2 */
```

The application does not read SW2/SW3 and cannot change role at runtime.
`g_AppRole` is the debugger-visible role.

- Master samples ADC0_SE12, maps 0..4095 evenly to KeepAlive levels 0..6,
  blinks its blue LED with the selected level's visible period, increments
  `AliveCounter` at 500/200/100/50/20/10/5 ms, and monitors both Slave Status
  I-PDUs. UART accepts `[uint16 length LE][raw ASCII bytes]` and sends image
  chunks of at most 62 bytes through CanTp.
- Slave 1 receives KeepAlive, blinks the blue LED at the level's visible
  period, reports its status, receives CanTp image chunks, and writes exact
  image bytes to UART.
- Slave 2 receives KeepAlive, blinks the blue LED, and reports its status. Its
  CanTp Data N-PDU receive path is disabled, so it does not participate in
  image transfer.

## Active communication profile

| Purpose | Standard CAN ID | DLC | Producer |
| --- | ---: | ---: | --- |
| KeepAlive `[AliveCounter][RateLevel][00..]` | `0x100` | 8 | Master |
| Slave 1 Status `[status][00..]` | `0x201` | 8 | Slave 1 |
| Slave 2 Status `[status][00..]` | `0x202` | 8 | Slave 2 |
| CanTp Data | `0x650` | 8 | Master |
| CanTp Flow Control | `0x658` | 8 | Slave 1 |

COM transmits KeepAlive every 10 ms and each Slave status every 500 ms.
Application KeepAlive updates are independent of the fixed COM period. A Slave
refreshes `lastAliveTime` only when `AliveCounter` changes and reports
`MASTER_LOST` after 2000 ms without a new value. Master declares a Slave
offline after 2000 ms without its status frame.

CanTp implements Phase 1 through Phase 3 of `CanTp_Student_Guide.md`: exact
SF/FF/CF/FC format, retry, N_As/N_Ar/N_Bs/N_Cr, abort and late confirmation,
plus defensive length/SN/session/OVFLW handling. CanTp uses the CanIf L-PDU
macros directly; current Data/FC local handles are 3 and 4 after the three COM
routes.

## Integration order and ownership

`src/main.c` is a minimal entry point that calls `System_Init()` and repeatedly
calls `System_RunTask()`. `system/System.c` owns hardware/stack/application
initialization, system failure policy, SysTick setup, optional loopback
fixtures, and this 1 ms order:

```text
Can_MainFunction_Write
Can_MainFunction_Read
CanTp_MainFunction
App_MainFunction
Com_MainFunctionTx
```

`app/app.c` owns role behavior, ADC, LED, UART framing, COM Signal use, Slave
liveness and image chunk retry. `app/node_app.c` owns stable CanTp Tx storage
and the two-slot receive queue. COM owns Signal packing and periodic I-PDU
scheduling; PduR owns routes; CanIf owns CAN-ID mapping; CanDrv owns CAN0.

## Image-transfer policy

The UART receive ring is bounded and never overwrites unread bytes. Only one
image and one chunk are active at a time. A chunk buffer remains unchanged
until final CanTp confirmation. Every failed chunk gets one initial attempt
plus at most three retries; exhaustion drops that chunk and permits the next
chunk. No application ACK, CRC, image ID, sequence field, or receiver retry is
implemented because the assignment excludes them.

## Verification baseline

The following checks pass on 2026-09-24:

- `tests/board_demo/run_tests.ps1`: Master, Slave1 and Slave2 profiles; ADC
  boundaries; KeepAlive timing; AliveCounter filtering; matching Master/Slave
  LED timing; status; `62/62/6` image chunking; three retries; scheduler
  ordering; strict ARM compile.
- `tests/cantp/run_tests.ps1`: Phase 1-3 T01-T14 implemented cases, routing,
  and strict ARM compilation.
- `tests/canif/run_tests.ps1`: CanIf unit and production mapping tests.
- `tests/can_driver/run_tests.ps1`: CAN0 driver tests and ARM compilation.
- COM runtime/config tests: KeepAlive/status bytes, role gate, retry/drop, Rx
  counters, and 13 malformed configurations.
- `Debug_FLASH/Mock_MCU_Prj.elf` links successfully; the measured image is
  text 31012, data 1072, bss 7896.
- `tests/cantp/generate_evidence.ps1` regenerates `evidence/cantp_phase3/` and
  confirms older CanTp cases still pass.

Host fixtures and compiler results do not prove physical three-board timing,
transceiver wiring, termination, UART wiring, or ADC behavior. Those remain
board-level validation steps.
