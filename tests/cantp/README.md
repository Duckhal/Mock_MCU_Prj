# CanTp Phase 1 tests

Run from the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File tests/cantp/run_tests.ps1
```

The runner verifies the approved T01-T03 wire vectors, SF/FF/CF/CTS flow,
block size and STmin gates, confirmation-time progress commits, application
ownership, the two-slot Rx queue, PduR Data/FC routes, and strict ARM object
compilation. The reproducible evidence log is written to
`build/cantp_phase1/verification.log`.

This suite covers Phase 1 only. Retry, timeout, OVFLW and session replacement
belong to Phases 2 and 3.
