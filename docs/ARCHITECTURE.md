# Architecture

## Process model

The validator runs as three cooperating processes rather than one program.

```
  IDTechSDK_Demo            vendor binary, closed source, menu-driven on stdin
        ^  stdout
        |  stdin
  run_ctls_read.exp         expect: answers the menu, scrapes the card hex
        |
        |  writes /tmp/card_data_hex.txt, then forks:
        v
  parse_card_data           C++: parse -> authorise -> report
        |
        |  writes /tmp/bus_payment_control
        v
  card_gui.py               PyQt5: the only reader on the pipe
```

The split is forced by the reader SDK. IDTech ships a demo binary that drives
the Kiosk V over USB and prints tag data to stdout, but it is interactive: it
expects menu selections on stdin and has no library entry point exposed to us.
Something has to sit in front of it and type. That is `run_ctls_read.exp`, and
expect is the right tool for exactly that job.

Once the card hex is on disk the rest is ordinary programming, so it moves into
C++. The passenger-facing screen is a third process because it must stay
responsive while a payment is in flight — a blocking HTTPS call in the same
event loop would freeze the spinner it is supposed to be animating.

## Why a FIFO

The two halves communicate over a named pipe at `/tmp/bus_payment_control`.

A pipe was chosen over a socket or a message queue because the traffic is
trivial — a few dozen bytes per tap, one direction, one reader — and because a
FIFO gives the ordering guarantee needed for free: writes below `PIPE_BUF`
(4096 bytes on Linux) are atomic, so two writers cannot interleave a line. Both
the pan record and the result token are well under that.

The protocol is line-based:

| Line | Meaning |
| --- | --- |
| `PAN:<digits>;EXP:<YYMM>` | A card was read. Show it, start the spinner |
| `0` | Backend approved. Show the tick |
| `1` | Backend declined, or the pipeline failed. Show the cross |
| `exit` | Shut the interface down |

`card_gui.py` is the single reader. `start_all.sh` brings it up before the
reader loop for that reason: a FIFO write with no reader attached fails with
`ENXIO`, and the ordering makes that the normal case rather than a race.

The writers do not assume the reader is there anyway. Both `writeToFIFO` and
`sendToGUI` open the pipe `O_WRONLY | O_NONBLOCK` and retry every 100 ms for up
to 3 seconds. If the UI has crashed and systemd is restarting it, the payment
still completes and only the display is late.

## Startup ordering

`start_all.sh` runs a fixed sequence, and each step exists because of a specific
failure seen on the bench:

1. **Kill the previous run.** `pkill` on the SDK demo, the expect script and the
   UI. A second reader on the FIFO steals half the lines and the screen goes
   incoherent.
2. **Recreate the FIFO.** `rm -f` then `mkfifo`. A stale pipe left by a crashed
   run keeps its buffered contents; the new UI would come up and immediately
   replay the last tap.
3. **Start the UI first**, so there is a reader on the pipe before anything
   writes to it.
4. **Wait one second** for Qt to open the display and the FIFO reader thread to
   enter its blocking read.
5. **Start the reader loop** under sudo, in the foreground. It is the last step
   so that the systemd unit's lifetime tracks the card loop: if the reader dies,
   the unit dies and `Restart=always` brings the whole stack back.

## The reader loop

`run_ctls_read.exp` answers the SDK menu once at startup (selections `2`, `1`,
`2` reach the contactless transaction submenu) and then loops forever:

- Send `1` to arm a contactless transaction.
- Watch stdout for up to 20 seconds for either tag pattern.
- On a match, write the hex to `/tmp/card_data_hex.txt` and fork
  `parse_card_data` in the background so the loop can re-arm immediately.
- Send `2` to cancel the transaction, then arm again.

The 20-second window and the explicit cancel matter: the reader will not accept
a second card while a transaction is still open, so an uncancelled timeout
silently stops taking payments while looking alive.

Two capture patterns are handled because the SDK emits different shapes
depending on the card and the configuration:

| Pattern | Content |
| --- | --- |
| `DFEF4D:<hex>` | Hex-encoded ASCII track data, track 1 in the clear |
| `C1DFEE<hex>` | A TLV blob carrying EMV tags directly |

The expect script captures group 1 for `DFEF4D` and group 0 for `C1DFEE` — the
whole match, prefix included — because `parse_card_data` decides which parser to
run by testing the first six characters of the file for `C1DFEE`.

## Display

The 43H-800480-IPS-CT is an 800x480 landscape panel mounted portrait, so the
interface is rendered landscape and rotated in software.

`RotatedWidget.paintEvent` translates to the widget centre, rotates the painter
90 degrees, translates back by the swapped dimensions and renders the real
widget into it. Rotating in the compositor or via `xrandr` was avoided because
the Qt widget then lays out against the wrong aspect ratio; rotating at paint
time keeps layout in the panel's native geometry.

The cost is that every repaint renders the full widget tree through a transform.
At the update rates involved — a 120 ms spinner tick and a screen change per tap
— that is not a problem on a Pi 5.

## Failure behaviour

| Failure | Result |
| --- | --- |
| No card within 20 s | Transaction cancelled and re-armed, nothing on screen |
| Tag present, pan not parseable | `1` to the UI, cross shown, no backend call |
| Expiry missing or malformed | Pan shown, `1` to the UI, no backend call |
| Backend returns non-2xx | Cross shown |
| `curl` cannot run | Cross shown, `popen` failure logged |
| UI not running | Payment completes, FIFO writes retry for 3 s then log and give up |
| Reader loop dies | systemd restarts the whole stack after 2 s |

Every branch that ends a tap sends a token to the UI. There is no path that
leaves the spinner running forever — and if one appeared, the 5-second result
timer in `card_gui.py` returns the screen to idle regardless.

## What is not here yet

The camera, GNSS, IMU and environmental sensors are wired on the prototype but
their software is not in this repository. The intended shape is a fourth
process publishing telemetry, with positioning built first in
[Robot-Tracker](https://github.com/FARADTechnologies/Robot-Tracker) and folded
back in — the modem family and the store-and-forward problem are the same on
both.

The offline queue is the one part of the payment path that belongs inside
`parse_card_data` rather than beside it, because the decision to accept a tap
without a backend answer has to be made in the same code path that would
otherwise wait for one. See [PAYMENT_FLOW.md](PAYMENT_FLOW.md).
