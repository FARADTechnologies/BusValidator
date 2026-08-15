# Media

Photographs and screenshots referenced from the README and the documentation.

## Files

| Filename | Content |
| --- | --- |
| `hero-device.jpg` | The assembled unit, plain background, even lighting |
| `enclosure-front.jpg` | Front face: display, reader, camera |
| `internals-wiring.jpg` | Interior, boards and wiring visible |
| `bench-test.jpg` | Bench setup during development |
| `field-test-bus.jpg` | The unit installed in a vehicle |
| `ui-idle.png` | Idle screen |
| `ui-success.png` | Approved payment |
| `ui-failure.png` | Declined payment |
| `demo.gif` | Short loop of a tap and result |

## Guidelines

Photographs at 1200 px on the long edge, screenshots at native resolution.
Keep each file under about 500 kB — a README that takes seconds to load on a
phone is a README that does not get read.

`demo.gif` should stay under 5 MB. Anything longer belongs on YouTube with a
link from the README, not committed here.

The social preview image set in the repository settings is a separate asset at
1280x640. It is what appears when the repository link is shared, so it is worth
producing deliberately rather than cropping one of the above.

## Before committing a photograph

Check each frame for card numbers, vehicle registration plates, faces of people
who have not agreed to appear, and anything readable on a screen in the
background — API endpoints, keys, terminal scrollback. A photograph of a bench
often catches a monitor.

Card numbers are the one to be strict about: a real pan visible in a photograph
is a disclosure, and a committed image is difficult to retract.
