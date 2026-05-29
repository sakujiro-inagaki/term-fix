## Why

Shift+Tab is a common terminal input (used for reverse focus traversal, reverse autocomplete, etc.) and is emitted by xterm-compatible terminals as the standard CSI sequence `ESC [ Z` (terminfo `kcbt`). The current `parse` function falls through to the catch-all and reports this sequence as `invalid(3)`, so library users cannot distinguish Shift+Tab from other unrecognised CSI input. A library user has explicitly requested that the sequence be recognised as a dedicated logical key.

## What Changes

- Add a new `back_tab : ()` variant to the `Key` union.
- Recognise `ESC [ Z` (3-byte CSI with terminator `Z` / `0x5A`) as `complete(back_tab, 3)` instead of `invalid(3)`.
- Extend `to_string` so that `Key::back_tab()` renders as the string `"back_tab"`.
- Add scenarios to the `term-key` spec covering both the new variant and the new parse behaviour.

Non-breaking: existing variants and parse results are unchanged. Callers that previously observed `invalid(3)` for `ESC [ Z` will now observe `complete(back_tab, 3)`, which is a strict refinement (a previously-unrecognised input becomes recognised).

## Capabilities

### New Capabilities
<!-- none -->

### Modified Capabilities
- `term-key`: extend the `Key` union with a `back_tab` variant and extend the CSI 3-byte parser to map `ESC [ Z` → `complete(back_tab, 3)`. `to_string` must render `back_tab` distinctly.

## Impact

- Code: [src/Yaynu/Term/Key.fix](src/Yaynu/Term/Key.fix) — `Key` union, `_parse_csi`, `to_string`.
- Tests: [tests/](tests/) — add scenarios for `ESC [ Z` parsing and `to_string(Key::back_tab())`.
- Public API: additive. New constructor `Key::back_tab` is now part of the surface; pattern matches that previously enumerated every variant will need to be updated (this is inherent to extending any union and is the user-requested behaviour).
- Docs: no changes required to README beyond what falls out of regenerated examples.
