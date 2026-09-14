# Part 1 Architecture Notes

This document explains **why** the training architecture in `assignment_part1_com_signal.md` was designed this way.

It is not the assignment specification itself.

---

## 1. Purpose of the Training Model

The training stack is intentionally smaller than production AUTOSAR Classic.

The objective is not to reproduce every AUTOSAR configuration option. The objective is to preserve the most important architectural boundaries:

```text
COM    = Signal packing + communication scheduling
PduR   = PDU routing
CanIf  = Logical CAN mapping
CanDrv = CAN hardware control
```

The model should be small enough that students can implement it themselves and understand every relationship.

---

## 2. Why Signal → Signal Group → I-PDU?

The class uses:

```text
N Signals
    ↓
1 Signal Group
    ↓
1 I-PDU
```

This separates three concepts:

```text
Signal       = UPDATE UNIT
Signal Group = GROUPING / PACKING UNIT
I-PDU        = SCHEDULING / TRANSMISSION UNIT
```

This creates the mental model:

```text
Com_SendSignal()   = update communication state
Com_MainFunctionTx = schedule communication
```

---

## 3. Why Does Signal Not Directly Store `IPduRef`?

A direct `Signal → IPduRef` relationship is convenient for `Com_SendSignal()`, but inconvenient when processing an entire I-PDU.

The training model instead uses:

```text
Signal → Signal Group → I-PDU
```

A generator may still create optimized reverse lookup tables for runtime.

> Model relationship and runtime lookup structure do not have to be identical.

---

## 4. Why Use a Signal Slot?

Each Signal uses one self-contained representation:

```text
bit N-1                          bit 1 bit 0
┌────────────────────────────────────┬───┐
│          Signal Payload            │ U │
└────────────────────────────────────┴───┘
```

This allows:

```text
Encode         = (Payload << 1) | U
Decode Payload = Slot >> 1
Decode U       = Slot & 1
```

This avoids arbitrary bit extraction, cross-byte fields, unrelated mask/merge operations, and separate Update Bit lookup.

---

## 5. Why Must Signal Slots Be Byte Aligned?

Training rules:

```text
SlotStartBit % 8 == 0
SlotLength   % 8 == 0
```

The goal is predictable access.

Trade-off:

```text
Implementation simplicity ↑
Runtime predictability ↑
Debug simplicity ↑
Traceability ↑

Packing flexibility ↓
Maximum payload utilization ↓
```

---

## 6. Why Is the Update Bit Inside the Signal Slot?

The class chose:

```text
[Payload][UpdateBit]
```

as one aligned slot.

There is no separate Update Bit area.

Advantages:

- one access retrieves both payload and freshness;
- no separate bit-position table is required;
- easy to visualize in CAN traces;
- simple encode/decode logic.

This is a training convention, not a general AUTOSAR requirement.

---

## 7. Software Data Type vs Network Payload Size

A C type and the number of payload bits on the network are different concepts.

Example:

```text
Software type = uint16
16-bit Signal Slot = 15 payload bits + 1 Update Bit
```

Students shall not assume:

```text
sizeof(C type) = network payload width
```

---

## 8. Why Is I-PDU the Scheduling Unit?

Signals represent state values. The network transmits a complete serialized message.

Example:

```text
VehicleSpeed changes at t=3 ms
Gear changes at t=7 ms
VehicleStatusPdu is due at t=10 ms
```

The frame at t=10 ms contains the latest current state.

---

## 9. Why Use `Com_MainFunctionTx()`?

`Com_SendSignal()` should not directly control network timing.

```text
Com_SendSignal()    → update COM buffer
Com_MainFunctionTx → apply communication timing policy
```

This separates data update timing from network transmission timing.

---

## 10. Why Use a 1 ms COM MainFunction?

A 1 ms base tick gives a simple integer timing model:

```text
10 ms PDU  → 10 ticks
20 ms PDU  → 20 ticks
100 ms PDU → 100 ticks
```

Part 1 requires:

```text
PduPeriod % MainFunctionPeriod == 0
```

This avoids fractional scheduling.

---

## 11. Why Use an Initial Offset?

Without offsets:

