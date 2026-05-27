# term-ansi Specification

## Purpose

Provide pure functions and constants that generate ANSI escape sequences as Fix `String` values for terminal control: cursor movement, screen and line clearing, alternate screen buffer toggling, text style attributes (SGR), and 16-color / 256-color / 24-bit RGB foreground and background coloring. This module has no IO and is safe to import from anywhere.

## Requirements

### Requirement: Control sequence constants

The system SHALL expose two string constants representing the raw control sequence prefixes used to build all ANSI escape sequences.

- `esc` MUST equal the single byte `0x1B` ("ESC") encoded as a Fix `String`.
- `csi` MUST equal `esc` followed by the literal character `[` (i.e. `"\x1B["`).

#### Scenario: esc constant value
- **WHEN** the caller reads `esc`
- **THEN** the returned `String` contains exactly one byte with value `0x1B`

#### Scenario: csi constant value
- **WHEN** the caller reads `csi`
- **THEN** the returned `String` equals `esc + "["`

### Requirement: Screen clearing sequences

The system SHALL provide string constants that emit the standard ANSI screen and line clearing sequences.

- `clear_screen` MUST equal `csi + "2J"`
- `clear_line` MUST equal `csi + "2K"`
- `clear_to_end_of_line` MUST equal `csi + "0K"`
- `clear_to_start_of_line` MUST equal `csi + "1K"`

#### Scenario: clear the entire screen
- **WHEN** the caller writes `clear_screen` to a terminal that respects CSI 2J
- **THEN** the terminal screen is cleared

#### Scenario: clear from cursor to end of line
- **WHEN** the caller writes `clear_to_end_of_line`
- **THEN** the bytes are `ESC [ 0 K`

### Requirement: Alternate screen buffer toggles

The system SHALL provide string constants to enter and leave the xterm alternate screen buffer so that TUI applications can restore the previous terminal contents on exit.

- `enter_alternate_screen` MUST equal `csi + "?1049h"`
- `leave_alternate_screen` MUST equal `csi + "?1049l"`

#### Scenario: enter alternate screen
- **WHEN** the caller writes `enter_alternate_screen`
- **THEN** the emitted bytes are `ESC [ ? 1 0 4 9 h`

#### Scenario: leave alternate screen
- **WHEN** the caller writes `leave_alternate_screen`
- **THEN** the emitted bytes are `ESC [ ? 1 0 4 9 l`

### Requirement: Absolute cursor positioning

The system SHALL provide functions to move the cursor to an absolute position using 1-origin row/column coordinates.

- `move_to(row, col)` MUST return `csi + show(row) + ";" + show(col) + "H"` where `show` is the decimal representation of the integer.
- `move_to_col(col)` MUST return `csi + show(col) + "G"`.
- Negative or zero coordinates are not validated by the library; the caller is responsible for passing values in the terminal's coordinate range. Behaviour on invalid input is defined by the terminal emulator.

#### Scenario: move to row 5, column 10
- **WHEN** the caller invokes `move_to(5, 10)`
- **THEN** the returned string equals `"\x1B[5;10H"`

#### Scenario: move to absolute column
- **WHEN** the caller invokes `move_to_col(20)`
- **THEN** the returned string equals `"\x1B[20G"`

### Requirement: Relative cursor movement

The system SHALL provide four functions for relative cursor movement.

- `move_up(n)` MUST return `csi + show(n) + "A"`
- `move_down(n)` MUST return `csi + show(n) + "B"`
- `move_right(n)` MUST return `csi + show(n) + "C"`
- `move_left(n)` MUST return `csi + show(n) + "D"`

#### Scenario: move cursor up by 3
- **WHEN** the caller invokes `move_up(3)`
- **THEN** the returned string equals `"\x1B[3A"`

#### Scenario: move cursor right by 1
- **WHEN** the caller invokes `move_right(1)`
- **THEN** the returned string equals `"\x1B[1C"`

### Requirement: Cursor visibility and position save/restore

The system SHALL expose string constants for cursor visibility and cursor save/restore.

- `hide_cursor` MUST equal `csi + "?25l"`
- `show_cursor` MUST equal `csi + "?25h"`
- `save_cursor` MUST equal `csi + "s"`
- `restore_cursor` MUST equal `csi + "u"`

#### Scenario: hide and show cursor
- **WHEN** the caller writes `hide_cursor` followed by `show_cursor`
- **THEN** the emitted bytes are `ESC [ ? 2 5 l ESC [ ? 2 5 h`

#### Scenario: save and restore cursor position
- **WHEN** the caller writes `save_cursor`, moves the cursor, then writes `restore_cursor`
- **THEN** the emitted bytes contain `ESC [ s` and `ESC [ u` in that order

