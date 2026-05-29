## MODIFIED Requirements

### Requirement: Key union of logical keys

The system SHALL expose a `Key` union covering printable characters, special keys, navigation keys, function keys, and modifier-prefixed keys.

`Key` MUST be declared as `box union` with at least the following variants:

- `char : String` — a single grapheme as a complete UTF-8 byte sequence (1–4 bytes).
- `enter : ()` — pressed for CR (`0x0D`) or LF (`0x0A`).
- `tab : ()` — pressed for `0x09`.
- `back_tab : ()` — pressed for Shift+Tab, emitted by xterm-compatible terminals as `ESC [ Z` (terminfo `kcbt`).
- `backspace : ()` — pressed for `0x7F` or `0x08`.
- `escape : ()` — a bare ESC press once the caller has decided (via timeout) that no follow-up will arrive.
- `arrow_up : ()`, `arrow_down : ()`, `arrow_left : ()`, `arrow_right : ()`.
- `home : ()`, `end_key : ()`, `page_up : ()`, `page_down : ()`, `insert : ()`, `delete_key : ()`.
- `f : I64` — function key number 1..12 inclusive.
- `ctrl : String` — `ctrl("a") .. ctrl("z")` for control bytes `0x01..0x1A`.
- `alt : String` — single-character Alt-prefixed input (`ESC + printable`).
- `unknown : Array U8` — bytes that do not match any of the above forms.

Variants `end_key` and `delete_key` use the `_key` suffix because `end` and `delete` are reserved.

#### Scenario: Key is a box union with the documented variants
- **WHEN** the caller inspects the `Key` type
- **THEN** it is a box union and exposes constructors `char`, `enter`, `tab`, `back_tab`, `backspace`, `escape`, `arrow_up`, `arrow_down`, `arrow_left`, `arrow_right`, `home`, `end_key`, `page_up`, `page_down`, `insert`, `delete_key`, `f`, `ctrl`, `alt`, `unknown`

### Requirement: parse function recognises ESC-prefixed sequences

When the first byte is `0x1B` (ESC), the system SHALL interpret the following bytes as an escape sequence according to the table below.

Bare ESC: if only the byte `0x1B` is present in the buffer, the function MUST return `incomplete()`. (The caller decides — via timeout — when to declare a lone ESC; the helper `escape_from_timeout : Key` is exposed for that purpose by returning `Key::escape`.)

CSI sequences (`ESC [ ...`):

- `ESC [ A` → `complete(arrow_up, 3)`
- `ESC [ B` → `complete(arrow_down, 3)`
- `ESC [ C` → `complete(arrow_right, 3)`
- `ESC [ D` → `complete(arrow_left, 3)`
- `ESC [ H` → `complete(home, 3)`
- `ESC [ F` → `complete(end_key, 3)`
- `ESC [ Z` → `complete(back_tab, 3)`
- `ESC [ 1 ~` → `complete(home, 4)` (alternate form)
- `ESC [ 2 ~` → `complete(insert, 4)`
- `ESC [ 3 ~` → `complete(delete_key, 4)`
- `ESC [ 4 ~` → `complete(end_key, 4)` (alternate form)
- `ESC [ 5 ~` → `complete(page_up, 4)`
- `ESC [ 6 ~` → `complete(page_down, 4)`
- `ESC [ 1 5 ~` → `complete(f(5), 5)`
- `ESC [ 1 7 ~` → `complete(f(6), 5)`
- `ESC [ 1 8 ~` → `complete(f(7), 5)`
- `ESC [ 1 9 ~` → `complete(f(8), 5)`
- `ESC [ 2 0 ~` → `complete(f(9), 5)`
- `ESC [ 2 1 ~` → `complete(f(10), 5)`
- `ESC [ 2 3 ~` → `complete(f(11), 5)`
- `ESC [ 2 4 ~` → `complete(f(12), 5)`

SS3 sequences (`ESC O ...`):

