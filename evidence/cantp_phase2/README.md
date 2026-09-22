# CanTp Phase 1-2 acceptance evidence

This package covers every Acceptance Matrix case implemented through Phase 2:
T01-T08 and T13. Results come from deterministic host fixtures with a 1 ms
virtual tick and the production `Cantp.c`/`Cantp_Cfg.c`. `E_OK=0`,
`E_NOT_OK=1`; abort reason values are decoded in each report.

| Test | Result | Report | Primary evidence |
|---|---|---|---|
| T01 | PASS | [T01](T01.md) | SF, zero FC, READY 5 B, Tx/Rx callbacks |
| T02 | PASS | [T02](T02.md) | FF + 2 CF + CTS, offsets 6/13/20 |
| T03 | PASS | [T03](T03.md) | 9 Data + 2 FC, complete 62-byte payload |
| T04 | PASS | [T04](T04.md) | CF1-CF8 tick trace and two CTS gates |
| T05 | PASS | [T05](T05.md) | Three immutable CF3 attempts |
| T06 | PASS | [T06](T06.md) | Four rejected CF3 attempts and one abort |
| T07 | PASS | [T07](T07.md) | N_Bs 99/100 ms boundary and same-tick CTS |
| T08 | PASS | [T08](T08.md) | N_Cr restart/timeout and reservation release |
| T13 | PASS | [T13](T13.md) | N_As timeout, retained lock and late confirmation |

Supporting Phase 2 checks for FC retry and N_Ar are in
[SUPPLEMENTAL_FC_N_AR.md](SUPPLEMENTAL_FC_N_AR.md).

Raw artifacts:

- [`raw/protocol_trace.log`](raw/protocol_trace.log): frame, tick, state,
  timer and callback observations for T01-T08/T13.
- [`raw/routing_trace.log`](raw/routing_trace.log): NodeApp/PduR READY queue
  and routing observations.
- [`raw/full_verification.log`](raw/full_verification.log): compilation,
  regression and ARM-object verification.
- [`raw/environment.txt`](raw/environment.txt): compiler versions, source
  commit/status and SHA-256 hashes.

Regenerate all artifacts from the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File tests/cantp/generate_evidence.ps1
```

These are host/simulation results. They prove deterministic protocol behavior
against the approved fixtures; they are not a physical CAN-bus capture.

