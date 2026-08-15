# Payment flow

## From tap to result

```
tap -> EMV response -> hex on disk -> parse -> authorise -> token -> screen
       ~200 ms         immediate      <1 ms    ~300-800 ms   <1 ms   next frame
```

The timings are indicative from bench runs. Nearly all of the wall time is the
backend round trip over LTE; the parsing is not measurable next to it.

## Extracting the pan

The reader returns one of two shapes and `parse_card_data` picks a parser by
testing the first six characters of the captured hex for `C1DFEE`.

### TLV path

The `C1DFEE` blob carries EMV tags in the usual tag-length-value encoding. Two
are needed:

| Tag | Length | Meaning |
| --- | --- | --- |
| `5A` | 8 bytes | Application PAN, BCD encoded |
| `5F24` | 3 bytes | Application expiry date, `YYMMDD` |

So the parser searches for the literal `5A08` and takes the next 16 hex
characters as the pan, and for `5F2403` and takes the next 4 as `YYMM`.

Hardcoding the length byte narrows what this accepts: `5A08` matches a 16-digit
pan only, and a 13- or 19-digit card encoded as `5A07` or `5A0A` would be
missed. Every card issued in Azerbaijan is 16 digits, so the prototype takes
that shortcut deliberately. It is the first thing to generalise if the terminal
is ever deployed outside that assumption — a proper TLV walk reading the length
byte rather than matching it, and stripping the `F` nibble that pads an
odd-length pan.

### Track path

Anything else is treated as hex-encoded ASCII track data. The bytes are decoded
two hex characters at a time, then track 1 is cut out between `;` and `?` and
split on `=`:

```
;4111111111111111=2512101...?
 ^--------------^ ^--^
        pan       YYMM
```

Malformed bytes are skipped rather than aborting the decode, because a single
bad character in a field that is not the pan should not cost the passenger a
ride.

### Expiry

Both paths yield `YYMM` and both reformat to `MM/YY` for the backend and the
screen. `2512` becomes `12/25`. The reformat happens twice — once in the C++
before the API call and once in `card_gui.py` for the display — which is
duplicated logic worth collapsing when the pan handling is reworked.

## Authorising

The request is a JSON POST:

```json
{
  "pan": "4111111111111111",
  "expiryDate": "12/25",
  "expiry_date": "12/25"
}
```

The expiry is sent twice under both spellings. That is not an accident of
generated code — it dates from bringing the terminal up against a backend whose
field name was in flux, and sending both made the terminal insensitive to which
side deployed first. It should be reduced to one field once the contract is
frozen.

A 200 or 201 sends `0` to the interface. Everything else — any other status, an
unparseable response, a `curl` that would not start — sends `1`. The terminal
treats every ambiguous outcome as a decline, which is the safe direction: a
passenger wrongly refused taps again, a passenger wrongly admitted rides free.

Response handling reads the body and status code from a single `curl -w
'%{http_code}'` capture, splitting the last three characters off the end. That
works because the status is written last, but it also means a response body
ending in three digits and a truncated read would be misparsed. Moving to
libcurl removes the whole class of problem.

## Offline acceptance

Cellular coverage on a moving bus is not continuous. A terminal that refuses to
work without it would stop boarding in every tunnel and every dead spot, which
is worse than the risk it avoids.

So when the backend is unreachable the tap is accepted locally and queued. On
reconnection the queue is settled in order. Most entries clear. Some do not —
insufficient funds, an expired card, a blocked account — and that card is then
added to a local blacklist.

A blacklisted card is refused on any subsequent offline tap. It is not refused
online: if the passenger has since covered the balance, the live authorisation
succeeds and the card is cleared. The blacklist gates the offline path only,
because that is the only path that extends credit.

The exposure is bounded by design. A card can take exactly one unfunded ride
before it is blacklisted, and only while the unit is offline. Set against the
cost of a bus that stops accepting passengers whenever it enters a tunnel, one
fare is the cheaper failure.

```
        tap
         |
   backend reachable?
     /          \
   yes           no
    |             |
 authorise    blacklisted?
    |           /     \
 approve      yes      no
 or decline    |        |
             refuse   accept, queue
                          |
                    coverage returns
                          |
                     settle queue
                       /      \
                  cleared    rejected
                     |          |
                   done     blacklist card
```

## FIFO protocol

`parse_card_data` writes two records per tap, in order:

```
PAN:4111111111111111;EXP:2512
0
```

The pan record comes first so the screen can show the card and start the spinner
while the backend call is in flight. The token follows when the answer arrives.

`card_gui.py` tolerates either order. If a token arrives before the pan record —
possible if the backend answers faster than the first FIFO write drains — it is
held in `last_status` and applied as soon as the card is shown. That was a real
bug on the bench, not a hypothetical: a fast local backend beat the display and
the tick was lost.

Both writers open the pipe non-blocking and retry for 3 seconds. A payment is
never held up by the screen.

## Screen states

| State | Shown |
| --- | --- |
| Idle | Fare and card artwork |
| Card read | Pan, expiry, spinner, "Prosess Emal edilir" |
| Approved | Green tick, "Ödəmə Uğurludur" |
| Declined | Red cross, "Ödəmə Uğursuzdur" |

Any result returns to idle after 5 seconds. The timer restarts when a result
lands, so a decline is readable for its full 5 seconds rather than inheriting
what was left of the card timer.

The interface is currently Azerbaijani. Translating it, and the logs behind it,
to English is tracked in [ROADMAP.md](ROADMAP.md).

## Handling the pan

The pan is currently written to the log at `/tmp/api_log.txt`, printed to stdout
and shown in full on the display. None of that is acceptable in a production
terminal and all of it is tracked in
[SECURITY_NOTES.md](SECURITY_NOTES.md) with the fix.
