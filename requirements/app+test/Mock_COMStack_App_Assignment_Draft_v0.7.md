# Mock COM Stack – Application Assignment
## Draft v0.7

> **Target:** 3 ECUs — 1 Master + 2 Slaves  
> **Main features:** KeepAlive + ADC, Slave Status, ASCII Image Transfer  
> **Goal:** Keep the Application Layer simple and use it mainly to demonstrate the Mock COM Stack.

---

# 1. System Overview

```text
                         +----------------------+
                         |          PC          |
                         |  ASCII Image Sender  |
                         +----------+-----------+
                                    |
                                   UART
                                    |
                         +----------v-----------+
                         |      MASTER ECU      |
                         |                      |
                         | ADC -> KeepAlive     |
                         | Slave Monitor        |
                         | UART Rx Queue        |
                         | Image Sender         |
                         +----------+-----------+
                                    |
                                   CAN
                    +---------------+---------------+
                    |                               |
          +---------v----------+          +---------v----------+
          |      SLAVE 1       |          |      SLAVE 2       |
          |                    |          |                    |
          | KeepAlive Rx       |          | KeepAlive Rx       |
          | LED Indicator      |          | LED Indicator      |
          | Slave Status Tx    |          | Slave Status Tx    |
          | Image Rx -> UART   |          |                    |
          +--------------------+          +--------------------+
```

The assignment contains only three Application functions:

1. **KeepAlive + ADC**
2. **Slave Status**
3. **ASCII Image Transfer**

---

# 2. KeepAlive Mechanism

## 2.1 Purpose

The KeepAlive demo is used to show the interaction between:

- the **Application update rate**, and
- the **fixed COM transmission rate**.

The potentiometer does **not** change the COM Tx period.

Instead, ADC changes how often the **Application updates the KeepAlive signal**.

```text
Application Scheduler                 COM Scheduler

ADC                                   Fixed COM Tx Period
 |                                             |
 v                                             v
KeepAlive Rate                          Pack current signals
 |                                             |
 v                                             v
AliveCounter++ -----------------> COM Signal Buffer ---> CAN
```

---

## 2.2 KeepAlive COM Signals

The Master KeepAlive I-PDU contains two signals:

| Signal | Type | Purpose |
|---|---|---|
| `AliveCounter` | `uint8` | Incremented on every Application KeepAlive event |
| `KeepAliveRateLevel` | `uint8` | ADC-selected KeepAlive rate level |

Suggested packing:

```text
Byte 0 : AliveCounter
Byte 1 : KeepAliveRateLevel
```

Suggested KeepAlive CAN ID:

```text
0x100
```

Both Slaves receive the same KeepAlive I-PDU.

---

## 2.3 Fixed COM Period

The COM Tx period remains fixed.

Recommended baseline:

```text
COM KeepAlive Tx Period = 10 ms
```

`Com_SendSignal()` only updates the COM signal buffer.  
COM transmits the latest signal value according to its own periodic scheduler.

---

## 2.4 ADC Controls Application KeepAlive Rate

ADC is converted to a small number of discrete levels.

Recommended table:

| Rate Level | App KeepAlive Period |
|---:|---:|
| 0 | 500 ms |
| 1 | 200 ms |
| 2 | 100 ms |
| 3 | 50 ms |
| 4 | 20 ms |
| 5 | 10 ms |
| 6 | 5 ms |

Example:

```text
ADC
 |
 v
RateLevel = 4
 |
 +--> KeepAliveRateLevel = 4
 |
 +--> Application KeepAlive Period = 20 ms
```

A periodic Application task may use a simple software countdown.

```text
App task period = 1 ms
Selected KeepAlive period = 20 ms

countdown = 20
```

When countdown expires:

```text
AliveCounter++
Com_SendSignal(ALIVE_COUNTER)
Reload countdown
```

No additional hardware timer is required.

---

## 2.5 Important Behavior: App Rate vs COM Rate

Assume:

```text
COM Tx Period = 10 ms
```

### Case A — Application slower than COM

```text
App KeepAlive Period = 50 ms
```

COM may transmit the same counter value several times:

```text
CAN:
0 0 0 0 0 1 1 1 1 1 2 ...
```

### Case B — Application and COM have similar rate

```text
App KeepAlive Period = 10 ms
```

Approximately one new counter value is available for each COM transmission:

```text
CAN:
1 2 3 4 5 6 ...
```

### Case C — Application faster than COM

```text
App KeepAlive Period = 5 ms
COM Tx Period         = 10 ms
```

Slave may observe:

```text
2 -> 4 -> 6 -> 8 ...
```

Intermediate counter values are overwritten in the COM signal buffer.

**This is expected behavior and is part of the demo.**

---

