# CanTp Phase 1-3 tests

Run from the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File tests/cantp/run_tests.ps1
```

The runner verifies T01-T14 through the Phase 3 gate. Coverage includes the
Phase 1 wire vectors, every Phase 2 retry/timer/late-confirmation rule, and the
Phase 3 wrong-SN, malformed-length, queue-full SF/FF, standalone OVFLW,
final-CF padding, invalid-FC and active-session replacement behavior. It also
checks application ownership, the two-slot Rx queue, PduR Data/FC routes and
strict ARM object compilation. The reproducible verification log is written
to `build/cantp_phase3/verification.log`.

Generate the checked-in evidence package with:

```powershell
powershell -ExecutionPolicy Bypass -File tests/cantp/generate_evidence.ps1
```

The generator writes the complete package under `evidence/cantp_phase3/`
while preserving the earlier Phase 2 package.
