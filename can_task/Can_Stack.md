# CAN Communication Module Report: NXP S32K144

## 1. System Overview & Architecture

The CAN module is an AUTOSAR-like layered stack for FlexCAN0. The application uses PDU identifiers, `CanUpper` maps PDUs to hardware handles, and `Can` accesses FlexCAN registers.

```text
Application (`can_task/test/main.c`)
        |
        v
CAN Upper Layer (`CanUpper.c/.h`)
        |
        v
CAN Driver Layer (`Can.c/.h`)
        |
        v
S32K144 FlexCAN0
```

Responsibilities are separated as follows:

* The application selects and executes TC-001 through TC-004.
* `CanUpper` maps PDU IDs to CAN IDs and HTH/HRH IDs, tracks TX status, and stores received data.
* `Can` configures FlexCAN0, message buffers, bit timing, filters, and TX/RX processing.
* FlexCAN0 provides MB0 for TX and MB1 for RX in the final configuration.

The final implementation uses polling mode. The application calls `CanUpper_MainFunction()`, which calls `Can_MainFunction_Write()` and `Can_MainFunction_Read()`.

## 2. Bit Timing Configuration

The CAN controller uses the external 8 MHz crystal through `SOSCDIV2` and is configured for 500 kbps.

* Nominal bit time: $T_{bit} = \frac{1}{500\,\text{kbps}} = 2\,\mu\text{s}$.
* Time quanta per bit: $16\,\text{TQ}$.
* Time quantum: $T_q = \frac{2\,\mu\text{s}}{16} = 125\,\text{ns}$.
* Prescaler: `PRESDIV = 0`.
* Segments: `SYNC_SEG = 1`, `PROPSEG = 7`, `PSEG1 = 6`, `PSEG2 = 2` TQ.
* Sample point: $\frac{1 + 7 + 6}{16} = 87.5\%$.

These values are written while FlexCAN is in Freeze Mode.

## 3. Hardware Object Handle Mapping

| HOH | Logical handle | Message buffer | Direction | CAN ID/filter | Purpose |
| :--- | :--- | :---: | :---: | :--- | :--- |
| `CAN_HTH_0` | HTH 0 | MB0 | TX | ID supplied by PDU | Transmit frames |
| `CAN_HRH_0` | HRH 0 | MB1 | RX | `0x123`, mask `0x7FF` | Receive LED-control frames |
| Unassigned | N/A | MB2..MB15 | Inactive | N/A | Reserved |

`PDU_TX_LED_COMMAND` maps to CAN ID `0x123` and `CAN_HTH_0`. `PDU_RX_LED_COMMAND` maps to `CAN_HRH_0`.

## 4. Key Implementation Details

* Standard 11-bit CAN IDs are shifted by `CAN_STANDARD_ID_SHIFT` before writing to the message-buffer ID field.
* Payload bytes are packed MSB-first into the two FlexCAN data words and unpacked in the reverse order.
* RX processing reads the control/status word and payload, reads `CAN0->TIMER` to unlock the message buffer, then clears the `IFLAG1` bit using write-1-to-clear semantics.
* RX masks are configured only while `MCR[FRZACK]` is asserted.
* All pointer and range inputs are validated by the driver or upper layer.

```text
TX: CanUpper_Transmit()
    -> Can_Write()
    -> FlexCAN MB0
    -> Can_MainFunction_Write()
    -> CanUpper_TxConfirmation()

RX: FlexCAN MB1
    -> Can_MainFunction_Read()
    -> CanUpper_RxIndication()
    -> CanUpper_GetRxData()
    -> Application LED control
```

## 5. TC-003 Application Protocol

TC-003 uses two S32K144EVB boards connected to the CAN bus. Board A transmits and Board B controls its LEDs.

### Transmitter flow

1. Configure SW2 as active-low input **PTC12** with an internal pull-up.
2. Poll and debounce the button.
3. Increment the press counter and sequence number.
4. Send a three-byte frame through `CanUpper_Transmit(PDU_TX_LED_COMMAND, ...)`.
5. Poll until `PDU_TX_DONE`, then wait for button release.

### Receiver flow

1. Call `CanUpper_MainFunction()`.
2. Read `PDU_RX_LED_COMMAND` using `CanUpper_GetRxData()`.
3. Accept only frames whose magic byte is `0xCA`.
4. Turn off all LEDs and apply the command byte.

| Byte | Meaning |
| :---: | :--- |
| 0 | Magic byte `0xCA` |
| 1 | `0x01` blue, `0x02` red, `0x03` green/yellow, `0x00` all off |
| 2 | Incrementing sequence number |

The RGB LEDs are active-low: `0` turns an LED on and `1` turns it off.

## 6. Verification and Test Results

Testing was performed on an NXP S32K144 evaluation platform using S32 Design Studio and an OpenSDA/PEMicro debugger.

| Test ID | Objective | Verification | Result |
| :--- | :--- | :--- | :--- |
| **TC-001** | Driver initialization and parameter validation | Freeze mode exit, MB0/MB1 setup, ID/mask values, NULL PDU rejection, and invalid ID rejection | **PASS** |
| **TC-002** | Loopback TX-to-RX | Transmit `{0xDE, 0xAD, 0xBE, 0xEF}`, poll until TX done, read RX, and compare every byte | **PASS** |
| **TC-003** | Physical two-board LED control | SW2 on PTC12 sends CAN ID `0x123` with the `0xCA` protocol frame; receiver validates it and changes the requested LED | **PASS** |
| **TC-004** | Robustness and boundary validation | Invalid parameters are rejected; zero-length and eight-byte frames transmit without a HardFault | **PASS** |

## 7. Compliance with Homework Section 11

| Criterion | Weight | Final status and evidence |
| :--- | :---: | :--- |
| Compile without errors | 15% | **PASS**. `Low_Level_S32K144` builds successfully with the S32DS GCC toolchain. |
| Loopback TX/RX operation | 25% | **PASS**. TC-002 confirms status transitions and byte-for-byte payload integrity. |
| Two-board LED control | 20% | **PASS**. TC-003 confirms PTC12 -> CAN ID `0x123` -> protocol decoding -> LED control. |
| Robustness | 15% | **PASS**. TC-004 covers NULL pointers, invalid IDs, invalid lengths, invalid handles, and valid boundary lengths. |
| Code quality | 15% | **PASS**. Driver, upper layer, configuration, and application responsibilities are separated; inputs are validated and hardware mappings are centralized. |
| Design and flow explanation | 20% | **PASS**. HOH abstraction, PDU mapping, polling flow, Freeze Mode, byte order, and RX buffer unlock are documented above. |

## 8. Final Conclusion

TC-001, TC-002, TC-003, and TC-004 pass. The CAN stack satisfies the functional and documentation requirements of the homework.