```text
A = 10 ms
B = 20 ms
C = 50 ms
D = 100 ms
```

may produce a burst where all are due together.

Initial offsets allow static load distribution without an advanced scheduler.

Example:

```text
A: period=10,  offset=1
B: period=20,  offset=5
C: period=50,  offset=12
D: period=100, offset=25
```

---

## 12. Why No Priority or Fairness Scheduler?

Priority and fairness are valid production concerns, but they introduce concepts that are not necessary for Part 1:

- priority queues;
- starvation;
- round-robin indices;
- software arbitration;
- scheduling policies.

The class intentionally uses static configuration order.

The goal is to understand the communication stack before studying scheduler design.

---

## 13. Why Retry as Early as Possible?

If an I-PDU is due at 10 ms and the lower layer is busy, waiting until the next full period would introduce unnecessary latency.

Therefore:

```text
CAN_BUSY → PENDING → retry next Com_MainFunctionTx tick
```

For a 1 ms main function:

```text
t=10 → BUSY
t=11 → retry
```

---

## 14. Why Must Retry Be Non-blocking?

Forbidden:

```c
while (Can_Write(...) == CAN_BUSY)
{
}
```

because one busy PDU would prevent all other communication work from progressing.

Instead:

```text
PDU0 → BUSY
PDU1 → still checked
PDU2 → still checked
```

---

## 15. Why Bound the Number of Retries?

Retrying forever creates a permanently pending I-PDU when the lower communication path remains unavailable.

Part 1 therefore uses:

```text
initial attempt
+
max_retries
```

Example:

```text
max_retries = 3
→ 1 initial attempt + 3 retries
→ at most 4 total attempts
```

After the retry budget is exhausted:

```text
drop current occurrence
pending = FALSE
retry_count = 0
```

> Drop one transmission occurrence, not the I-PDU.

The I-PDU remains active at future nominal periods. Update Bits remain set because the lower stack never accepted the dropped occurrence.

This adds only one small runtime counter but avoids unbounded retry behavior.

---

## 16. Why "Latest Value Wins"?

Example:

```text
t=10  Speed=100 → due → BUSY
t=15  Speed=110
t=18  Speed=120
t=21  transmission accepted
```

The transmitted value is 120.

The model does not queue 100, 110, and 120 because Signal communication represents state, not an event log.

Missed periodic occurrences are coalesced into one pending transmission.

---

## 17. Why Must the Nominal Schedule Not Drift?

For a 10 ms period:

```text
Nominal occurrences = 10, 20, 30, 40, ...
```

If the request due at 10 ms is accepted at 12 ms, the next nominal occurrence remains 20 ms.

Otherwise repeated BUSY conditions would progressively shift the periodic schedule.

Therefore scheduling timeline and actual transmission-request time are separate concepts.

---

## 18. Why Clear Update Bits on Accepted Transmit Request?

Part 1 uses:

```text
PduR_ComTransmit() == E_OK → clear Update Bits
```

This is intentionally simple.

Important:

```text
E_OK = lower communication stack accepted the request
```

It does not mean the CAN frame has physically completed transmission.

A more advanced design could clear Update Bits on TxConfirmation instead. That is deferred.

---

## 19. Why `Can_MainFunction_Write()` Exists

With polling-based Tx completion:

```text
Can_Write()
   ↓
request accepted
   ↓
hardware busy
   ↓
Can_MainFunction_Write()
   ↓
Tx complete
   ↓
CanIf_TxConfirmation()
```

`Com_MainFunctionTx()` asks:

```text
When should this I-PDU be requested?
```

`Can_MainFunction_Write()` asks:

```text
Has the accepted hardware transmission completed?
```

These are different responsibilities.

---

## 20. Why Run CAN MainFunction Before COM MainFunction?

Recommended polling order:

```text
1 ms tick

Can_MainFunction_Write()
        ↓
release completed Tx resource

Com_MainFunctionTx()
        ↓
pending PDU can retry immediately
```

This is a training recommendation, not a universal scheduling rule.

---

## 21. Configuration Ownership Philosophy

```text
COM    owns communication data model
PduR   owns routing
CanIf  owns logical CAN mapping
CanDrv owns CAN hardware resources
```

