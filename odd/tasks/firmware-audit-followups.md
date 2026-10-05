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
- [ ] E-2 — Ignore `credentials.txt` and `.env` files.
  - Route: inline (one mechanical file).
  - RED: `git check-ignore -v credentials.txt .env .env.local` → exit 1, nothing ignored.
- [ ] E-3 — Bound `readLine()` in provisioning with the existing activity timeout + hard cap.
  - Route: inline (one header + one source + tests, already understood).
- [ ] E-4 — Keep every watchdog-fed segment of the MQTT connect well under the WDT,
  including DNS.
  - Route: inline (understood after mapping the framework sources).
- [ ] E-7 — Offline restart must follow the selected transport, not always MQTT.
  - Route: inline.

## Progress and next step

Next: E-2 PR.
