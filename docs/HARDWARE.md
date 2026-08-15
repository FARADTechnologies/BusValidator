# Hardware

## Bill of materials

| Part | Role | Notes |
| --- | --- | --- |
| Raspberry Pi 5 | Application processor | Prototype choice. Production moves to a cost-reduced board |
| IDTech Kiosk V | Contactless EMV reader | The certified element of the payment path |
| 43H-800480-IPS-CT | 4.3 in 800x480 IPS display | Mounted portrait, rotated in software |
| HD camera module | Passenger counting, QR capture | Also the sensor for biometric research |
| IR illumination ring | Night operation | Camera stays usable with the cabin lights off |
| SimCom A7608H | LTE Cat-4 modem with GNSS | Same family as the Robot-Tracker device |
| Active GNSS antenna | Position fix | Needs a ground plane, see below |
| ST ASM330LHHXG1 | 6-axis IMU | Automotive AEC-Q100 |
| ST IIS2MDC | 3-axis magnetometer | Heading reference for the fusion filter |
| Sensirion SHT45 | Humidity and temperature | Condensation-tolerant |
| Bosch BMP390 | Barometric pressure | Vertical channel of the fusion filter |
| Bosch BME688 | Gas and air quality | Cabin environment logging |
| Regulation | 5 V rail from vehicle supply | Bus supply is noisy and transient-prone |

## Why these sensors

Each part was picked for the one thing it is best at, rather than taking a
single combined sensor and accepting its weakest axis.

### Inertial: ASM330LHHXG1

The requirement is dead reckoning that stays usable while GNSS is blocked — in
tunnels, under overpasses, and in the urban canyons where a bus spends most of
its route. That is a different problem from the one a consumer IMU is designed
for, and the constraints that follow are specific:

- **Automotive qualification.** AEC-Q100 Grade 3, and rated for a 10-year
  supply life. A terminal fitted to a fleet has to be buildable in five years
  with the same part.
- **Bias stability over temperature.** Dead reckoning integrates gyro bias
  twice into position error. A part whose bias wanders with cabin temperature
  produces drift that grows with the square of time in the tunnel.
- **Vibration rejection.** A bus chassis is a poor mounting surface. Rectified
  vibration reads as a constant acceleration, which integrates into a phantom
  velocity.

The ASM330LHHXG1 is built for vehicle dead reckoning specifically, which is why
it was chosen over the higher-raw-performance alternatives evaluated below.

### Magnetic: IIS2MDC

A 6-axis IMU has no absolute heading reference: yaw is an integral, and it
drifts. The magnetometer bounds it. Fused, the two give a heading that neither
provides alone — the gyro supplies the short-term rate the magnetometer is too
slow and too noisy to track, the magnetometer supplies the long-term absolute
reference the gyro lacks.

Mounting matters more than the part. A magnetometer near the LTE antenna, the
display backlight or a DC-DC converter reads their fields, not the Earth's.
Hard-iron and soft-iron calibration is done in place, with the enclosure closed
and the unit powered, because the distortion being corrected is the unit's own.

### Barometric: BMP390

Pressure is the third leg of the fusion filter, not a weather instrument.

Fusing barometric altitude with the inertial solution constrains the vertical
channel, which is the axis a 6-axis dead reckoner is worst at: gravity dominates
the accelerometer's Z reading, so a small attitude error leaks a large false
vertical acceleration. A pressure reference stops that error accumulating.

The result is a 3D solution. Route work is normally 2D, but the vertical channel
earns its place twice: it improves the horizontal solution by absorbing errors
that would otherwise contaminate it during climbs and descents, and it gives the
mapping work a Z axis to build on later.

### Humidity and temperature: SHT45

The unit sits in an unconditioned cabin through Azerbaijani winters and
summers. Condensation on a reader or a display is a failure, so the humidity
channel is a health signal, not telemetry.

The SHT45 was chosen over a combined environmental sensor for accuracy —
typically ±1% RH against the ±3% of the integrated parts — and for surviving
condensing conditions rather than merely reporting them.

### Gas: BME688

Cabin air quality and occupancy correlation. It is the one sensor here whose
output is a product feature rather than a system input.

