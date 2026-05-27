## Context

Yaynu はターミナル上で動く TUI アプリ。現状リポジトリは初期化済みでコードは未着手。将来的に `tui-fix` (ウィジェット層) と Yaynu アプリ本体を積む前提で、その基盤となる**低レベルなターミナル制御ライブラリ**を最初に整える必要がある。

「低レベル」と呼ぶ理由:
- ANSI エスケープシーケンス生成 (純粋な文字列計算)
- termios による raw mode 遷移
- `ioctl(TIOCGWINSZ)` によるサイズ取得と `sigaction(SIGWINCH)` ハンドリング
- 標準入力からのタイムアウト付きバイト読み取り (`select(2)`)
- 入力バイト列 → 論理キー (`Key` 型) の状態機械

これらは POSIX system call をたたく FFI が必要で、シグナル安全性・raw mode 復帰保証・UTF-8 マルチバイト処理などのデリケートな実装上の判断が含まれる。アプリ本体や TUI ウィジェット層からは隠蔽したい。

初版は **yaynu リポジトリの内部モジュール** (`src/Yaynu/Term/...`、名前空間 `Yaynu::Term::*`) として実装する。理由は (a) 開発初期に別リポジトリ管理のオーバーヘッドを避ける、(b) yaynu の進捗とロックステップで API を進化させやすい、の 2 点。ただし設計段階から **`Yaynu` 名前空間に依存する型・関数を export しない**こと、**`Yaynu` 名以外で再利用可能な API 形にしておく**ことを規律として持つ。将来 `term` として抽出する際は、ほぼ機械的な名前空間 rename とリポジトリ移動で済むようにする。

参考とする周辺ライブラリ: openssl-fix / httpc-fix の Fix プロジェクト群 — 「Std のみ依存」「C shim を `c_src/` に置き `scripts/build_shim.sh` でビルド」「FFI 宣言は内部モジュールに集約」というパターンを踏襲する。

## Goals / Non-Goals

**Goals:**
- Linux / macOS で動く ANSI ベースのターミナル制御プリミティブを提供する
- 例外発生時にもターミナル状態 (termios) が必ず復帰するパターン (`with_raw`) を提供する
- raw mode 中に `read(2)` がブロックしないタイムアウト付き入力 API を提供する
- 入力バイト列 → 論理キーへの変換を **純粋関数の状態機械** として提供 (テスト可能性)
- 16 色 / 256 色 / True Color (24-bit RGB) の出力に対応する
- SIGWINCH によるリサイズ検知をシグナル安全に行う (ハンドラ内ではフラグ立てのみ)
- 依存は **Std のみ** (Minilib 不使用)
- API 形が `Yaynu` 固有概念を持たず、将来そのまま `term` として切り出せる

**Non-Goals:**
- ウィジェット、レイアウト、差分描画 (`tui-fix` の責務)
- Windows サポート (v0.2 以降)
- ターミナルケーパビリティ検出 (常に ANSI + UTF-8 前提)
- マウス入力対応 (v0.2 以降で検討)
- ESC タイムアウトのメインループ判断ロジック自体 (`parse` の責務外、利用側で実装)
- 高水準 IO 抽象 (パスワード入力 UI、補完など) — 別レイヤ

## Decisions

### 1. 名前空間: `Yaynu::Term::*` (将来 `Term::*` に rename)

**選択**: 初版は `src/Yaynu/Term/Ansi.fix` のように Yaynu サブモジュールとして実装。モジュールヘッダは `module Yaynu.Term.Ansi;` 形式。
**理由**: yaynu リポジトリ内で素直に `import Yaynu.Term.Ansi;` できる。API 形そのものは `Yaynu` に依存しないので、抽出時は `sed s/Yaynu\.Term/Term/g` 相当の機械変換で済む。
**代替**: いきなり別リポジトリ `term` を切る案 → 初期開発で yaynu と同期更新するオーバーヘッドが大きい。pass.

### 2. FFI shim は C で 1 ファイル、Fix からは非公開モジュールで宣言

**選択**: `c_src/term_shim.c` に termios/ioctl/sigaction/select ラッパー関数群を実装。`scripts/build_shim.sh` で静的ライブラリにビルド。`src/Yaynu/Term/Ffi.fix` で `FFI_CALL` 宣言し、**この `Yaynu::Term::Ffi` モジュールは外部に公開しない** (他モジュールから内部利用のみ)。
**理由**: Fix の FFI は C の関数呼び出しが基本で、termios/ioctl のような構造体を直接触るには C ラッパーが要る。openssl-fix の構成と一致させ、ビルドフローの学習コストを下げる。
**代替**: Rust 経由 → 依存増えるので不採用。

