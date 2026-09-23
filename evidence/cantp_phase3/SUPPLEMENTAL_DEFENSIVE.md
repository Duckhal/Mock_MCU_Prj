# Supplemental Phase 3 defensive checks

- **Final-CF padding:** a 60-byte N-SDU copied only its five real bytes from
  the final CF and ignored both padding bytes. **PASS**.
- **Queue-full SF:** a valid SF received with both slots READY produced no FC
  and left both queued messages unchanged. **PASS**.
- **Valid SF replacement:** a valid three-byte SF replaced an active segmented
  receive while the FC resource was idle. The old reservation failed once and
  the clean SF payload became READY without another FC. **PASS**.
- **Standalone OVFLW lock:** after CanIf accepted OVFLW, the immutable FC frame
  and pending flag remained protected through the N_Ar timeout. A late FC
  confirmation only unlocked the resource and did not create or complete an Rx
  session. **PASS**.
- **Invalid FC:** an FC received while Tx was idle had no effect. CTS with BS
  3, CTS with STmin 6, FC(OVFLW), and unsupported FC(WAIT) each aborted an
  independently reset sender once with result `E_NOT_OK` and abort reason 7 =
  `CANTP_ABORT_INVALID_FLOW_CONTROL`. **PASS**.

Evidence is in the `EVIDENCE SUP_PHASE3` and `EVIDENCE SUP_INVALID_FC` lines of
[phase3_trace.log](raw/phase3_trace.log).
