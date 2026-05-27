## ADDED Requirements

### Requirement: Write a single string to stdout and flush

The system SHALL provide `write : String -> IOFail ()` that writes the supplied string to stdout and flushes the stdout stream before returning.

- The function MUST write the raw bytes of the string to stdout exactly as-is, without any additional newline or escape processing.
- After writing, the function MUST flush stdout so that the bytes are visible on the terminal before control returns.
- If the underlying write or flush fails, the function MUST fail the `IOFail` with an error string mentioning the failing operation (`write` or `flush`).
- Writing an empty string MUST still perform a flush and MUST succeed with `Ok ()`.

#### Scenario: write hello world
- **WHEN** the caller invokes `write("Hello, TUI!")`
- **THEN** the bytes `H e l l o , (space) T U I !` appear on stdout before the call returns

#### Scenario: write an ANSI escape sequence
- **WHEN** the caller invokes `write("\x1B[2J")`
- **THEN** the terminal receives the clear-screen sequence and the screen is cleared before the call returns

#### Scenario: write empty string
- **WHEN** the caller invokes `write("")`
- **THEN** the result is `Ok ()` and stdout has been flushed

### Requirement: Write a batch of strings atomically

The system SHALL provide `write_batch : Array String -> IOFail ()` that writes a sequence of strings to stdout as a single underlying write so that the output cannot be visually torn between strings.

- The function MUST concatenate the input strings in array order into a single buffer and then issue exactly one write to stdout.
- After writing, the function MUST flush stdout.
- An empty input array MUST be treated as writing the empty string (single flush, no bytes written) and MUST succeed with `Ok ()`.
- If the underlying write or flush fails, the function MUST fail the `IOFail` with an error string mentioning the failing operation.

#### Scenario: batch write of a frame
- **GIVEN** an array `["\x1B[2J", "\x1B[1;1H", "Hello"]`
- **WHEN** the caller invokes `write_batch(array)`
- **THEN** stdout receives the concatenated bytes in order and is flushed before the call returns

#### Scenario: batch write atomicity
- **WHEN** the caller invokes `write_batch` with several strings
- **THEN** the underlying syscall trace shows a single contiguous write of the concatenated payload (no interleaving between the array elements)

#### Scenario: batch write of an empty array
- **WHEN** the caller invokes `write_batch([])`
- **THEN** the result is `Ok ()` and stdout has been flushed