- `ESC O P` → `complete(f(1), 3)`
- `ESC O Q` → `complete(f(2), 3)`
- `ESC O R` → `complete(f(3), 3)`
- `ESC O S` → `complete(f(4), 3)`

Alt-prefixed printable ASCII:

- `ESC <printable ASCII byte b>` where `b` is in `0x20..0x7E` MUST yield `complete(alt(<single-byte UTF-8 string of b>), 2)`.

Incomplete CSI / SS3:

- A buffer that is exactly `[ESC]`, `[ESC, '[']`, or `[ESC, 'O']`, or any longer ESC sequence that has not yet matched a terminator (`A..Z`, `a..z`, `~`) MUST yield `incomplete()`.

Invalid CSI / SS3:

- An ESC sequence whose final byte does not match any recognised entry above MUST yield `invalid(n)` where `n` is the total number of bytes the caller should discard to resync (the full unrecognised sequence including ESC and the terminator).

#### Scenario: parse arrow up
- **WHEN** the caller invokes `parse([0x1B_U8, 0x5B_U8, 0x41_U8])`
- **THEN** the result is `complete(arrow_up, 3)`

#### Scenario: parse F1
- **WHEN** the caller invokes `parse([0x1B_U8, 0x4F_U8, 0x50_U8])`
- **THEN** the result is `complete(f(1), 3)`

#### Scenario: parse F5
- **WHEN** the caller invokes `parse([0x1B_U8, 0x5B_U8, 0x31_U8, 0x35_U8, 0x7E_U8])`
- **THEN** the result is `complete(f(5), 5)`

#### Scenario: parse Alt+a
- **WHEN** the caller invokes `parse([0x1B_U8, 0x61_U8])`
- **THEN** the result is `complete(alt("a"), 2)`

#### Scenario: parse Shift+Tab
- **WHEN** the caller invokes `parse([0x1B_U8, 0x5B_U8, 0x5A_U8])`
- **THEN** the result is `complete(back_tab, 3)`

#### Scenario: bare ESC is incomplete
- **WHEN** the caller invokes `parse([0x1B_U8])`
- **THEN** the result is `incomplete()`

#### Scenario: ESC [ alone is incomplete
- **WHEN** the caller invokes `parse([0x1B_U8, 0x5B_U8])`
- **THEN** the result is `incomplete()`

#### Scenario: ESC [ followed by unknown terminator is invalid
- **WHEN** the caller invokes `parse([0x1B_U8, 0x5B_U8, 0x7A_U8])`
- **THEN** the result is `invalid(3)`

### Requirement: Key::to_string for diagnostics

The system SHALL provide `to_string : Key -> String` that returns a human-readable representation of a `Key` for debugging and logging.

- The result MUST be a non-empty `String`.
- Distinct variants MUST produce distinguishable strings (e.g. `arrow_up` vs `arrow_down`, `tab` vs `back_tab`).
- For `char(s)`, `ctrl(s)`, `alt(s)`, and `f(n)` the result MUST include the carried value (e.g. `"char:a"`, `"ctrl:c"`, `"alt:b"`, `"f:5"`). The exact format is not constrained beyond these requirements.
- For `unknown(bytes)` the result MUST include the byte values in some readable hex form.

#### Scenario: to_string for arrow_up
- **WHEN** the caller invokes `Key::to_string(Key::arrow_up())`
- **THEN** the returned string is non-empty and distinct from the to_string of any other variant

#### Scenario: to_string for ctrl("c")
- **WHEN** the caller invokes `Key::to_string(Key::ctrl("c"))`
- **THEN** the returned string contains the substring `"c"`

#### Scenario: to_string for f(5)
- **WHEN** the caller invokes `Key::to_string(Key::f(5))`
- **THEN** the returned string contains the substring `"5"`

#### Scenario: to_string for back_tab
- **WHEN** the caller invokes `Key::to_string(Key::back_tab())`
- **THEN** the returned string is non-empty and distinct from `Key::to_string(Key::tab())`
