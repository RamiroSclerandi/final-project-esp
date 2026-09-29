# datalogger.v1 Fixtures

Canonical golden payloads for the `datalogger.v1` JSON envelope produced by
`JsonCodec::encode()`. These files ARE the contract: the backend ingest
worker must accept them unchanged. `test/test_codec/test_json_codec.cpp`
byte-compares `JsonCodec::encode()`'s output against them directly (not a
semantic/round-trip diff), so any change here or in `JsonCodec.cpp` must keep
both in sync.

The backend worker's `Meta` model uses pydantic `extra="forbid"`
(`proyecto-final-backend ingest-worker/src/ingest/domain/payload.py:30`),
so it rejects any field these fixtures do not already contain, including
`meta.lost`. **Deployment order matters**: the backend must accept `lost`
(and `store.drop`) in its `Meta` model *before* firmware 1.2.0 is flashed to
any production device, or the worker will reject every message a 1.2.0 node
sends.

Every fixture is generated from the same two-channel scenario: one
sensor (`TestSensor`) reporting a valid `temperature` reading and a failed
`pressure` reading, encoded with a fixed `PayloadMeta` (`rssi:-55, seq:42,
boot:1`, no reset reason, no store info) through the `env:native` host shims
(`lib/native_support/DeviceInfo.cpp`, fixed device id `AABBCCDDEEFF`, fixed
epoch `1700000000`, clock reported as synced).

## Files (firmware v1.2.0)

- `valid.json` — a normal payload: `v:1`, one channel with a value (`ok:true`),
  one failed channel (`ok:false`, no `val`), `meta.lost:0`.
- `oversize.json` — **empty (0 bytes)**, on purpose. When the destination
  buffer is too small, `JsonCodec::encode()` returns `0` and leaves `out`
  as an empty C-string rather than emitting truncated/malformed JSON. That
  guarded state has no JSON envelope to send, so its golden is genuinely
  empty — this documents the guard's exact current shape, it does not
  represent a message the backend ever receives. Unaffected by `meta.lost`:
  the destination buffer overflows before any field is serialized.
- `meta_lost_zero.json` — the same scenario as `valid.json`, kept as its own
  fixture to document the `meta.lost` field specifically (cumulative
  device-side loss count, always present, `0` on a boot with no drops).
- `meta_lost_nonzero.json` — same scenario with `meta.lost:3`, documenting a
  boot that recorded three device-side drops before this payload was built.

`valid.json` and the `meta_lost_*` fixtures all omit the `meta.store`
object because their `PayloadMeta.storeKind` is `nullptr` in the test
setup, not because the field does not exist; production always sets
`storeKind`, so a real device's payloads carry `meta.store.drop` alongside
`meta.lost`.

## seq gap invariant (G-10)

`meta.lost` and `meta.store.drop` count two different categories of
device-side loss, and only one of them ever explains a `seq` gap:

- `meta.lost` — PRE-EMISSION drops: a reading discarded before its `seq`
  was ever assigned (failed encode, oversize guard, or
  queue-full-and-buffer-append-failure). `seq` was never stamped for these,
  so they never open a gap.
- `meta.store.drop` — POST-EMISSION drops: a record whose `seq` WAS already
  assigned, then lost from local storage — either an *undelivered* record
  evicted under space pressure (an eviction later still delivered does not
  count), or lost when networkTask's send failed and its own re-append also
  failed. Both leave a `seq` gap.

So: `seq gap = transport/broker loss + Δmeta.store.drop`, and
`total device loss = Δmeta.lost + Δmeta.store.drop`. Subtracting
`Δmeta.store.drop` (never `Δmeta.lost`) from an observed `seq` gap isolates
true transport/broker loss.
