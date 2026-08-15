# Roadmap

Ordered by what blocks what, not by size.

## Hardening

The gaps documented in [SECURITY_NOTES.md](SECURITY_NOTES.md). Pan masking,
field validation and configuration have landed; what is left:

- **Replace the shelled-out curl with libcurl.** Validation closes the known
  injection path, but the shell is still in the request path. libcurl removes
  the category, and fixes the fragile status-code parsing in the same change.
- **Drop the sudo rule for a udev rule.** Raw USB access by group membership
  instead of passwordless root. The rule is written up in DEPLOYMENT.md; the
  default deployment has not moved to it.
- **Mutual TLS to the backend.** Per-unit client certificates, individually
  revocable.

## English throughout

The interface, the log messages and the code comments are in Azerbaijani and
Turkish. Everything becomes English, in one pass so the codebase is never half
converted:

- User-facing strings in `card_gui.py` — `Ödəmə Uğurludur`, `Ödəmə Uğursuzdur`,
  `Prosess Emal edilir`, the `YERI Validator` header.
- Console output in `card_gui.py` — the startup control hints and the FIFO
  command traces.
- Log messages in `parse_card_data.cpp` — every `logInfo`, `logError` and
  `logSuccess` string.
- Console output in `run_ctls_read.exp` and `start_all.sh`.
- Code comments across all five source files.

The interface strings move into a lookup table in the same pass rather than
being translated in place, so Azerbaijani returns as a selectable language
instead of being lost. English becomes the default and the source of truth.

## Card parsing

- **Walk the TLV properly.** Read the length byte instead of matching the
  literal `5A08`, so 13- and 19-digit pans parse. Strip the `F` nibble padding
  an odd-length pan.
- **Collapse the duplicated expiry reformat.** `YYMM` to `MM/YY` happens in both
  the C++ and the Python; it should happen once.
- **Reduce the doubled expiry field.** The request sends `expiryDate` and
  `expiry_date` for a backend contract that has since settled.
- **Retry with backoff.** A single failed request is currently a decline. A
  transient 5xx or a dropped connection deserves a retry before the passenger
  is refused.

## Modules to publish

Built or in progress on the prototype, not yet in this repository:

- **Offline store-and-forward queue and blacklist.** Belongs inside
  `parse_card_data`, in the same code path that would otherwise wait for the
  backend.
- **QR ticket validation.** Camera capture and decode, for the printed receipt
  issued to passengers boarding without a card or phone.
- **Passenger counting.** Camera-based, with the IR ring for night operation.
- **GPS telemetry and route analytics.** Positioning is being built first in
  [Robot-Tracker](https://github.com/FARADTechnologies/Robot-Tracker) — same
  modem family, same coverage-gap problem — and folded back in.

## Later

- **In-ride advertising** on the display and speaker.
- **Face-based payment.** Research only. Gated on the regulatory position and on
  explicit passenger consent, neither of which is settled.

## Documentation

- Photographs of the prototype, assembly and field testing.
- The demonstration recordings consolidated and captioned.
- A social preview image so the repository presents properly when shared.
