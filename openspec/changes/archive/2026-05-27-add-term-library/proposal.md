## Why

Yaynu はターミナル上で動く TUI アプリだが、現状 ANSI 制御・raw mode・入力デコード・サイズ取得といったターミナル低レベル操作の抽象が存在しない。これらを各所に直書きすると TUI レイヤ (将来の `tui-fix`) の責務が膨らみ、Ctrl+C 後にターミナルが壊れる等の事故も起こりやすい。最小限の依存で動く独立した「ターミナル制御基盤」を 1 本作り、後段 (`tui-fix`、アプリ本体) がこの上に積めるようにする。

将来的にはスタンドアローンの `term` ライブラリとして切り出して再利用可能にする予定だが、初版は yaynu プロジェクトの内部モジュールとして `Yaynu::Term::*` 名前空間配下に置く。これにより yaynu リポジトリの開発フローに乗せつつ、API 形と責務境界をスタンドアローン抽出を前提に設計しておく。

## What Changes

- 新規ライブラリ `Yaynu::Term` を追加 (`src/Yaynu/Term/` 配下)
  - `Yaynu::Term::Ansi`: ANSI エスケープシーケンス生成 (色、カーソル、画面、スタイル)
    - 16 色 / 256 色 / True Color (24-bit RGB) すべて対応
  - `Yaynu::Term::Mode`: termios による raw mode 出入り、`with_raw` で例外安全な復帰を保証
  - `Yaynu::Term::Size`: `ioctl(TIOCGWINSZ)` でサイズ取得、SIGWINCH ハンドラ登録 (シグナル安全)
  - `Yaynu::Term::Input`: タイムアウト付き / 非ブロッキングのバイト読み取り
  - `Yaynu::Term::Key`: 入力バイト列 → 論理キー (`Key` 型) 変換の状態機械、UTF-8 マルチバイト対応
  - `Yaynu::Term::Output`: stdout への flush 付き書き込み (単発・バッチ)
  - `Yaynu::Term::Ffi`: FFI 宣言 (非公開モジュール)
- C shim `c_src/term_shim.c` を追加 (termios / ioctl / sigaction / select ラッパー)
- ビルドスクリプト `scripts/build_shim.sh` (`openssl-fix` のものを参考に C shim をビルド)
- 単体テスト (`tests/TermTest.fix`) で純粋関数部 (`Ansi`、`Key::parse`) をカバー
- 手動動作確認用の `examples/` (`echo_keys.fix`、`show_size.fix`、`colors.fix`、`cursor.fix`)
- README にスコープ・対応プラットフォーム (Linux/macOS のみ)・基本的な使い方・raw mode の注意・ESC タイムアウトの説明を記載

非対応 (v0.1 では含まない):
- ウィジェット、差分描画、レイアウト (`tui-fix` の責務)
- Windows サポート (termios/ioctl が違うため v0.2 以降)
- ターミナルケーパビリティ検出 (常に ANSI 前提)

## Capabilities

### New Capabilities
- `term-ansi`: ANSI エスケープシーケンスを文字列として生成する純粋関数群 (色・カーソル・画面・スタイル)
- `term-mode`: termios の raw mode 出入りと例外安全な復帰
- `term-size`: ターミナルサイズ取得と SIGWINCH によるリサイズ検知
- `term-input`: 標準入力からのタイムアウト付き / 非ブロッキングなバイト読み取り
- `term-key`: 入力バイト列を論理キー (`Key`) に変換する状態機械 (UTF-8 / ESC シーケンス対応)
- `term-output`: stdout への flush 付き書き込みヘルパー (単発・バッチング)

### Modified Capabilities
<!-- 既存の spec はないため空 -->

## Impact

- 新規コード: `src/Yaynu/Term/*.fix` (7 モジュール)、`c_src/term_shim.c`、`scripts/build_shim.sh`、`tests/TermTest.fix`、`examples/*.fix`
- 依存: Std のみ (Minilib には依存しない、独立配布性を優先)
- 対応プラットフォーム: Linux / macOS (POSIX 系)。Windows は対象外
- ビルド: `fixproj.toml` に C shim のリンク設定を追加。`scripts/build_shim.sh` を `cargo build` 相当の前処理として走らせる必要あり
- FFI: `c_src/term_shim.c` 経由で `tcgetattr` / `tcsetattr` / `ioctl(TIOCGWINSZ)` / `sigaction` / `select` を呼ぶ
- 将来計画: `Yaynu::Term::*` → `Term::*` への名前空間リネーム + 別リポジトリ抽出。今のうちに API が `Yaynu` 固有概念に依存しないよう設計しておく