# 3. Slave KeepAlive Reception and LED

KeepAlive reception is event-driven.

```text
COM RxIndication
      |
      v
App KeepAlive Rx Callback
      |
      +--> Com_ReceiveSignal(AliveCounter)
      |
      +--> Com_ReceiveSignal(KeepAliveRateLevel)
```

The Slave stores the latest values.

A KeepAlive is considered **new** when:

```text
newAliveCounter != lastAliveCounter
```

When a new counter is detected:

```text
lastAliveCounter = newAliveCounter
lastAliveTime    = currentTime
```

If the counter does not change for the configured timeout, the Slave reports:

```text
MASTER_LOST
```

When a new counter is received again:

```text
NORMAL
```

---

## 3.1 LED Visualization

The real KeepAlive rate can become too fast for a human to observe directly.

Therefore the LED shall **not** blink directly at the Application KeepAlive period.

Instead, `KeepAliveRateLevel` is mapped to a slower human-visible LED period.

| Rate Level | App KeepAlive | LED Full Cycle |
|---:|---:|---:|
| 0 | 500 ms | 1500 ms |
| 1 | 200 ms | 1000 ms |
| 2 | 100 ms | 800 ms |
| 3 | 50 ms | 600 ms |
| 4 | 20 ms | 400 ms |
| 5 | 10 ms | 300 ms |
| 6 | 5 ms | 200 ms |

The LED is only a **visual indicator of the selected rate level**.

---

# 4. Slave Status

Each Slave periodically sends one simple COM signal to Master.

Suggested I-PDUs:

```text
Slave 1 Status CAN ID = 0x201
Slave 2 Status CAN ID = 0x202
```

Signal:

| Signal | Type | Value |
|---|---|---|
| `SlaveStatus` | `uint8` | `0 = NORMAL`, `1 = MASTER_LOST` |

Suggested COM Tx period:

```text
500 ms
```

Master receives each Slave Status I-PDU through a COM Rx callback.

```text
Slave 1 / Slave 2
      |
      v
COM Status Tx
      |
      v
CAN Bus
      |
      v
Master COM Rx Callback
      |
      +--> update lastSeenTick
      +--> set online = TRUE
```

A periodic Master task performs the timeout check.

```text
Network Monitor Task = 10 ms
Slave Offline Timeout = 2000 ms
```

If a Slave Status I-PDU is no longer received:

```text
Slave Online = FALSE
```

Master shall be able to show:

```text
Online Slaves: 0/2
Online Slaves: 1/2
Online Slaves: 2/2
```

Example UART output:

```text
[MASTER] Slave 1 ONLINE
[MASTER] Slave 2 ONLINE
[MASTER] Online Slaves: 2/2

[MASTER] Slave 1 OFFLINE
[MASTER] Online Slaves: 1/2
```

---

# 5. ASCII Image Transfer

The image transfer is intentionally simple and **best effort**.

```text
PC
 |
UART
 |
v
Master UART Rx Queue
 |
| pop data
v
Master App Tx Buffer
 |
CanTp
 |
CAN
 |
CanTp
 |
v
Slave 1 Image Receiver
 |
UART
 |
v
PC Terminal
```

Slave 2 does not participate in image transfer.

---

## 5.1 PC Input

Only one image is sent at a time.

Recommended UART input format:

```text
+--------------------------+
| ImageLength : uint16 LE  |
+--------------------------+
| Raw ASCII image bytes    |
| ...                      |
+--------------------------+
```

PC shall use a configurable delay between UART blocks.

The purpose of the delay is to prevent the PC from continuously filling the Master queue faster than the ECU can consume it.

---

## 5.2 Master UART Rx Queue

UART reception pushes incoming image bytes into a bounded queue.

```text
UART Rx
   |
   v
+-----------------------+
|     UART Rx Queue     |
| [ ][ ][ ][ ][ ] ...   |
+-----------+-----------+
            |
            | pop
            v
      App Tx Buffer
```

The queue shall not overwrite unread data.

---

## 5.3 Master Image Sender

Current Mock CanTp limitation:

```text
Maximum N-SDU = 62 bytes
```

The Application may pop up to 62 raw image bytes for one N-SDU.

No sequence number, image ID, CRC, or additional reliable-transfer header is required.

Only one Application Tx chunk is active at a time.

After the chunk is copied from the UART queue:

```text
UART Queue
    |
    | pop
    v
+----------------+
| App Tx Buffer  |
+--------+-------+
         |
         v
       CanTp
```

The Tx buffer must remain unchanged until CanTp reports the transmission result.

---

## 5.4 Retry Policy

For every Application image chunk:

```text
1 initial attempt
+ maximum 3 retries
```

Illustration:

