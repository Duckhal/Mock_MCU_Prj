# CanTp Phase 3 acceptance evidence

This package covers the Phase 3 gate in section 10 of the student guide:
T09-T12 and T14, plus regression of all earlier T01-T08/T13 behavior. Results
come from deterministic host fixtures with a 1 ms virtual tick and the
production `Cantp.c`/`Cantp_Cfg.c`. `E_OK=0` and `E_NOT_OK=1`.

| Test | Result | Report | Primary evidence |
|---|---|---|---|
| T09 | PASS | [T09](T09.md) | Wrong SN aborts before append and releases once |
| T10 | PASS | [T10](T10.md) | Full queue emits standalone OVFLW; sender aborts |
| T11 | PASS | [T11](T11.md) | Oversized FF emits OVFLW; malformed frames preserve session |
| T12 | PASS | [T12](T12.md) | One 62-byte copy and READY-before-indication order |
| T14 | PASS | [T14](T14.md) | Valid FF replaces active session cleanly |

[SUPPLEMENTAL_DEFENSIVE.md](SUPPLEMENTAL_DEFENSIVE.md) records the additional
Phase 3 checks for final-CF padding, queue-full SF, standalone OVFLW resource
locking and invalid FC handling.

Raw artifacts:

- [raw/phase3_trace.log](raw/phase3_trace.log): T09-T12/T14 and supplemental
  defensive observations.
- [raw/protocol_trace.log](raw/protocol_trace.log): previous T01-T08/T13
  protocol regressions.
- [raw/routing_trace.log](raw/routing_trace.log): NodeApp/PduR queue and route
  regressions.
- [raw/full_verification.log](raw/full_verification.log): all host tests and
  strict ARM object compilation.
- [raw/environment.txt](raw/environment.txt): compiler versions, repository
  state and SHA-256 hashes for production/test inputs.

Regenerate the package from the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File tests/cantp/generate_evidence.ps1
```

These are deterministic host/simulation results. They verify protocol state,
frames, timers, callbacks and queue ownership; they are not a physical CAN-bus
capture.
