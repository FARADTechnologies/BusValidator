# Bus Validator

An open-loop fare collection terminal for public transport. A passenger taps a
bank card, phone or watch on the unit and boards — no transit card, no top-up
kiosk, no cash handed to the driver.

The unit reads the card over NFC, extracts the pan and expiry from the EMV
response, authorises against the payment backend and shows the result in under
a second. When the bus has no cellular coverage the transaction is accepted
offline and settled later, so a tunnel or a dead spot never stops the queue.

```
   [ passenger taps ]
           |
   IDTech Kiosk V reader  --USB--> run_ctls_read.exp
                                        | /tmp/card_data_hex.txt
                                        v
                                  parse_card_data ──HTTPS──> payment backend
                                        |                          |
                                        | PAN:...;EXP:...          | approved -> 0
                                        |                          | declined -> 1
                                        v                          v
                              /tmp/bus_payment_control  (FIFO)
                                        |
                                        v
                                   card_gui.py
                              43H-800480-IPS-CT display
```

Two processes, one pipe. The reader side is C++ and expect because it has to sit
on a vendor SDK and answer a terminal menu; the passenger-facing side is Python
and Qt because it only has to draw. Neither can wedge the other: the UI is the
single reader on the FIFO, the reader side writes and moves on.

## Background

Until recently, buses and the metro in Azerbaijan accepted one thing: BakıKart,
a closed-loop transit card you bought once and topped up at a machine. Forget to
top it up and you do not travel. Arrive without the card and you ask another
passenger to tap for you and hand them cash.

Open-loop ticketing removes the intermediate card entirely — the bank card the
passenger already carries becomes the ticket. This project is the vehicle-side
terminal for that model, built for the Azerbaijani network.

## Capabilities

| Capability | Status |
| --- | --- |
| EMV contactless (Visa, Mastercard) | Implemented |
| Google Pay and Apple Pay over NFC | Implemented |
| Kiosk user interface | Implemented |
| Payment backend authorisation | Implemented |
| Offline store-and-forward with card blacklisting | Implemented |
| QR ticket validation | In development |
| Passenger counting from the onboard camera | In development |
| GPS telemetry and route analytics | In development |
| In-ride advertising on the display and speaker | Planned |
| Face-based payment | Research |

Source for several subsystems is being prepared for publication and is not yet
part of this repository.

## Demonstrations