### 3. termios 構造体は `Array U8` の不透明バッファとして Fix に持つ

**選択**: `TermMode = box struct { saved : Array U8 }`。C 側で `sizeof(struct termios)` を返す関数 `fix_term_termios_size` を用意し、Fix 側はそのサイズの `Array U8` を確保して保存先 / 読み込み先として C に渡す。
**理由**: termios の中身 (c_iflag/c_oflag/c_cflag/c_lflag/c_cc 等) を Fix の型で表現する意味はゼロ。C 側でだけ意味を持てばよい。
**代替**: C 側に静的バッファ → 複数モード並存 (例: `enter_no_echo` の上に `enter_raw`) ができない、再入可能性なし。pass.

### 4. raw mode 復帰保証: `with_raw : (() -> IOFail a) -> IOFail a`

**選択**: 「raw mode に入る → ユーザ関数を呼ぶ → 必ず restore する」を高階関数で提供。例外が出ても `IOFail` の bind チェーンで cleanup を保証する。
**理由**: 「Ctrl+C で落ちたらターミナルが壊れる」という TUI 開発者の典型的事故を防ぐ。`enter_raw` / `restore` の生 API も併設するが、README で `with_raw` を強く推奨。
**追加保険**: C shim 側で `fix_term_install_atexit_restore` を提供…はせず Fix 層の責任にする。`atexit` は Fix のランタイムと相性悪い可能性があるため (要検証 → Open Questions)。

### 5. Ctrl+C の扱いは「2 モード提供」

**選択**: `enter_raw` (デフォルト, ISIG 維持) と `enter_raw_no_sig` (ISIG 解除) の 2 種類を提供。
- ISIG 維持: Ctrl+C → SIGINT がプロセスに飛ぶ。シンプルだが TUI 側で割り込みハンドラを別途仕込まないとターミナルが壊れる。
- ISIG 解除: Ctrl+C → 0x03 バイトとして `read(2)` から読める。`Key::parse` で `ctrl("c")` として認識可能。TUI アプリ向け。
**理由**: 用途で使い分けたい。`paste` 系の単発 raw mode 利用は ISIG 維持で十分だが、Yaynu のようなフル TUI は `enter_raw_no_sig` を選ぶ。

### 6. SIGWINCH ハンドラ: C shim 側でフラグだけ立てる

**選択**: `c_src/term_shim.c` に `static volatile sig_atomic_t g_winch_flag = 0;` を持ち、`fix_term_install_winch_handler` で `sigaction` 登録、ハンドラ内では `g_winch_flag = 1;` のみ。Fix 側からは `fix_term_check_winch_flag` で読み取り + クリア (test-and-clear)。
**理由**: シグナルハンドラ内では async-signal-safe な関数しか呼べないので、Fix のランタイム/GC をたたくのはご法度。フラグ立てだけならアセンブリ命令 1〜数個で済む。
**代替**: self-pipe trick → 実装複雑度に対して得るものが少ない。pass.

### 7. 入力タイムアウト: `select(2)` で 1 fd 監視

**選択**: `fix_term_read_byte_timeout(fd, timeout_ms, *out)` を C 側で実装。中で `select(2)` を `timeout_ms` で待ち、readable なら `read(fd, &out, 1)` で 1 バイト読む。戻り値で `0=timeout / 1=success / -1=error` を区別。
**理由**: `O_NONBLOCK` 設定方式は fd の永続的な状態変更が必要で他のコードへの副作用が懸念される。`select` は呼び出し単位で完結。
**注意**: stdin が pipe や redirect 経由で繋がっている場合 `read` が 0 を返す (EOF)。これは別途 `Option` で表現せず、まずは「raw mode = TTY」を前提に v0.1 では EOF を error として返す。

### 8. UTF-8 入力の組み立ては `Term::Key::parse` の責務

**選択**: `parse : Array U8 -> KeyParseResult` で先頭バイトのビットパターンから期待長を判定。続きが足りなければ `incomplete`、揃っていれば `char(<UTF-8 文字列>)` として返す。
**理由**: 入力読み取り側はバイト列を蓄積するだけにして、論理層は純粋関数で純粋にテストできる。UTF-8 デコードを `parse` に閉じることで境界条件のテスト網羅が容易。
**範囲**: BMP 外 (4 バイト UTF-8) も対応。サロゲートペア検証や Unicode 正規化はやらない。

