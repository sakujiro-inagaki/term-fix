## ADDED Requirements

### Requirement: Key union of logical keys

The system SHALL expose a `Key` union covering printable characters, special keys, navigation keys, function keys, and modifier-prefixed keys.

`Key` MUST be declared as `box union` with at least the following variants:

- `char : String` — a single grapheme as a complete UTF-8 byte sequence (1–4 bytes).
- `enter : ()` — pressed for CR (`0x0D`) or LF (`0x0A`).
- `tab : ()` — pressed for `0x09`.
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
- **THEN** it is a box union and exposes constructors `char`, `enter`, `tab`, `backspace`, `escape`, `arrow_up`, `arrow_down`, `arrow_left`, `arrow_right`, `home`, `end_key`, `page_up`, `page_down`, `insert`, `delete_key`, `f`, `ctrl`, `alt`, `unknown`

### Requirement: KeyParseResult variants

The system SHALL expose a `KeyParseResult` union that describes the outcome of attempting to parse a key from a byte buffer.

`KeyParseResult` MUST be `box union` with the following variants:

- `complete : (Key, I64)` — a key was recognised; the second component is the number of input bytes consumed (always `>= 1`).
- `incomplete : ()` — the buffer is a valid prefix of a longer sequence; the caller should append more bytes and call `parse` again.
- `invalid : I64` — the leading bytes do not form a recognisable sequence; the second component is the number of bytes the caller should discard (always `>= 1`).

#### Scenario: KeyParseResult variants
- **WHEN** the caller inspects the `KeyParseResult` type
- **THEN** it exposes constructors `complete : (Key, I64)`, `incomplete : ()`, `invalid : I64`

### Requirement: parse function recognises plain ASCII printable bytes

The system SHALL provide `parse : Array U8 -> KeyParseResult` that, when the first byte is a printable ASCII byte (`0x20..0x7E`), returns `complete(char(<single-byte UTF-8 string>), 1)`.

- The function MUST NOT consume more than one byte for printable ASCII input.

#### Scenario: parse "a"
- **WHEN** the caller invokes `parse([0x61_U8])`
- **THEN** the result is `complete(char("a"), 1)`

#### Scenario: parse space
- **WHEN** the caller invokes `parse([0x20_U8])`
- **THEN** the result is `complete(char(" "), 1)`

### Requirement: parse function recognises special control bytes

The system SHALL map the following individual control bytes to their corresponding `Key` variants, consuming exactly one byte each.

- `0x0D` and `0x0A` MUST yield `complete(enter, 1)`.
- `0x09` MUST yield `complete(tab, 1)`.
- `0x7F` and `0x08` MUST yield `complete(backspace, 1)`.
- A control byte in the range `0x01..0x1A` other than the above (i.e. `0x01..0x08`, `0x0B..0x0C`, `0x0E..0x1A`) MUST yield `complete(ctrl(<single lowercase letter>), 1)`, where the letter is `(byte - 1) + 'a'`. For example, `0x03` MUST yield `complete(ctrl("c"), 1)`.

#### Scenario: parse Enter
- **WHEN** the caller invokes `parse([0x0D_U8])`
- **THEN** the result is `complete(enter, 1)`

#### Scenario: parse Tab
- **WHEN** the caller invokes `parse([0x09_U8])`
- **THEN** the result is `complete(tab, 1)`

#### Scenario: parse Ctrl+C
- **WHEN** the caller invokes `parse([0x03_U8])`
- **THEN** the result is `complete(ctrl("c"), 1)`

#### Scenario: parse Backspace
- **WHEN** the caller invokes `parse([0x7F_U8])`
- **THEN** the result is `complete(backspace, 1)`

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

#### Scenario: bare ESC is incomplete
- **WHEN** the caller invokes `parse([0x1B_U8])`
- **THEN** the result is `incomplete()`

#### Scenario: ESC [ alone is incomplete
- **WHEN** the caller invokes `parse([0x1B_U8, 0x5B_U8])`
- **THEN** the result is `incomplete()`

#### Scenario: ESC [ followed by unknown terminator is invalid
- **WHEN** the caller invokes `parse([0x1B_U8, 0x5B_U8, 0x7A_U8])`
- **THEN** the result is `invalid(3)`

### Requirement: parse function handles UTF-8 multi-byte input

When the first byte has the high-bit set, the system SHALL interpret the buffer as the start of a UTF-8 encoded character.

- A leading byte of `110xxxxx` requires 2 bytes total.
- A leading byte of `1110xxxx` requires 3 bytes total.
- A leading byte of `11110xxx` requires 4 bytes total.
- A leading byte of `10xxxxxx` (continuation byte without a leader) MUST yield `invalid(1)`.
- A leading byte of `11111xxx` or other malformed leader MUST yield `invalid(1)`.
- If the buffer holds the required number of bytes and every continuation byte starts with `10`, the function MUST yield `complete(char(<the bytes as a String>), N)` where `N` is 2, 3, or 4.
- If a continuation byte does not start with `10`, the function MUST yield `invalid(k)` where `k` is the number of bytes consumed so far (the leader plus any valid continuations encountered before the malformed one, minimum 1).
- If the buffer is shorter than the expected length but all bytes seen so far are valid, the function MUST yield `incomplete()`.

#### Scenario: parse 2-byte UTF-8 (é, 0xC3 0xA9)
- **WHEN** the caller invokes `parse([0xC3_U8, 0xA9_U8])`
- **THEN** the result is `complete(char("é"), 2)`

#### Scenario: parse 3-byte UTF-8 (あ, 0xE3 0x81 0x82)
- **WHEN** the caller invokes `parse([0xE3_U8, 0x81_U8, 0x82_U8])`
- **THEN** the result is `complete(char("あ"), 3)`

#### Scenario: parse 4-byte UTF-8 (🙂, 0xF0 0x9F 0x99 0x82)
- **WHEN** the caller invokes `parse([0xF0_U8, 0x9F_U8, 0x99_U8, 0x82_U8])`
- **THEN** the result is `complete(char("🙂"), 4)`

#### Scenario: incomplete 3-byte UTF-8
- **WHEN** the caller invokes `parse([0xE3_U8, 0x81_U8])`
- **THEN** the result is `incomplete()`

#### Scenario: stray continuation byte
- **WHEN** the caller invokes `parse([0x80_U8])`
- **THEN** the result is `invalid(1)`

### Requirement: parse function handles empty input

When the input array is empty, the system SHALL return `incomplete()`.

#### Scenario: parse empty input
- **WHEN** the caller invokes `parse([])`
- **THEN** the result is `incomplete()`

### Requirement: Key::to_string for diagnostics

The system SHALL provide `to_string : Key -> String` that returns a human-readable representation of a `Key` for debugging and logging.

- The result MUST be a non-empty `String`.
- Distinct variants MUST produce distinguishable strings (e.g. `arrow_up` vs `arrow_down`).
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
