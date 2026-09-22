# Supplemental Phase 2 evidence - FC retry and N_Ar

These checks are required by the Phase 2 debug checklist but do not have a
separate T-number in the Acceptance Matrix.

- CTS request attempts occurred at ticks `1, 2, 3` for injected results
  `E_NOT_OK, E_NOT_OK, E_OK`; all eight frame bytes were identical and no Rx
  failure callback occurred after the accepted FC was confirmed.
- With an accepted CTS and no local FC confirmation, N_Ar expired at tick 101;
  Rx final count was 1, the reservation was released, and abort reason 4 was
  `N_AR_TIMEOUT`.
- A late FC confirmation did not recreate the aborted Rx session. A subsequent
  clean FF could reserve a new session.
- Four rejected CTS attempts produced one Rx failure and abort reason 2 =
  `FC_RETRY_EXHAUSTED`.
- **Result:** **PASS**.
- **Evidence:** `EVIDENCE SUP_FC_RETRY` and `EVIDENCE SUP_N_AR` lines in
  [protocol_trace.log](raw/protocol_trace.log).

