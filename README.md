# term-fix (`Yaynu::Term`)

Low-level terminal-control primitives for Fix TUI applications.
This is the foundation layer that future widget / layout libraries
(`tui-fix`) and the Yaynu application itself will build on.

> **Namespace note**: v0.1 ships under `Yaynu::Term::*` as a part of
> the Yaynu project. Standalone re-release under bare `Term::*` (and
> a separate git repository) is planned for a later version. API
> shapes are designed to be Yaynu-agnostic so the future rename is
> mechanical.

## Scope

In scope (v0.1):

- ANSI escape-sequence builders — 16-colour, 256-colour, 24-bit RGB,
  cursor positioning, screen clearing, text styles, alternate screen.
- `termios` raw-mode entry / exit, with exception-safe restoration
  via `with_raw`.
- Terminal size via `ioctl(TIOCGWINSZ)` plus `SIGWINCH`-driven resize
  detection (signal-handler-safe: flag setting only).
- Single-byte reads from stdin with timeout (`select(2)`-based).
- Pure state-machine parser converting input bytes into logical
  `Key` values (UTF-8 1..4-byte, CSI / SS3 escape sequences,
  Ctrl / Alt modifiers).
- Stdout writers with explicit flush, including a batching variant
  that issues exactly one underlying write (no tearing).

Out of scope:

- Widgets, layout, diff rendering — that belongs in a higher layer
  (`tui-fix`).
- Windows support — `termios` / `ioctl` differ; revisit in v0.2.
- Terminal capability detection — always assumes ANSI + UTF-8.
- Mouse input — to be considered in v0.2.

## Supported platforms

- Linux
- macOS

Other POSIX systems are likely to work but are untested.

## Dependencies

Only `Std`. No transitive Fix dependencies. The build process
additionally requires a system C compiler (`cc`) and `ar` for the
C shim.

## Building

The C shim is compiled automatically as a `preliminary_command` in
`fixproj.toml`:

```sh
fix check        # type-check (fast)
fix test         # run unit tests for Ansi + Key
```

If you want to rebuild the C shim manually:

```sh
./scripts/build_shim.sh
```

This produces `c_src/libterm_shim.a`, which `fixproj.toml` links
statically.

## Quick start

```fix
import Yaynu.Term.Ansi;
import Yaynu.Term.Key;
import Yaynu.Term.Mode;
import Yaynu.Term.Input;
import Yaynu.Term.Output;

main : IO ();
main = (
    let result = *Mode::with_raw(|_|
        Output::write("Press any key (Ctrl+C to exit)\r\n");;
        loop_m([] : Array U8, |buf|
            match Key::parse(buf) {
                complete((k, n)) => (
                    Output::write(Key::to_string(k) + "\r\n");;
                    match k {
                        ctrl(s) => if s == "c" { break_m $ () }
                                   else { continue_m $ buf.subarray(n, buf.get_size) },
                        _       => continue_m $ buf.subarray(n, buf.get_size)
                    }
                ),
                incomplete() => (
                    let b = *Input::read_byte_timeout(80);
                    match b {
                        some(byte) => continue_m $ buf.push_back(byte),
                        none()     => continue_m $ buf
                    }
                ),
                invalid(n) => continue_m $ buf.subarray(n, buf.get_size)
            }
        )
    ).to_result;
    match result {
        ok(_)  => pure(),
        err(e) => eprintln("failed: " + e)
    }
);
```

A full, self-contained version of this loop is in
[examples/echo_keys.fix](examples/echo_keys.fix).

## Why prefer `with_raw`

`enter_raw` / `restore` are exposed as the building blocks, but
**always reach for `with_raw` first**. It guarantees the saved
termios is reapplied even if the inner action fails — without that,
an unhandled error in your TUI leaves the terminal in raw mode and
the shell unusable until you `reset(1)`. `with_raw` propagates the
inner action's error in preference to any `restore` error so the
user-visible cause of failure is the actual bug, not a cleanup
hiccup.

## ESC timeout

`Key::parse` is a pure state machine and does not measure time.
A bare ESC byte returns `incomplete()` because the parser cannot
tell whether it is the start of a longer escape sequence (Alt+key,
arrow key, CSI ...) or a real Escape press.

The caller resolves this ambiguity with a timeout:

1. Read bytes into a buffer; feed the buffer to `parse`.
2. If the result is `incomplete()` and the buffer is exactly
   `[0x1B]`, wait for the next byte with a short timeout (the
   xterm convention is 50–100 ms; the example uses 80 ms).
3. If the timeout fires with the buffer still `[0x1B]`, treat
   that as an Escape press by emitting `Key::escape_from_timeout`
   (which is just `Key::escape()`) and clearing the buffer.
4. Otherwise, append the byte and re-run `parse`.

This responsibility lives in the application loop, not in the
library, so that `Key::parse` stays a pure function.

## Examples

Run any example with `fix run -f examples/<name>.fix`. All require
a TTY for stdout (and stdin for `echo_keys`).

- [examples/colors.fix](examples/colors.fix) — 16 / 256 / RGB
  colour swatches.
- [examples/cursor.fix](examples/cursor.fix) — `move_to`,
  `hide_cursor` / `show_cursor`, `clear_screen`.
- [examples/show_size.fix](examples/show_size.fix) — polls
  `check_resized` and re-reads `TermSize::get` on each resize.
  Resize the terminal window to see updates.
- [examples/echo_keys.fix](examples/echo_keys.fix) — enters
  `enter_raw_no_sig` mode and echoes parsed `Key`s. Press Ctrl+C
  to exit; the terminal is restored automatically.

## Limitations

- Linux / macOS only. Windows is not supported in v0.1.
- `enter_raw` flavours require stdin to be a TTY. `IOFail` errors
  surface clearly when it is not.
- Modified-arrow sequences (`ESC [ 1 ; <mod> A`) are not recognised
  in v0.1 and surface as `invalid`. Plain arrows and the
  documented xterm-flavoured escape table work.

## License

MIT.