```text
             +----------------------+
             |   Prepare Tx Buffer  |
             +----------+-----------+
                        |
                        v
             +----------------------+
             |   CanTp Transmit     |
             +----------+-----------+
                        |
             +----------+----------+
             |                     |
             v                     v
        SUCCESS                 FAILURE
             |                     |
             |                retryCount++
             |                     |
             v                     v
 Release current chunk      retryCount <= 3 ?
 Process next chunk          /             \
                            yes            no
                            |               |
                            v               v
                     Retry same chunk   Drop current chunk
                                       Process next chunk
```

The complete image transfer shall **not** be aborted because one chunk fails.

Small corruption in the final ASCII image is acceptable in error conditions.

---

# 6. Slave 1 Image Receiver

Slave 1 processes complete N-SDUs delivered successfully by CanTp.

```text
CAN
 |
v
CanTp Rx
 |
v
App Image Receiver
 |
v
UART Tx
 |
v
PC Terminal
```

Application behavior:

1. receive the N-SDU,
2. extract the image payload,
3. push the payload toward UART transmission,
4. continue receiving following image chunks.

No additional duplicate handling, ACK, retransmission, CRC, or image-recovery algorithm is required.

---

# 7. Provided ASCII Test Data

Suggested files:

| Level | File | Size | Purpose |
|---|---|---:|---|
| Small | `ascii_cat_512B.txt` | 512 B | Basic end-to-end test |
| Medium | `ascii_owl_2KB.txt` | ~2 KB | Multiple N-SDUs |
| Large | `ascii_monalisa_refstyle_8KB.txt` | ~8 KB | Longer transfer |
| XL | `ascii_monalisa_refstyle_16KB.txt` | 16 KB | Stress / showcase test |

The files contain only printable ASCII characters and LF (`\n`) line endings.

---

# 8. Task Summary

## Task 1 — Master KeepAlive + ADC

```text
ADC -> RateLevel -> Countdown -> AliveCounter++
                           |
                           +-> Com_SendSignal()
```

Expected result:
- ADC changes KeepAlive update rate.
- COM period remains fixed.

---

## Task 2 — Slave KeepAlive Reception + LED

```text
KeepAlive Rx Callback
      |
      +--> update AliveCounter
      +--> update RateLevel
      +--> update LED timing
      +--> refresh lastAliveTime
```

Expected result:
- both Slaves detect new KeepAlive,
- LED changes according to rate level,
- timeout leads to `MASTER_LOST`.

---

## Task 3 — Slave Status Monitoring

```text
Slave Status Tx --> CAN --> Master Rx Callback --> online/offline state
```

Expected result:
- Master can show `0/2`, `1/2`, `2/2`.

---

## Task 4 — UART Rx Queue

```text
PC UART -> Master UART Rx -> Queue
```

Expected result:
- queue stores image bytes safely,
- no overwrite.

---

## Task 5 — Master Image Sender

```text
UART Queue -> App Tx Buffer -> CanTp -> CAN
```

Expected result:
- send image chunk by chunk,
- retry up to 3 times,
- drop chunk if still failed.

---

## Task 6 — Slave 1 Image Receiver

```text
CanTp -> App Rx -> UART Tx -> PC Terminal
```

Expected result:
- image is printed at Slave 1 side.

---

# 9. Final Demo

The final demo shall show the three functions working together.

Suggested sequence:

```text
1. Start Master + Slave 1 + Slave 2
   -> Master reports 2/2 Slaves online

2. Rotate the potentiometer
   -> Application KeepAlive rate changes
   -> Both Slave LEDs change visible blink level

3. Increase KeepAlive update rate up to / beyond COM Tx rate
   -> Observe repeated or skipped AliveCounter values as expected

4. Send an ASCII image from PC
   -> Master UART queue receives data
   -> Master sends chunks through CanTp
   -> Slave 1 prints the image through UART

5. Disconnect one Slave
   -> Master reports 1/2 Slaves online

6. Reconnect the Slave
   -> Master returns to 2/2
```

---

# 10. Simplification Rules

The following are intentionally **out of scope**:

- Dynamic ECU discovery
- Network Management protocol
- Dynamic COM Tx period
- Dynamic CanTp STmin
- Application-level ACK
- End-to-end CRC
- Reliable image transfer
- Image retransmission requested by the receiver
- Complex duplicate handling
- Complex diagnostics

---

# 11. Suggested Application Files

```text
App_Heartbeat.c
App_HeartbeatMonitor.c
App_SlaveStatus.c
App_NetworkMonitor.c
App_ImageSender.c
App_ImageReceiver.c
App_Main.c
```

This file structure is only a recommendation.

The Application should remain small enough that the main learning focus stays on the Mock COM Stack.