### Requirement: Text style sequences

The system SHALL expose string constants for the common SGR text style attributes.

- `reset` MUST equal `csi + "0m"`
- `bold` MUST equal `csi + "1m"`
- `dim` MUST equal `csi + "2m"`
- `italic` MUST equal `csi + "3m"`
- `underline` MUST equal `csi + "4m"`
- `blink` MUST equal `csi + "5m"`
- `reverse` MUST equal `csi + "7m"`

#### Scenario: bold style
- **WHEN** the caller reads `bold`
- **THEN** the value equals `"\x1B[1m"`

#### Scenario: reset all attributes
- **WHEN** the caller reads `reset`
- **THEN** the value equals `"\x1B[0m"`

### Requirement: Sixteen-colour foreground and background

The system SHALL provide a `Color16` union covering the 8 base ANSI colours and their 8 bright variants, and functions `fg_color_16` / `bg_color_16` that emit the corresponding SGR sequences.

- `Color16` SHALL be `box union` with variants `black, red, green, yellow, blue, magenta, cyan, white, bright_black, bright_red, bright_green, bright_yellow, bright_blue, bright_magenta, bright_cyan, bright_white`, each carrying `()`.
- `fg_color_16(c)` for the 8 base colours MUST return `csi + "3" + digit(c) + "m"` where `digit` maps `black..white` to `0..7`.
- `fg_color_16(c)` for the 8 bright variants MUST return `csi + "9" + digit(c) + "m"` where `digit` maps `bright_black..bright_white` to `0..7`.
- `bg_color_16(c)` MUST use the same mapping but with prefixes `4` (base) and `10` (bright).

#### Scenario: red foreground
- **WHEN** the caller invokes `fg_color_16(Color16::red())`
- **THEN** the returned string equals `"\x1B[31m"`

#### Scenario: bright green foreground
- **WHEN** the caller invokes `fg_color_16(Color16::bright_green())`
- **THEN** the returned string equals `"\x1B[92m"`

#### Scenario: blue background
- **WHEN** the caller invokes `bg_color_16(Color16::blue())`
- **THEN** the returned string equals `"\x1B[44m"`

#### Scenario: bright white background
- **WHEN** the caller invokes `bg_color_16(Color16::bright_white())`
- **THEN** the returned string equals `"\x1B[107m"`

### Requirement: 256-colour foreground and background

The system SHALL provide functions to emit xterm 256-colour SGR sequences for any `U8` palette index.

- `fg_color_256(n : U8)` MUST return `csi + "38;5;" + show(n) + "m"`
- `bg_color_256(n : U8)` MUST return `csi + "48;5;" + show(n) + "m"`

#### Scenario: 256-colour foreground index 196
- **WHEN** the caller invokes `fg_color_256(196_U8)`
- **THEN** the returned string equals `"\x1B[38;5;196m"`

#### Scenario: 256-colour background index 0
- **WHEN** the caller invokes `bg_color_256(0_U8)`
- **THEN** the returned string equals `"\x1B[48;5;0m"`

### Requirement: True colour (24-bit RGB) foreground and background

The system SHALL provide functions to emit 24-bit RGB SGR sequences.

- `fg_color_rgb(r, g, b)` MUST return `csi + "38;2;" + show(r) + ";" + show(g) + ";" + show(b) + "m"`
- `bg_color_rgb(r, g, b)` MUST return `csi + "48;2;" + show(r) + ";" + show(g) + ";" + show(b) + "m"`

#### Scenario: RGB foreground (255, 128, 0)
- **WHEN** the caller invokes `fg_color_rgb(255_U8, 128_U8, 0_U8)`
- **THEN** the returned string equals `"\x1B[38;2;255;128;0m"`

#### Scenario: RGB background black
- **WHEN** the caller invokes `bg_color_rgb(0_U8, 0_U8, 0_U8)`
- **THEN** the returned string equals `"\x1B[48;2;0;0;0m"`

### Requirement: Paint helper

The system SHALL provide a convenience function `paint(style_prefix, text)` that wraps `text` between the supplied style prefix and a trailing `reset` sequence.

- `paint(prefix, text)` MUST return `prefix + text + reset`.

#### Scenario: paint red text
- **WHEN** the caller invokes `paint(fg_color_16(Color16::red()), "hi")`
- **THEN** the returned string equals `"\x1B[31mhi\x1B[0m"`

#### Scenario: paint with empty text
- **WHEN** the caller invokes `paint(bold, "")`
- **THEN** the returned string equals `"\x1B[1m\x1B[0m"`