This separation is more important than the numeric values of handles.

---

## 22. Why Does CanIf Own CAN ID Mapping?

In mandatory Direct CAN Binding, PduR remains payload-transparent and routes local handles:

```text
Source logical PDU → Destination logical PDU
```

If optional multiplexed Global PDU mode is implemented later, the Global PDU mapping/demultiplexing function remains an internal PduR subcomponent:

```text
GlobalPduId ↔ local PduR route handle
```

This does not make PduR depend on CAN hardware concepts such as HTH, HRH, or Controller.

CanIf knows:

```text
Logical CAN L-PDU → CAN-specific representation
```

Tx:

```text
Tx L-PDU → CAN ID + HTH
```

Rx:

```text
HRH + CAN ID → Rx L-PDU
```

---

## 23. Why Does CanDrv Own HTH/HRH?

HTH and HRH represent CAN hardware resources.

Therefore their definitions belong to CanDrv.

CanIf references them but does not own them.

---

## 24. Why Are HOH IDs Unique Across One CanDrv?

The training model supports multiple CAN Controllers:

```text
CanDrv
├── CAN0
└── CAN1
```

With unique HOH IDs:

```text
CAN0: HTH 0, HRH 1
CAN1: HTH 2, HRH 3
```

then:

```text
HOH → Hardware Object → Controller
```

is enough to identify the Controller.

---

## 25. Why Does Tx Not Need `ControllerId`?

Tx starts from a known logical Tx L-PDU.

CanIf resolves:

```text
TxPduId → HTH
```

CanDrv resolves:

```text
HTH → Hardware Object → Controller
```

Therefore `ControllerId` would be redundant in the training Tx API.

---

## 26. Why Does Rx Use `HRH + CAN ID`?

An HRH may receive many CAN IDs in BasicCAN.

Example:

```text
HRH 3
0x100 → RxPduA
0x200 → RxPduB
0x321 → VehicleStatusRx
```

Therefore HRH alone does not identify the logical Rx PDU.

The required information is:

```text
HRH + CAN ID
```

---

## 27. Why Does the Training Rx API Differ from AUTOSAR?

Training API:

```c
CanIf_RxIndication(Hrh, RxPdu);
```

AUTOSAR uses a more generic receive hardware-context object conceptually containing:

```text
CAN ID
HOH
ControllerId
```

The training model does not need explicit `ControllerId` because HOH is unique within one CanDrv instance and therefore `HRH → Controller` is already derivable.

---

## 28. Why Avoid the Term "Mailbox" in the Training API?

"Mailbox" can easily be confused with a physical FlexCAN Message Buffer.

The training API exposes:

```text
HRH + RxPdu
```

which directly communicates:

```text
Which receive hardware resource?
+
Which CAN frame/data?
```

Physical mailbox/message-buffer details remain inside CanDrv.

---

## 29. Tx and Rx Are Not Mirror Images

Tx begins with logical identity:

```text
Tx L-PDU → CAN ID + HTH → hardware
```

Rx begins with hardware/network context:

```text
hardware → HRH + CAN ID → Rx L-PDU
```

Useful class phrase:

```text
TX = logical → hardware
RX = hardware/network → logical
```

---

## 30. Why Make `GlobalPduId` the Canonical Identity?

Module-local IDs solve local lookup problems:

```text
ComIPduId
PduR local PduId
CanIfTxPduId
CanIfRxPduId
HTH / HRH
```

but they make end-to-end tracing harder because one logical message appears under different handles at different layers.

`GlobalPduId` provides one stable system identity:

```text
GlobalPduId 0x0010 = VehicleStatus
```

The runtime APIs do not need to carry it everywhere. A logger or route may derive it from generated configuration.

```text
one logical message = one traceable system identity
```

---

## 31. Why Is Direct CAN Binding an Optimization of the General Model?

The general identity is:

```text
GlobalPduId = logical message identity
```

For CAN, a one-to-one binding can provide:

```text
GlobalPduId ↔ CanIf L-PDU ↔ CAN ID
```

CAN ID already identifies the message on the wire, so serializing another Global PDU ID field is redundant.

