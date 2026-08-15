# Deployment

Bringing a validator unit up from a fresh Raspberry Pi OS install.

## 1. Base system

Raspberry Pi OS (64-bit) on a Pi 5. The interface needs a desktop session
because it is an X11 Qt application — the Lite image will not do without adding
one.

```bash
sudo apt update
sudo apt install build-essential cmake expect python3-pyqt5 curl git
```

`expect` drives the reader SDK, `curl` is invoked by the authorisation code, and
`python3-pyqt5` comes from apt rather than pip because the packaged build is
already compiled for the Pi. Installing PyQt5 from pip pulls a source
distribution and builds Qt, which takes hours on the device and frequently runs
out of memory.

## 2. Account

The kiosk runs as its own unprivileged account rather than as `pi`:

```bash
sudo adduser --disabled-password --gecos "" validator
sudo usermod -aG video,plugdev validator
```

`video` for the display and camera, `plugdev` for USB device access.

## 3. Reader SDK

The IDTech SDK is supplied by the vendor with the Kiosk V and is not
redistributable, so it is not in this repository. Unpack it on the device:

```bash
sudo mkdir -p /opt/idtech
sudo tar xf 80144801-001-B.tar.gz -C /opt/idtech
sudo chown -R validator:validator /opt/idtech
```

The demo binary lives at `Demo/aarch64/IDTechSDK_Demo` and needs its own
directory on `LD_LIBRARY_PATH`; the shared objects sit beside the executable
rather than in a system path. `run_ctls_read.exp` sets that up in the shell it
spawns, so nothing needs to go in `/etc/ld.so.conf.d`.

Confirm the reader enumerates before going further:

```bash
lsusb | grep -i idtech
```

Nothing else will work until it does.

## 4. Application

```bash
sudo git clone https://github.com/FARADTechnologies/BusValidator.git /opt/busvalidator
sudo chown -R validator:validator /opt/busvalidator
cd /opt/busvalidator
cmake -B build && cmake --build build
```

Then the configuration:

```bash
cp config/validator.env.example config/validator.env
```

Set at minimum `VALIDATOR_API_URL`, `VALIDATOR_USER`, `VALIDATOR_HOME` and
`VALIDATOR_IDTECH_DIR`. The file is git-ignored and names the production
endpoint — it does not belong in a repository or in a support ticket.

Create the idle-screen artwork directory referenced by the interface:

```bash
mkdir -p /home/validator/assets
# place credit-card.png here
```

The interface degrades gracefully if the image is missing — the idle screen
simply shows the fare with no artwork — so an absent file is not a fault.

## 5. Reader privileges

The reader loop needs root for raw USB access. Two options, in order of
preference.

### Preferred: udev rule, no sudo

Grant the device to a group instead of granting root to a script:

```bash
sudo tee /etc/udev/rules.d/99-idtech.rules >/dev/null <<'EOF'
SUBSYSTEM=="usb", ATTRS{idVendor}=="0acd", MODE="0660", GROUP="plugdev"
EOF
sudo udevadm control --reload-rules && sudo udevadm trigger
```

Confirm the vendor id against `lsusb` output before relying on it. With the rule
in place, drop the `sudo` from the last line of `start_all.sh` and the sudoers
fragment is not needed at all.

### Fallback: sudoers fragment

```bash
sudo install -m 0440 -o root -g root \
    deploy/sudoers.d/busvalidator-reader.example \
    /etc/sudoers.d/busvalidator-reader
sudo visudo -c
```

Edit the installed copy to replace `VALIDATOR_USER` with the real account name
first. `visudo -c` must pass before you log out — a malformed sudoers file locks
sudo for everyone.

If you take this route, the script it names **must not be writable by the kiosk
account**, or the rule grants that account full root:

```bash
sudo chown root:root /opt/busvalidator/src/reader/run_ctls_read.exp
sudo chmod 0644      /opt/busvalidator/src/reader/run_ctls_read.exp
```

See [SECURITY_NOTES.md](SECURITY_NOTES.md).

## 6. Display

The panel is 800x480 landscape, mounted portrait. The rotation happens inside
the application, not in the display server — do not also rotate with `xrandr` or
`display_rotate`, or the two compose and the interface lands sideways.

Blank the screen saver and the power management that would otherwise turn the
display off mid-service. In the desktop autostart:

```
@xset s off
@xset -dpms
@xset s noblank
```

Hide the cursor with `unclutter` if the unit has no pointing device.

## 7. Service

```bash
sudo cp deploy/systemd/bus-kiosk.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now bus-kiosk
```

Adjust `User=`, `WorkingDirectory=` and `ExecStart=` in the unit to match the
account and install path chosen above.

The unit waits on `graphical.target` because the interface needs a display
server, and `network-online.target` because the first authorisation will fail
without a route. `Restart=always` with `RestartSec=2` brings the stack back
after any crash; because `start_all.sh` ends by running the reader loop in the
foreground, the unit's lifetime tracks the part that matters.

## 8. Verify

```bash
systemctl status bus-kiosk
journalctl -u bus-kiosk -f
```

The idle screen should show the fare. Then, without a card:

```bash
/opt/busvalidator/build/fifo_writer
echo '0' > /tmp/bus_payment_control
```

A card and then a green tick confirms the FIFO, the interface and the rotation
are all correct. Only then try a real tap.

## Logs

| Path | Contents |
| --- | --- |
| `$VALIDATOR_LOG_DIR/start_all.log` | Startup sequence |
| `$VALIDATOR_LOG_DIR/card_gui.service.log` | Interface stdout |
| `$VALIDATOR_LOG_DIR/card_reader_live.log` | Reader loop and SDK output |
| `/tmp/api_log.txt` | Authorisation attempts and timings |
| `journalctl -u bus-kiosk` | Unit lifecycle |

`/tmp/api_log.txt` currently contains unmasked pans and is cleared on reboot
only because `/tmp` is a tmpfs. Treat it as sensitive until the masking work in
[ROADMAP.md](ROADMAP.md) lands — do not attach it to a bug report.

## Troubleshooting

| Symptom | Cause |
| --- | --- |
| Blank screen, unit otherwise healthy | Qt has no display. Check `DISPLAY` and `XAUTHORITY` in `start_all.sh` match the logged-in session |
| Interface sideways or clipped | Rotation applied twice. Remove the `xrandr` or `display_rotate` setting |
| Taps do nothing, no log entries | Reader not enumerated. `lsusb`, then check cable and power |
| Card read, screen never updates | No reader on the FIFO. The interface died; check its log and that it starts before the reader loop |
| Every tap declines | Backend unreachable or wrong `VALIDATOR_API_URL`. `/tmp/api_log.txt` shows the status code |
| Screen replays the last tap at startup | Stale FIFO. `start_all.sh` recreates it; confirm the `rm -f` succeeded |
| Reader stops accepting after one card | Transaction not cancelled. The loop sends `2` after each attempt — check the expect log for a menu desync |
| Unit reboots under load | Rail dipping on LTE transmit bursts. See the power section of [HARDWARE.md](HARDWARE.md) |
