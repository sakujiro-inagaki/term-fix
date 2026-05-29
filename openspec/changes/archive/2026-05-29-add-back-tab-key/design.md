## Context

The `Yaynu.Term.Key` module exposes a logical `Key` union plus a pure byte-buffer state machine `parse : Array U8 -> KeyParseResult`. The parser already recognises the xterm CSI family for arrow keys, Home/End, navigation keys (`~`-terminated), and SS3 function keys, but the CSI sequence `ESC [ Z` — which xterm-compatible terminals emit for Shift+Tab (terminfo `kcbt`) — is currently classified as `invalid(3)` because the 3-byte CSI dispatch in `_parse_csi` does not list `Z` (`0x5A`).

A library user (`linenoise-fix`-style consumer) needs to distinguish Shift+Tab from other keys to implement reverse focus / reverse autocomplete. They have requested a dedicated `Key::back_tab` variant rather than e.g. re-using `Key::tab` with a modifier field, which is consistent with how this module already represents other "named" keys (no modifier carrier on the variant).

## Goals / Non-Goals

**Goals:**
- Recognise `ESC [ Z` as a distinct logical key with a stable, self-describing name.
- Keep the change additive at the type level (no rename, no removal).
- Keep the parser change minimal — one new dispatch arm inside the existing 3-byte CSI branch.
- Update `to_string` so diagnostics distinguish `back_tab` from `tab`.

**Non-Goals:**
- Modifier-aware parsing in general (`ESC [ 1 ; <mod> A` etc. remain v0.1-style `invalid`). This change does not introduce a general modifier model; `back_tab` is a single named key, not `tab + shift`.
- Adding any other backtab encodings (no `ESC O Z`, no terminfo lookup, no `kcbt` variants). xterm/alacritty/iTerm2/VT220 all emit `ESC [ Z`; that is the only sequence we recognise.
- Changing how `Key::tab` itself is parsed or rendered.

## Decisions

**1. Name the variant `back_tab`, not `shift_tab`.**
xterm/ncurses terminfo names this capability `kcbt` ("key back-tab"), and `back_tab` matches the naming the library user requested. It also reads as a peer of `tab` rather than implying a general modifier model that the library does not have.

**2. Place the new dispatch arm in the existing 3-byte CSI block in `_parse_csi`, not in a new helper.**
The block at [Key.fix:159-167](src/Yaynu/Term/Key.fix#L159-L167) already enumerates each letter-terminated 3-byte CSI form (`A`/`B`/`C`/`D`/`H`/`F`). Adding `if term == 90_U8 { ... Key::back_tab() ... }` keeps the existing shape and the existing fall-through to `invalid(3)`. No new helper or refactor is justified for one line.

**3. `to_string(back_tab()) = "back_tab"`.**
Consistent with the existing precedent: each named variant renders as its constructor name (`tab` → `"tab"`, `arrow_up` → `"arrow_up"`, etc.). The user's requested string matches this convention.

**4. Refinement of `invalid(3)`, not a breaking change.**
Callers currently receive `invalid(3)` for `ESC [ Z` and discard 3 bytes. After this change they receive `complete(back_tab, 3)` and consume 3 bytes — same buffer advancement, different semantics. Pattern matches on `Key` that enumerate every variant will require a new arm; this is the inherent cost of extending any union and is the user's explicit request.

## Risks / Trade-offs

- **Exhaustive `match Key` in downstream code**: any consumer that pattern-matches every variant will fail to compile until they add a `back_tab()` arm.
  Mitigation: this is expected/desired by the requesting user, and it is the same risk as any other variant addition. Noted in the proposal's Impact section.

- **Misclassification of `ESC [ Z` by a non-xterm terminal**: in principle some terminal could emit `ESC [ Z` for something else. In practice xterm, VT220, alacritty, iTerm2, kitty, and the Linux console all use it for Shift+Tab; no widely-deployed terminal disagrees.
  Mitigation: none required — accepted as standard.

- **No symmetric encoding helper**: the library does not currently emit key sequences (it only parses), so there is nothing to keep in sync on the encoding side.
