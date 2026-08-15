# Contributing

This is a single-maintainer project. Issues and pull requests are welcome, but
please open an issue before starting anything substantial — much of the roadmap
depends on hardware that is not easy to work against remotely.

## Building

```bash
cmake -B build && cmake --build build
pip install -r requirements.txt
```

The C++ builds anywhere with a C++17 compiler. The interface needs PyQt5 and a
display. The reader loop needs the IDTech SDK and the physical reader, so that
path can only be tested on a real unit.

## Testing without hardware

Most of the system can be exercised without a reader:

```bash
mkfifo /tmp/bus_payment_control
python3 src/gui/card_gui.py &
./build/fifo_writer                        # inject a synthetic card
echo '0' > /tmp/bus_payment_control        # approve
echo '1' > /tmp/bus_payment_control        # decline
echo 'exit' > /tmp/bus_payment_control     # shut down
```

`parse_card_data` can be run against a saved hex capture by writing it to
`/tmp/card_data_hex.txt` first. Point it at a local endpoint rather than the
production one.

## Style

`.editorconfig` covers indentation and line endings. Beyond that, match the file
you are editing.

Keep comments to the ones that say why. The code says what.

## Commits

Write the subject as an instruction, in the imperative, under about 70
characters:

```
Mask the pan before it reaches the log
```

not `masking fix` or `Updated logging`. If the change needs justifying, put the
reasoning in the body — what was wrong before, and why this is the fix.

One logical change per commit. A restructure and a behaviour change in the same
commit cannot be reviewed or reverted independently.

## Pull requests

Branch from `main`, and say in the description what you changed, why, and how
you tested it. If you tested on hardware, say which reader and which display —
the two are the usual source of behaviour that does not reproduce.

## Card data

Never commit a real pan, and never paste one into an issue or a pull request.
Use test card numbers. If you attach a log, scrub it first —
`/tmp/api_log.txt` currently contains unmasked pans.
