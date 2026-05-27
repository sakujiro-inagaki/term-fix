# term-input Specification

## Purpose

Provide blocking-with-timeout and non-blocking byte reads from stdin (via `select(2)` + `read(2)` in the FFI shim) so that TUI applications running in raw mode can poll input on a tick, distinguish a bare ESC press from the start of an escape sequence, and drain pasted or buffered bytes without ever blocking the main loop indefinitely.

## Requirements

### Requirement: Read one byte with timeout

The system SHALL provide `read_byte_timeout : I64 -> IOFail (Option U8)` that reads a single byte from stdin, blocking for at most the supplied number of milliseconds.

- The function MUST call the FFI shim `fix_term_read_byte_timeout(STDIN_FILENO, timeout_ms, *out)` which uses `select(2)` to wait for stdin readability up to `timeout_ms` and then `read(2)` for exactly one byte.
- Return value mapping:
  - shim returns `1` (byte read): the function MUST yield `Ok (Some(byte))`.
  - shim returns `0` (timeout elapsed, no byte): the function MUST yield `Ok None`.
  - shim returns `-1` (syscall error): the function MUST fail the `IOFail` with an error string mentioning `select` or `read` plus the errno description.
- A `timeout_ms` of `0` MUST be treated as a non-blocking poll (return immediately).
- A negative `timeout_ms` is not supported in v0.1; callers MUST pass `>= 0`. Behaviour is implementation-defined for negative values (subject to change).
- The function MUST be safe to call from a thread that holds raw mode on stdin and MUST NOT modify the termios state.

#### Scenario: byte available within timeout
- **GIVEN** stdin is in raw mode and the user types `a` within 100 ms
- **WHEN** the caller invokes `read_byte_timeout(1000)`
- **THEN** the result is `Ok (Some(0x61_U8))`

#### Scenario: timeout elapses with no input
- **GIVEN** stdin is in raw mode and no input is provided
- **WHEN** the caller invokes `read_byte_timeout(50)`
- **THEN** the result is `Ok None` after approximately 50 ms

#### Scenario: non-blocking poll with timeout 0
- **GIVEN** stdin has no input buffered
- **WHEN** the caller invokes `read_byte_timeout(0)`
- **THEN** the result is `Ok None` returned without blocking

### Requirement: Non-blocking single-byte read

The system SHALL provide `read_byte_nonblock : IOFail (Option U8)` as a convenience equivalent to `read_byte_timeout(0)`.

- The function MUST return immediately.
- The result MUST be `Ok (Some(byte))` if a byte is available, `Ok None` if not, or a failure if the underlying syscall errors.

#### Scenario: non-blocking read with no input
- **GIVEN** stdin has no buffered input
- **WHEN** the caller invokes `read_byte_nonblock`
- **THEN** the result is `Ok None` without blocking

#### Scenario: non-blocking read with buffered input
- **GIVEN** stdin has the byte `0x1B` buffered
- **WHEN** the caller invokes `read_byte_nonblock`
- **THEN** the result is `Ok (Some(0x1B_U8))`

### Requirement: Drain all available bytes

The system SHALL provide `read_available : IOFail (Array U8)` that returns every byte currently available on stdin without blocking.

- The function MUST repeatedly call the non-blocking single-byte read until it yields `None` or an error.
- It MUST collect the read bytes in order into a freshly allocated `Array U8` and return them.
- If the first read fails with an error, the function MUST fail the `IOFail`. If a later read fails after some bytes were already collected, the function MUST also fail (collected bytes are discarded); the underlying error string SHALL mention `read`.
- If no bytes are available at all, the function MUST return `Ok (Array U8 of length 0)`.

#### Scenario: drain a multi-byte escape sequence
- **GIVEN** stdin has the bytes `ESC [ A` (`0x1B 0x5B 0x41`) buffered
- **WHEN** the caller invokes `read_available`
- **THEN** the result is `Ok [0x1B_U8, 0x5B_U8, 0x41_U8]` and a subsequent `read_available` returns `Ok []`

#### Scenario: drain with nothing buffered
- **WHEN** the caller invokes `read_available` while no input is buffered
- **THEN** the result is `Ok []` returned without blocking