### 9. ESC タイムアウト判断は **メインループの責務**

**選択**: `parse` は ESC 1 バイトだけ来た時点では `incomplete` を返す (続きを待つ可能性がある)。メインループ側で「ESC を読んだ後 50〜100ms 経っても次が来なければ ESC 単独」と確定させる。
**理由**: ライブラリ側が時間軸を握るのは抽象として弱い。`parse` を時間に依存しない純粋関数に保つ方が嬉しい。README に明示する。

### 10. `Term::Output::write_batch` は 1 つの文字列に結合してから 1 回 `write`

**選択**: 引数 `Array String` をまず concat してから単一の `write(2)` (stdout) + flush。
**理由**: ちらつき防止には複数のエスケープ + 描画内容を**カーネルから見て 1 回**で書き出すのが効果的。複数回 `write` を発行するとティアリングが起きうる。

## Risks / Trade-offs

- [Fix の FFI から signal-safe な C 関数を呼べることの確認が必要] → 初期段階で `c_src/term_shim.c` を最小構成で書いて smoke test。fix-programming スキル経由で FFI 呼び出しの作法を確認しつつ進める。
- [`atexit` で raw mode を自動復帰したいが Fix ランタイム的に怪しい] → 一旦 `with_raw` 推奨でしのぐ。検証して安全と分かれば後追いで導入。
- [stdin が TTY でない場合の挙動] → v0.1 では「raw mode 操作は TTY 前提」とドキュメントで明示し、TTY でないなら `IOFail` で失敗。
- [pty/screen/tmux などで一部 ANSI シーケンスが効かない] → 検出はしない (Non-Goal)。利用者がターゲット端末を把握する前提。
- [`Yaynu` 名前空間に置くことで「内部実装だから API を雑にしてもいい」という誘惑] → 設計レビューで「`Term` を `Yaynu` 外に出した時に困る依存があるか?」を毎回チェック。
- [16 色名 `bright_*` と SGR 番号のマッピングミス] → テストで全 16 色のエスケープ列を期待文字列と突き合わせる。
- [`Key::parse` のエッジケース (不完全 UTF-8、不正な ESC シーケンス、CSI のパラメータ違い等) のカバレッジ不足] → 単体テストで主要な ESC シーケンス (主要キー一覧) と境界 UTF-8 (1〜4 バイト、incomplete) をテーブル駆動でテスト。

## Migration Plan

新規ライブラリのため移行なし。導入手順:

1. `c_src/term_shim.c` と `scripts/build_shim.sh` を整備し、ローカルで `./scripts/build_shim.sh` がエラーなく走ることを確認
2. `fixproj.toml` に C shim のリンク設定 (静的ライブラリパス、必要なら `-lc` 系の追加リンクフラグ) を追加
3. `Yaynu::Term::Ffi`、`Yaynu::Term::Ansi`、`Yaynu::Term::Key` の順で実装 (純粋関数寄りから)
4. `fix test` で単体テストが通ることを CI 相当として手元で確認
5. `examples/echo_keys.fix` で raw mode + 入力 + Key 変換のスモーク確認
6. 残りモジュール (`Mode` / `Size` / `Input` / `Output`) を順次追加し、各 example を動かして手動検証
7. README に「初版は Yaynu サブモジュール、将来 `term` として抽出予定」と明記

ロールバック: ライブラリ追加のみで既存コードへの影響なし。問題があれば該当モジュールを削除して終了。

## Open Questions

- Fix での `atexit` 等価機構の有無 (raw mode 自動復帰の補強用) — 当面は `with_raw` でしのぎ、必要になったら調べる
- `fixproj.toml` での C 静的ライブラリリンクの正確な書き方 — openssl-fix の `fixproj.toml` を参照して移植する想定
- `select(2)` を `pselect(2)` に変えるべきか (シグナルとの競合回避) — v0.1 ではシンプルさ優先で `select`、SIGWINCH のレース条件が顕在化したら `pselect` に切り替え
- ESC シーケンスの方言 (xterm / Linux console / iTerm2 / Alacritty) — 主要 ESC は共通だがファンクションキーや修飾キー組み合わせで差がある。v0.1 は xterm 系互換 (本資料の ESC sequence 一覧) のみ
