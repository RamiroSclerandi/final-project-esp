# datalogger.v1 Fixtures

Canonical golden payloads for the `datalogger.v1` JSON envelope produced by
`JsonCodec::encode()`. These files ARE the contract: the backend ingest
worker must accept them unchanged. `test/test_codec/test_json_codec.cpp`
byte-compares `JsonCodec::encode()`'s output against them directly (not a
semantic/round-trip diff), so any change here or in `JsonCodec.cpp` must keep
both in sync.

Every fixture is generated from the same two-channel scenario: one
sensor (`TestSensor`) reporting a valid `temperature` reading and a failed
`pressure` reading, encoded with a fixed `PayloadMeta` (`rssi:-55, seq:42,
boot:1`, no reset reason, no store info) through the `env:native` host shims
(`lib/native_support/DeviceInfo.cpp`, fixed device id `AABBCCDDEEFF`, fixed
epoch `1700000000`, clock reported as synced).

## Files (firmware v1.1.0)

- `valid.json` — a normal payload: `v:1`, one channel with a value (`ok:true`),
  one failed channel (`ok:false`, no `val`).
- `oversize.json` — **empty (0 bytes)**, on purpose. When the destination
  buffer is too small, `JsonCodec::encode()` returns `0` and leaves `out`
  as an empty C-string rather than emitting truncated/malformed JSON. That
  guarded state has no JSON envelope to send, so its golden is genuinely
  empty — this documents the guard's exact current shape, it does not
  represent a message the backend ever receives.

`meta.lost` and `meta.store.drop` fixtures (`meta_lost_zero.json`,
`meta_lost_nonzero.json`) are added in PR4 alongside the fields themselves;
`valid.json`/`oversize.json` here predate those fields (firmware 1.1.0).
