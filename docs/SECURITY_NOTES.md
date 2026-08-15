# Security notes

This is a development build. It is documented here as it actually is, because a
payment terminal with undocumented gaps is worse than one with known ones.

## Scope of the trust boundary

The certified element of the payment path is the IDTech Kiosk V. It holds the
EMV kernel, the terminal keys and the contactless transaction logic, and it is
the part that carries certification. The application code in this repository
sits outside that boundary and handles what the reader hands back: a pan, an
expiry, and nothing else.

That matters for what follows. None of the issues below can forge a
transaction or extract a key from the reader. They are about what the
application does with the pan after the reader has finished with it, and about
the privileges the surrounding processes hold.

## Known gaps

### 1. The pan reaches logs unmasked

`parse_card_data` writes the extracted pan to `/tmp/api_log.txt` and to stdout,
which systemd captures into the unit log. The request body and the full `curl`
command line are logged too, and both contain the pan.

Storing a pan in cleartext logs is not acceptable in a production terminal. Card
data may only be retained masked, and logs are one of the places it most often
leaks by accident.

**Fix.** Mask everywhere the pan is logged, keeping the first six and last four
digits at most — enough to identify a card in support without reconstructing it.
Stop logging the request body and the command line entirely.

### 2. The pan is displayed in full

`card_gui.py` renders the pan as returned. A passenger's full card number is
visible on a screen mounted at head height in a public vehicle, readable by
anyone standing behind them.

**Fix.** Mask on screen as well. The passenger only needs to recognise which
card was charged, which the last four digits give them.

### 3. The backend call shells out

The authorisation request is built as a shell command string and run through
`popen`:

```cpp
std::string command = "curl -s -w '%{http_code}' -X POST " + apiUrl +
                      " -H 'Content-Type: application/json' -d '" + jsonData + "'";
FILE* pipe = popen(command.c_str(), "r");
```

`jsonData` contains the pan and expiry, and on the track-data path those come
from `hexToAscii`, which decodes arbitrary bytes off the card. A card crafted to
contain a single quote closes the quoted argument and the remainder of the field
is interpreted by the shell, running as the kiosk account.

This is the most serious item on the list. It needs physical proximity and a
crafted card, but the payload is attacker-chosen and the process it lands in is
the one that talks to the payment backend.

**Fix.** Two layers. Validate first — the pan is digits and the expiry is four
digits, so reject anything else before it reaches a buffer. Then remove the
shell entirely by moving to libcurl, which also fixes the fragile
three-characters-off-the-end status parsing.

### 4. The production endpoint is compiled in

The authorisation URL is a default argument in the `CardDataProcessor`
constructor. Changing it means recompiling, and it is visible to anyone reading
the source.

**Fix.** Read it from `config/validator.env`, which is already git-ignored for
this purpose.

### 5. The reader loop runs under a broad sudo rule

The deployed sudoers fragment grants the kiosk account passwordless sudo to run
`expect` against a named script. Because expect can spawn a shell, that rule
grants effective root to anyone who can modify the script it names.

The privilege boundary it appears to draw is only real if the script is not
writable by the account the rule is granted to.

**Mitigation, if the rule is kept.**

```bash
sudo chown root:root /opt/busvalidator/src/reader/run_ctls_read.exp
sudo chmod 0644      /opt/busvalidator/src/reader/run_ctls_read.exp
```

**Fix.** Remove the rule. Root is only needed for raw USB access to the reader,
and a udev rule grants that to a group without any sudo at all — see
[DEPLOYMENT.md](DEPLOYMENT.md). This is the preferred path and the reason the
sudoers file ships as an example rather than as the documented default.

### 6. No application-layer authentication on the backend call

The request carries no key, token or client certificate. Either the endpoint
authenticates by another means outside this code, or it does not authenticate at
all — and from the terminal's side those are indistinguishable.

**Fix.** Mutual TLS is the right answer for a fixed fleet of devices: each unit
gets a client certificate, the backend refuses anything else, and a stolen unit
can be revoked individually.

## What the build does correctly

Worth stating, because these are the things most often got wrong:

- **TLS is verified.** The `curl` invocation does not pass `-k`. A terminal that
  disables certificate verification to make a bring-up problem go away is a
  terminal that accepts any proxy on the route.
- **No secrets in the repository.** No keys, tokens or credentials are
  committed, and no email addresses appear in the history — the commits use
  GitHub's no-reply address.
- **Ambiguity fails closed.** Every unclear outcome — unparseable response,
  `curl` failure, missing expiry — is treated as a decline. The terminal never
  admits a passenger because it could not tell.
- **The pan never persists to durable storage.** `/tmp` is a tmpfs on Raspberry
  Pi OS, so the log does not survive a reboot. That limits the exposure of
  item 1; it does not excuse it.
- **The certified reader holds the keys.** No key material is handled in this
  code, which is why the gaps above are contained.

## Reporting

Do not open a public issue for a vulnerability. See [SECURITY.md](../SECURITY.md).