`GlobalPduId` remains present in the system model and trace database but becomes **implicit on wire**.

This preserves system-wide trace identity with zero GlobalPduId payload overhead, normal CAN hardware filtering, and normal CAN arbitration.

This is a **CAN-optimized Direct Binding**, not a BasicCAN-only optimization.

BasicCAN naturally resolves:

```text
HRH + CAN ID → Rx L-PDU → GlobalPduId
```

through configuration.

---

## 32. What Is the Multiplexed Global PDU Case?

Several logical PDUs may share one channel:

```text
VehicleStatus ─┐
EngineStatus  ─┼─→ Generic CAN channel
ClimateStatus ─┘
```

Here CAN ID identifies the channel, so the wire data must carry the logical identity:

```text
┌─────────────┬────────────────────┐
│ GlobalPduId │ Logical PDU Data   │
└─────────────┴────────────────────┘
```

Rx:

```text
CAN ID
  ↓
Generic CanIf Rx L-PDU
  ↓
extract GlobalPduId
  ↓
GlobalPduId → local route
  ↓
COM
```

Part 1 does not require students to implement this mode. It exists to show that Direct CAN Binding is an efficient specialization of the general identity model.

---

## 33. Why Is Global PDU Mapping Considered Part of PduR?

It is useful to describe a "Global-PDU Adapter" conceptually, but it does not need to become another public module.

Externally:

```text
COM → PduR → CanIf → CanDrv
```

Internally, multiplexed PduR may contain:

```text
PduR
├── Local Route Mapping
└── Global PDU Mapping / Demultiplexing
```

Rx:

```text
Generic CanIf Rx L-PDU
        ↓
PduR Global PDU Demux
        ↓ extract GlobalPduId
GlobalPduId → local PduR source handle
        ↓
normal route
        ↓
COM
```

Tx is the reverse operation.

```text
GlobalPduId ≠ PduR local PduId
```

The internal mapping translates between system identity and module-local routing identity.

---

## 34. Why Does Global PDU ID Improve Debugging?

Without a global identity:

```text
ComIPduId   = 3
PduR PduId  = 7
CanIfPduId  = 2
CAN ID      = 0x321
```

With:

```text
GlobalPduId = 0x0010 = VehicleStatus
```

a trace can correlate all layers:

```text
[COM]    GlobalPduId=0x0010
[PduR]   GlobalPduId=0x0010
[CanIf]  GlobalPduId=0x0010, CANID=0x321
[CanDrv] GlobalPduId=0x0010, Controller=CAN1, HTH=2
```

The logger may derive `GlobalPduId` from configuration. It does not need to be transmitted through every API.

This improves trace correlation, debugging, configuration review, test reporting, and cross-layer diagnostics.

---

## 35. Training Model vs Production Architecture

The training model intentionally favors:

```text
clarity
predictability
small state machines
simple lookup
visible ownership
```

over:

```text
maximum configurability
maximum packing efficiency
advanced scheduling
all AUTOSAR variants
```

Intended learning progression:

```text
understand responsibility
        ↓
implement simple model
        ↓
observe limitations
        ↓
introduce advanced mechanisms later
```

---

## 36. Key Design Decisions Summary

```text
Signal = byte-aligned update unit
Signal Slot = (Payload << 1) | UpdateBit
Signal Group = grouping unit
I-PDU = scheduling/transmission unit
COM MainFunction = period + offset + pending + retry_count
```

```text
Dynamic Tx =
early retry
+
non-blocking
+
bounded retry
+
drop current occurrence after retry limit
+
latest value wins
+
no schedule drift
```

```text
GlobalPduId = canonical system-wide logical PDU identity
Direct CAN Binding = GlobalPduId ↔ CanIf L-PDU ↔ CAN ID
Multiplexed Global PDU = optional explicit on-wire GlobalPduId
```

```text
PduR = local routing + optional internal Global PDU mapping/demux
CanIf = logical CAN mapping owner
CanDrv = hardware resource owner
HOH = unique within one CanDrv instance
```

```text
Tx = L-PDU → CAN ID + HTH → Controller
Rx = Controller/HW → HRH + CAN ID → L-PDU
```
