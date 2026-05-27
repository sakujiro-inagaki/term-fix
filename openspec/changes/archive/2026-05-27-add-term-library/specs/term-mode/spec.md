## ADDED Requirements

### Requirement: TermMode opaque handle

The system SHALL expose a `TermMode` type that opaquely holds a saved POSIX `termios` configuration so that callers can restore it later.

- `TermMode` MUST be declared as `box struct { saved : Array U8 }`.
- The `saved` array SHALL contain the raw bytes of a `struct termios` snapshot as obtained from `tcgetattr(2)` via the FFI shim. Its length MUST equal the value returned by `fix_term_termios_size()`.
- Callers MUST treat the `saved` field as opaque; the library reserves the right to change its internal representation in future versions.

#### Scenario: TermMode wraps saved termios bytes
- **WHEN** the FFI shim reports `sizeof(struct termios)` as `N`
- **THEN** any `TermMode` value produced by the library has `saved` of length `N`

### Requirement: Save current terminal mode

The system SHALL provide `TermMode::save : IOFail TermMode` that snapshots the current `stdin` termios configuration without modifying it.

- The function MUST call `tcgetattr(STDIN_FILENO, &buf)` via the FFI shim.
- On success it MUST return a `TermMode` wrapping the snapshot bytes.
- On failure (non-zero return from `tcgetattr`) it MUST fail the `IOFail` with an error string that includes the syscall name and the underlying errno description.

#### Scenario: save succeeds on a tty
- **WHEN** the caller runs `TermMode::save` while stdin is a tty
- **THEN** the call returns a `TermMode` value and stdin's termios configuration is unchanged

#### Scenario: save fails when stdin is not a tty
- **WHEN** the caller runs `TermMode::save` with stdin redirected from a file
- **THEN** the `IOFail` fails with an error mentioning `tcgetattr`

### Requirement: Restore a saved terminal mode

The system SHALL provide `TermMode::restore : TermMode -> IOFail ()` that re-applies a previously saved configuration to `stdin`.

- The function MUST call `tcsetattr(STDIN_FILENO, TCSANOW, saved_buf)` via the FFI shim.
- On non-zero return it MUST fail the `IOFail` with an error mentioning `tcsetattr`.

#### Scenario: restore after enter_raw returns terminal to canonical mode
- **GIVEN** the caller saved the current mode with `save`, then entered raw mode with `enter_raw`
- **WHEN** the caller invokes `restore` with the saved `TermMode`
- **THEN** subsequent reads on stdin behave according to the original (canonical) configuration

### Requirement: Enter raw mode keeping signal generation

The system SHALL provide `enter_raw : IOFail TermMode` that switches stdin into a raw-style mode useful for TUI applications while keeping signal generation (`ISIG`) enabled.

- The function MUST snapshot the current termios via `save` and return that snapshot in the result.
- It MUST then call the shim helper that disables `ICANON` and `ECHO` on `c_lflag`, leaves `ISIG` enabled, and applies the modified termios with `tcsetattr(STDIN_FILENO, TCSANOW, ...)`.
- After the call, `read(2)` on stdin MUST return as soon as a single byte is available (no line buffering) and MUST NOT echo typed characters.
- Pressing Ctrl+C in this mode MUST still deliver `SIGINT` to the process (because `ISIG` is preserved).

#### Scenario: enter_raw disables echo and canonical input
- **WHEN** the caller enters raw mode
- **THEN** typed bytes are not echoed and are delivered to `read` one byte at a time

#### Scenario: Ctrl+C still raises SIGINT in enter_raw
- **WHEN** Ctrl+C is pressed while in `enter_raw` mode
- **THEN** the process receives `SIGINT`

### Requirement: Enter no-echo mode

The system SHALL provide `enter_no_echo : IOFail TermMode` that disables echo while keeping canonical (line-buffered) input — suitable for password prompts.

- The function MUST snapshot the current termios with `save` and return that snapshot.
- It MUST disable `ECHO` on `c_lflag`, leaving `ICANON`, `ISIG`, and the rest of the configuration untouched, then apply with `tcsetattr(STDIN_FILENO, TCSANOW, ...)`.

#### Scenario: enter_no_echo disables echo only
- **WHEN** the caller enters no-echo mode and reads a line of input
- **THEN** the read returns the typed line and no characters are echoed to the terminal

### Requirement: Enter raw mode without signal generation

The system SHALL provide `enter_raw_no_sig : IOFail TermMode` that switches stdin into raw mode and additionally disables signal generation (`ISIG`).

- The function MUST behave like `enter_raw` but ALSO clear the `ISIG` bit on `c_lflag`.
- After the call, Ctrl+C MUST be delivered to `read(2)` as the byte `0x03` rather than raising `SIGINT`.

#### Scenario: Ctrl+C delivered as 0x03 in enter_raw_no_sig
- **WHEN** the caller is in `enter_raw_no_sig` mode and Ctrl+C is pressed
- **THEN** the next `read(2)` on stdin yields the byte `0x03` and no `SIGINT` is delivered

### Requirement: Exception-safe with_raw scope

The system SHALL provide `with_raw : [a] (() -> IOFail a) -> IOFail a` that runs a user action under raw mode and guarantees the previous termios is restored regardless of how the inner action terminates.

- The function MUST call `enter_raw` to obtain a saved `TermMode`.
- It MUST then invoke the supplied action.
- Whether the inner action succeeds with `Ok a` or fails with an error, the function MUST call `TermMode::restore` on the saved mode before returning.
- If both the inner action and `restore` fail, the inner action's error MUST take precedence in the returned `IOFail`.

#### Scenario: restore runs on success
- **GIVEN** an inner action that returns `Ok 42`
- **WHEN** the caller runs `with_raw(action)`
- **THEN** the result is `Ok 42` and stdin's termios is restored to the pre-call state

#### Scenario: restore runs on inner failure
- **GIVEN** an inner action that fails with `"boom"`
- **WHEN** the caller runs `with_raw(action)`
- **THEN** the returned `IOFail` carries `"boom"` AND stdin's termios is restored to the pre-call state
