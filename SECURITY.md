# Security policy

## Reporting a vulnerability

Do not open a public issue. This project handles payment card data, and a public
report is a disclosure before there is a fix.

Use GitHub's private vulnerability reporting on this repository
(**Security** tab, then **Report a vulnerability**), or reach the maintainer
through [LinkedIn](https://www.linkedin.com/in/hesen-dadaszade/).

Useful in a report:

- What an attacker gains, and what access they need to start
- The steps to reproduce it
- The commit or version you tested

Expect an acknowledgement within a few days. This is a single-maintainer
project, so please allow reasonable time for a fix before disclosing publicly.

## Supported versions

The prototype is under active documentation and hardening work. Only the current
`main` receives fixes.

## Known issues

The current build has documented gaps — an unmasked pan in logs and on screen, a
shell-based backend call, and a broad sudo rule for the reader loop. These are
described with their fixes in [docs/SECURITY_NOTES.md](docs/SECURITY_NOTES.md)
and tracked in [docs/ROADMAP.md](docs/ROADMAP.md).

They are already known. A report restating them is welcome but not needed; a
report showing one of them is worse than documented very much is.

## Scope

In scope: the code in this repository and the deployment configuration under
`deploy/`.

Out of scope: the IDTech reader firmware and its EMV kernel, which are the
vendor's, and the payment backend, which is not part of this repository.

## Handling card data

If a report requires card data to demonstrate, use a test card. Do not send a
real pan — not in an issue, not in a private report, and not in an attached log.
`/tmp/api_log.txt` currently contains unmasked pans; scrub it before sharing.
