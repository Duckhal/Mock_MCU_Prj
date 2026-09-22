# CanTp Phase 2 tests

Run from the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File tests/cantp/run_tests.ps1
```

The runner verifies the approved T01-T08 and T13 behavior: Phase 1 wire
vectors, STmin across every adjacent CF pair, immutable Data/FC retries,
retry exhaustion, N_As/N_Ar/N_Bs/N_Cr boundaries, abort-once behavior and
late-confirmation resource locks. It also checks application ownership, the
two-slot Rx queue, PduR Data/FC routes, and strict ARM object compilation.
The reproducible evidence log is written to
`build/cantp_phase2/verification.log`.

Queue-full, OVFLW and active-session replacement remain Phase 3 work.
