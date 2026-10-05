# Firmware audit follow-ups (E-1, E-2, E-3, E-4, E-7)

## Objective

Close the firmware follow-ups E-1, E-2, E-3, E-4 and E-7 from the E2E architecture audit
(`Auditoria_E2E_Arquitectura.md`, section "Follow-ups por sistema" → "Firmware").

## Problem and why

The 1.2.0 hardening left five small, surgical gaps: a wrong test count in the handoff and
verify memory (E-1), local secret files not covered by `.gitignore` (E-2), a provisioning
`readLine()` that can block forever (E-3), a `networkTask` watchdog budget that does not
account for DNS and is fed only once per connection attempt (E-4), and an offline-restart
rule that reads the MQTT link even when LoRaWAN is selected (E-7).

## Scope

- In: E-1, E-2, E-3, E-4, E-7.
- Out: E-5, E-6, E-8, E-9, E-10, enabling Modbus, any change to the `datalogger.v1`
  contract, other repositories.

## Constraints

- No hardware available: no flashing, no on-target runs. Everything that only hardware
  can prove is listed per PR under "Requires hardware validation".
- E-4 touches the watchdog margin: safety first, native tests, and no merge if the change
  can only be made safely with hardware.
- One branch and one PR per work unit; auto-merge after the required CI checks
  (`native`, `firmware`, `format`).

## Delivery strategy

`ask-on-risk`, one PR per item. Forecast: well under ~400 authored lines per PR.

## Checks per work unit

- `pio test -e native`
- `pio run -e esp32dev`
- `pio check -e esp32dev --fail-on-defect=medium --fail-on-defect=high`
- `git clang-format --diff --extensions c,cpp,h,hpp origin/main` (same as CI `format`)

## Tasks

- [x] E-1 — Correct "95 tests" to 88 in the handoff and in engram #482.
  - Route: inline (docs outside the repo + one memory update).
  - Evidence: handoff §9 now says 88/88 (11 suites); engram #482 corrected in both
    mentions. No PR (nothing in this repository changes).
- [x] E-2 — Ignore `credentials.txt` and `.env` files.
  - Route: inline (one mechanical file).
  - RED: `git check-ignore -v credentials.txt .env .env.local` → exit 1, nothing ignored.
  - GREEN: same command → exit 0. Commit `963a044`, PR #11 merged as `618d417`.
  - Review assess: medium, `under_budget` (no native review due).
- [x] E-3 — Bound `readLine()` in provisioning with the existing activity timeout + hard cap.
  - Route: inline (one header + one source + tests, already understood).
  - RED: `pio test -e native -f test_timing` → compile error, `isAcceptedLineChar` is not a
    member of `ProvisioningTimeout`.
  - GREEN: 27/27 in `test_timing`, 91/91 native. Commit `c7658c0`, PR #12 merged as `ad76f56`.
  - Wiring in `Provisioning.cpp` is compile-verified only (no native harness for the loop).
- [x] E-4 — Keep every watchdog-fed segment of the MQTT connect well under the WDT,
  including DNS.
  - Route: inline (understood after mapping the framework sources).
  - Finding: `WiFiClientSecure::connect(host)` resolves through `WiFi.hostByName()`, which
    waits up to 15 s (`WiFiGeneric.cpp:1578`). The unsplit attempt (DNS 15 + TCP 5 + TLS 10
    + CONNECT/CONNACK 10 + publish/subscribe writes 10) can block ~50 s on one feed.
  - Change: `ConnectSequence` stages (ResolveHost, OpenTls, MqttHandshake, Announce), WDT fed
    before each; TLS opened by IP + host (the framework's own two-step), PubSubClient reuses it.
  - RED: `fatal error: transport/ConnectSequence.h: No such file or directory`.
  - GREEN: 31/31 in `test_timing`, 95/95 native. Commits `b3f53f9`, `5c3f702`, PR #13 merged
    as `617fa58`.
  - Native review: slice `4d45e02..b3f53f9` reached the delivery budget (406 lines,
    `slice_budget_reached`), consent granted (pre-authorized), lens `review-reliability`,
    approved and acknowledged (`review-806f7436ce27a7bf`, authority burned). Advisory finding
    on the hidden PubSubClient reconnect and the dropped `setCACert()` fixed in `5c3f702`;
    the `readLine()` caller-contract finding checked and not applicable (`editCredential()`
    uses a local buffer and keeps the value on `false`).
  - Hardware only: real timing of each stage, TLS crypto time inside OpenTls.
- [x] E-7 — Offline restart must follow the selected transport, not always MQTT.
  - Route: inline.
  - Change: `ITransport::millisOffline()` (WiFi + MQTT: previous clock; LoRaWAN stub: 0) and
    `core/OfflineRestart.h` (30 min threshold); `main.cpp` reads the selected transport.
  - RED 1: `fatal error: core/OfflineRestart.h: No such file or directory`.
  - RED 2: `'class LoRaWANTransport' has no member named 'millisOffline'`.
  - GREEN: new suite `test_transport` 4/4, 99/99 native (12 suites). Commit `eb2fe8e`.
  - Review assess since the last reviewed boundary `b3f53f9`: medium, 133 lines,
    `under_budget` — `5c3f702` and E-7 remain an unreviewed slice under the budget.

## Progress and next step

All five items implemented. Remaining: hardware validation (see each PR's
"Requires hardware validation" section).
