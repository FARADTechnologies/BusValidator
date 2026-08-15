# Changelog

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and
versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Repository structure: sources grouped under `src/` by process, deployment
  material under `deploy/`, configuration under `config/`.
- Documentation set under `docs/` covering architecture, hardware, deployment,
  the payment flow, security notes and the roadmap.
- CMake build for both C++ targets, and a pinned PyQt5 requirement.
- `config/validator.env.example` documenting every path and endpoint the system
  depends on.
- Apache 2.0 licence.
- Security policy, contribution guide and this changelog.
- Continuous integration: C++ build, Python and shell linting on every push.
- Security note on the sudoers fragment explaining why the rule it contains is
  effectively root, and how to remove the need for it.

### Changed

- `idtech-expect` renamed to
  `deploy/sudoers.d/busvalidator-reader.example`; its previous name did not
  indicate that it was a sudoers fragment.
- README rewritten from a title and two screenshots to a full description of the
  system, its hardware and its deployment.

## [0.1.0] - 2025-12-25

First working prototype on a Raspberry Pi 5 with an IDTech Kiosk V.

### Added

- Contactless EMV card reading through the IDTech SDK, driven by an expect
  script that captures the tag data from the vendor demo binary.
- EMV parsing for both response shapes: TLV blobs carrying tags `5A` and `5F24`,
  and hex-encoded track data.
- Payment backend authorisation over HTTPS, with the outcome reported back to
  the interface.
- Kiosk interface in PyQt5: fare display, card and expiry, processing spinner,
  and approved and declined states, rendered rotated for a portrait-mounted
  panel.
- FIFO protocol between the reader pipeline and the interface, with non-blocking
  writes and retry so a payment is never held up by the display.
- systemd unit and startup script with process cleanup, FIFO recreation and
  ordered startup.