| Recording | Shows |
| --- | --- |
| [NFC card payment](https://www.youtube.com/watch?v=NH2q4bXe6AA) | Tapping a contactless bank card, authorisation, result screen |
| [Google Pay payment](https://www.youtube.com/watch?v=XWXjQ4tmaNY) | Paying from a phone wallet |

## Hardware

| Part | Role |
| --- | --- |
| Raspberry Pi 5 | Application processor. To be replaced by a cost-reduced board in production |
| IDTech Kiosk V | Contactless EMV reader, the certified element of the payment path |
| 43H-800480-IPS-CT | 4.3 in 800x480 IPS display, mounted portrait. 7 in or 10 in in production |
| HD camera module | Passenger counting, QR ticket capture, biometric research |
| IR illumination ring | Keeps the camera usable after dark |
| SimCom A7608H | LTE Cat-4 modem with built-in multi-constellation GNSS |
| Active GNSS antenna | Position and route timing |
| ST ASM330LHHXG1 | Automotive-grade 6-axis IMU, dead reckoning through tunnels |
| ST IIS2MDC | 3-axis magnetometer, fused with the IMU for heading |
| Sensirion SHT45 | Humidity and temperature |
| Bosch BMP390 | Barometric pressure, also the vertical channel of the fusion filter |
| Bosch BME688 | Cabin gas and air quality |

Sensor choices and the reasoning behind each are in
[docs/HARDWARE.md](docs/HARDWARE.md).

## Repository layout

```
src/
  gui/         card_gui.py        kiosk interface, reads the FIFO
  reader/      run_ctls_read.exp  drives the IDTech SDK, captures card hex
               parse_card_data.cpp  parses EMV, authorises, reports the result
  tools/       fifo_writer.cpp    bench tool, exercises the UI with no reader
deploy/
  systemd/     bus-kiosk.service  starts the kiosk at boot
  scripts/     start_all.sh       process supervision and FIFO setup
  sudoers.d/   reader privilege fragment, see the security note inside it
config/
  validator.env.example  copy to validator.env and fill in
docs/          architecture, hardware, deployment, payment flow, security
```

## Getting started

### Prerequisites

Raspberry Pi OS (64-bit) on a Pi 5, an IDTech Kiosk V with its SDK bundle, and:

```bash
sudo apt install build-essential cmake expect python3-pyqt5 curl
```

The IDTech SDK is distributed by the vendor and is not redistributable, so it is
not in this repository. Unpack it on the device and point
`VALIDATOR_IDTECH_DIR` at the `Demo/aarch64` directory inside it.

### Build

```bash
cmake -B build
cmake --build build
```

Produces `build/parse_card_data` and `build/fifo_writer`.

### Configure

```bash
cp config/validator.env.example config/validator.env
```

Set the payment endpoint, the kiosk account and the SDK path. `validator.env` is
git-ignored — it names the production endpoint and must not be committed.

### Run

```bash
./deploy/scripts/start_all.sh
```

The script clears any previous run, recreates the FIFO, brings the UI up first
so there is a reader on the pipe, then starts the card loop.

To drive the interface with no reader attached:

```bash
./build/fifo_writer                          # inject a synthetic card
echo '0' > /tmp/bus_payment_control          # approve
echo '1' > /tmp/bus_payment_control          # decline
```

### Install as a service

```bash
sudo cp deploy/systemd/bus-kiosk.service /etc/systemd/system/
sudo systemctl enable --now bus-kiosk
journalctl -u bus-kiosk -f
```

Full device setup, including the display rotation and the reader privilege
rule, is in [docs/DEPLOYMENT.md](docs/DEPLOYMENT.md).

## Payment flow

A tap resolves in one of three ways.

**Online, approved.** The reader returns the EMV response, `parse_card_data`
extracts the pan and expiry and posts them to the backend. A 200 puts a tick on
the screen. Typical end-to-end time is under a second, most of it network.

**Online, declined.** Anything other than a 2xx shows a cross. The passenger has
not been charged.

**Offline.** With no cellular link the transaction is accepted on the device and
queued. When coverage returns the queue is settled against the backend. A card
the backend then rejects — no funds, expired, blocked — is added to the local
blacklist and refused on any subsequent offline tap, until it settles its
outstanding balance while the unit is online.

That last case is the trade the model makes deliberately: a small, bounded risk
of an unfunded ride in exchange for never stalling boarding. The blacklist
bounds it to one ride per card.

[docs/PAYMENT_FLOW.md](docs/PAYMENT_FLOW.md) covers the EMV tag parsing, the
FIFO protocol and the offline state machine.

## Security

The prototype is a development build and is not a certified payment terminal.
The certified element of the payment path is the IDTech reader, which holds the
EMV kernel and the keys; the application code around it handles the extracted
pan only.

Known gaps in the current build, with the reasoning and the fix for each, are
documented in [docs/SECURITY_NOTES.md](docs/SECURITY_NOTES.md) rather than left
implicit. The short version: the pan reaches logs and the screen unmasked, the
backend call shells out to curl, and the reader loop runs under a broad sudo
rule. None of these belong in a production unit and all are tracked.

To report a vulnerability, see [SECURITY.md](SECURITY.md).

## Related projects

[Robot-Tracker](https://github.com/FARADTechnologies/Robot-Tracker) is the
positioning stack this project draws on — GNSS acquisition on the same A7608
modem family, quality filtering, and a store-and-forward buffer for coverage
gaps. The GPS telemetry listed above as in development is being built there
first and folded back in.

## Roadmap

Near term, in [docs/ROADMAP.md](docs/ROADMAP.md) with the detail:

- Move the pan out of logs and mask it on screen
- Replace the shelled-out curl with libcurl and drop the sudo rule for a udev rule
- Read configuration from `validator.env` instead of compiled-in constants
- Publish the QR, counting and telemetry modules
- English throughout — the interface and the logs are still Azerbaijani and Turkish

Further out: in-ride advertising, and face-based payment if the regulatory
position allows it.

## License

Apache 2.0 — see [LICENSE](LICENSE).

## Contact

Hasan Dadaszade — [LinkedIn](https://www.linkedin.com/in/hesen-dadaszade/) ·
[GitHub](https://github.com/FARADTechnologies)
