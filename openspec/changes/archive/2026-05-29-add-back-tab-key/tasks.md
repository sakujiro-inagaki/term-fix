## 1. Implementation

- [x] 1.1 Add `back_tab : ()` to the `Key` union in [src/Yaynu/Term/Key.fix](src/Yaynu/Term/Key.fix) (between `tab` and `backspace`, to preserve the existing reading order).
- [x] 1.2 In `_parse_csi`, inside the `total == 3` branch, add `if term == 90_U8 { KeyParseResult::complete((Key::back_tab(), 3)) };` after the existing `F` arm and before the trailing `KeyParseResult::invalid(3)`.
- [x] 1.3 In `to_string`, add the arm `back_tab() => "back_tab",` between the existing `tab()` and `backspace()` arms.

## 2. Tests

- [x] 2.1 In [tests/TermTest.fix](tests/TermTest.fix), add a parse assertion for Shift+Tab next to the existing `ArrowUp`/CSI cases: `_expect("BackTab", [27_U8, 91_U8, 90_U8], "complete(back_tab,3)")`.
- [x] 2.2 In `test_key_to_string`, add an assertion that `Key::to_string(Key::back_tab())` is non-empty and differs from `Key::to_string(Key::tab())`.

## 3. Verification

- [x] 3.1 Run `fix test` (or the project's test runner script under [scripts/](scripts/)) and confirm all assertions pass with no regressions.
- [x] 3.2 Run `openspec validate add-back-tab-key --strict` and confirm the change is valid.