## Inertial alternatives evaluated

| Part | Axes | Class | Why not |
| --- | --- | --- | --- |
| ST ASM330LHHXG1 | 6 | AEC-Q100 automotive | Chosen |
| Bosch BMI088 | 6 | Industrial | Excellent vibration rejection, automotive-derived gyro. Not itself automotive-qualified |
| Bosch BMI090L | 6 | AEC-Q100 | The qualified BMI088. Close second |
| Bosch BNO055 | 9 | Consumer plus | On-chip fusion makes integration trivial, but the fusion is a black box and its bias stability is short of the requirement |
| Bosch BNO085 | 9 | Industrial | Better fusion than the BNO055. Still not automotive-qualified |
| Bosch BMX160 | 9 | Consumer | End of life, unsuitable for a new design |
| TDK ICM-42688-P | 6 | High performance | Lowest noise density of the group. Consumer qualification |
| TDK IAM-20680HP | 6 | AEC-Q100 | Viable alternative |
| Murata SCHA63T | 6 | Heavy duty | Outstanding robustness, used in construction machinery. Cost is an order above the requirement |
| ADI ADIS16505 | 6 | Tactical | Factory-calibrated, lowest drift available. Cost rules it out for a fleet unit |

The pattern in that table is that raw noise performance was not the deciding
factor. A part with lower noise and worse bias stability produces a worse dead
reckoning solution, because the filter can average noise and cannot average
bias.

## Environmental alternatives evaluated

| Part | Measures | Why not |
| --- | --- | --- |
| Sensirion SHT45 | RH, temperature | Chosen |
| Bosch BMP390 | Pressure, temperature | Chosen |
| Bosch BME688 | RH, T, P, gas | Chosen for gas only |
| Bosch BME280 | RH, T, P | One part instead of three, but weakest on all three channels |
| Bosch BME680 | RH, T, P, gas | Superseded by the BME688 |

The BME280 is the obvious single-chip answer and was rejected on purpose. Three
specialised parts cost more board area and one more bus address each, and return
a better number on every channel that feeds the fusion filter.

## GNSS antenna

The same constraint as on the Robot-Tracker unit applies here, and it is the
cheapest accuracy improvement available.

A ceramic patch antenna needs a ground plane. On a bare breakout the plane is
the small PCB, which costs several dB of gain and, more importantly, degrades
the antenna's right-hand circular polarisation towards linear. That polarisation
is what rejects reflected signals, so a small plane directly worsens multipath —
exactly the error that dominates in the built-up areas a bus route runs through.

Mount the patch on a metal plate of at least 70 x 70 mm, ceramic facing the sky,
away from the vehicle body edge.

The A7608H feeds DC bias to an active antenna over the coax. The bias voltage
also sets the LNA gain on most active antennas, and the module wants under 18 dB
total at its port — subtract cable loss, roughly 1 dB/m for RG174 at 1.5 GHz,
and drop the bias if the result still overshoots.

See [Robot-Tracker's hardware notes](https://github.com/FARADTechnologies/Robot-Tracker/blob/main/docs/hardware.md)
for the antenna and modem detail worked out on that device.

## Power

Vehicle supply is the harshest part of the environment. A bus 24 V rail carries
cranking dips, load-dump transients and alternator noise, none of which a bench
supply prepares a design for.

The prototype takes the vehicle rail through regulation to a 5 V bus. Two
requirements follow from the peripherals:

- The modem's LTE transmit bursts draw around 2 A for a few milliseconds. That
  current must not flow through the processor board — split the rail at the
  regulator output and run separate pairs, as on the Robot-Tracker unit.
- Bulk capacitance near the modem, in the order of 1000 µF, keeps those bursts
  from dipping the rail. Without it the symptom is an unexplained processor
  reboot under load, which reads as a software fault and is not one.

## Production changes

| Prototype | Production |
| --- | --- |
| Raspberry Pi 5 | Cost-reduced board, locally manufactured |
| 4.3 in display | 7 in or 10 in |
| Bench wiring | Single carrier PCB |
| Open frame | Sealed enclosure, vehicle-mount bracket |
