# term-size Specification

## Purpose

Expose the terminal's current size (rows and columns) via `ioctl(TIOCGWINSZ)` and provide a signal-safe `SIGWINCH` handler with a check-and-clear flag so that TUI applications can detect window resizes and re-query the size at a controlled point in their main loop.

## Requirements

### Requirement: TermSize struct

The system SHALL expose a `TermSize` struct describing the terminal dimensions in character cells.

- `TermSize` MUST be declared as `struct { rows : I64, cols : I64 }`.
- `rows` MUST hold the number of visible rows (vertical character cells).
- `cols` MUST hold the number of visible columns (horizontal character cells).

#### Scenario: TermSize fields
- **WHEN** the caller reads `rows` and `cols` from a `TermSize`
- **THEN** both are `I64` and represent the row/column counts respectively

### Requirement: Get current terminal size

The system SHALL provide `TermSize::get : IOFail TermSize` that returns the current size of the terminal attached to stdout via `ioctl(TIOCGWINSZ)`.

- The function MUST call the FFI shim `fix_term_get_size(fd, *rows, *cols)` with `fd = STDOUT_FILENO`.
- On success it MUST construct a `TermSize` from the returned values and yield it via `Ok`.
- On non-zero return from the shim it MUST fail the `IOFail` with an error string mentioning `ioctl TIOCGWINSZ`.

#### Scenario: get terminal size on a tty
- **WHEN** the caller runs `TermSize::get` while stdout is a tty
- **THEN** the returned `TermSize` has positive `rows` and `cols` matching the actual terminal window

#### Scenario: get fails when stdout is not a tty
- **WHEN** the caller runs `TermSize::get` with stdout redirected to a pipe
- **THEN** the `IOFail` fails with an error mentioning `ioctl TIOCGWINSZ`

### Requirement: Install SIGWINCH handler

The system SHALL provide `install_resize_handler : IO ()` that installs a `SIGWINCH` handler whose only effect is to set a shared flag indicating that a resize has occurred.

- The function MUST call `fix_term_install_winch_handler()` via the FFI shim.
- The shim MUST use `sigaction(2)` with a handler that performs only async-signal-safe operations: writing the value `1` to a `volatile sig_atomic_t` flag.
- Repeated calls to `install_resize_handler` MUST be idempotent (re-installing the same handler is safe and overwrites any previous registration).
- The function MUST NOT block, allocate, or call back into the Fix runtime from within the signal handler.

#### Scenario: handler installation succeeds
- **WHEN** the caller invokes `install_resize_handler`
- **THEN** the IO action completes without error and subsequent `SIGWINCH` deliveries set the shared flag

#### Scenario: idempotent installation
- **WHEN** the caller invokes `install_resize_handler` twice
- **THEN** the second call completes without error and the handler remains armed

### Requirement: Check and clear the resize flag

The system SHALL provide `check_resized : IO Bool` that atomically reads and clears the shared resize flag set by the `SIGWINCH` handler.

- The function MUST call `fix_term_check_winch_flag()` via the FFI shim.
- If the flag was set since the previous call (or since `install_resize_handler`), the function MUST return `true` and the flag MUST be cleared before the next call.
- If the flag was not set, the function MUST return `false` without modifying it.
- The read-and-clear MUST be performed in a way that loses at most one resize event between two adjacent `check_resized` calls (it is acceptable to coalesce multiple resizes that occurred between two checks into a single `true`).

#### Scenario: flag returns true once per group of resizes
- **GIVEN** `install_resize_handler` has been called and three `SIGWINCH` signals have been delivered
- **WHEN** the caller invokes `check_resized` once
- **THEN** the result is `true`
- **AND** an immediately subsequent `check_resized` returns `false` (assuming no further signals)

#### Scenario: flag is false when no resize occurred
- **GIVEN** `install_resize_handler` has been called and no `SIGWINCH` has been delivered
- **WHEN** the caller invokes `check_resized`
- **THEN** the result is `false`
