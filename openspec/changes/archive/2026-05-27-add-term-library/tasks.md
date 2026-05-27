## 1. プロジェクト土台

- [x] 1.1 `fixproj.toml` を作成 (プロジェクト名 `term`、モジュールルート `src/`、Std のみ依存、C 静的ライブラリ `c_src/libterm_shim.a` をリンク設定)
- [x] 1.2 `.gitignore` に C ビルド成果物 (`*.o`、`*.a`、`c_src/build/`) と Fix のビルド成果物を追加
- [x] 1.3 `c_src/` ディレクトリと `scripts/` ディレクトリを作成

## 2. C shim 実装

- [x] 2.1 `c_src/term_shim.c` を作成し、ヘッダ (`termios.h`, `sys/ioctl.h`, `signal.h`, `sys/select.h`, `unistd.h`, `errno.h`) と関数プロトタイプを記述
- [x] 2.2 `int fix_term_termios_size(void)` を実装 (`sizeof(struct termios)` を返す)
- [x] 2.3 `int fix_term_get_attr(int fd, void* termios_buf)` を実装 (`tcgetattr`)
- [x] 2.4 `int fix_term_set_attr(int fd, int when, void* termios_buf)` を実装 (`tcsetattr`)
- [x] 2.5 `int fix_term_make_raw(void* termios_buf)` を実装 (`ICANON` と `ECHO` をクリア、`ISIG` は維持)
- [x] 2.6 `int fix_term_make_no_echo(void* termios_buf)` を実装 (`ECHO` のみクリア)
- [x] 2.7 `int fix_term_make_raw_no_sig(void* termios_buf)` を実装 (`ICANON`/`ECHO`/`ISIG` クリア)
- [x] 2.8 `int fix_term_get_size(int fd, int* rows, int* cols)` を実装 (`ioctl(TIOCGWINSZ)`)
- [x] 2.9 `static volatile sig_atomic_t g_winch_flag` を宣言、`int fix_term_install_winch_handler(void)` を `sigaction` で実装 (ハンドラはフラグ立てのみ)
- [x] 2.10 `int fix_term_check_winch_flag(void)` を実装 (フラグを test-and-clear、`async-signal-safe`)
- [x] 2.11 `int fix_term_read_byte_timeout(int fd, int timeout_ms, unsigned char* out)` を `select(2)` ベースで実装、戻り値は `-1/0/1`
- [x] 2.12 `scripts/build_shim.sh` を作成 (`cc -O2 -fPIC -c term_shim.c -o term_shim.o && ar rcs libterm_shim.a term_shim.o`、openssl-fix のスクリプトを参照)
- [x] 2.13 `./scripts/build_shim.sh` を手元で実行し `c_src/libterm_shim.a` が生成されることを確認

## 3. Yaynu::Term::Ffi (内部モジュール)

- [x] 3.1 `src/Yaynu/Term/Ffi.fix` を作成、`module Yaynu.Term.Ffi;` ヘッダを記述
- [x] 3.2 `fix_term_termios_size`、`fix_term_get_attr`、`fix_term_set_attr` の `FFI_CALL` 宣言を追加
- [x] 3.3 `fix_term_make_raw`、`fix_term_make_no_echo`、`fix_term_make_raw_no_sig` の宣言を追加
- [x] 3.4 `fix_term_get_size` の宣言を追加 (`I32` ポインタ 2 つの out 引数)
- [x] 3.5 `fix_term_install_winch_handler`、`fix_term_check_winch_flag` の宣言を追加
- [x] 3.6 `fix_term_read_byte_timeout` の宣言を追加 (`U8` ポインタ 1 つの out 引数)
- [x] 3.7 `Yaynu::Term::Ffi` を他モジュールから呼ぶ最小サンプル (`Yaynu::Term::Ffi::termios_size` を返すだけ) を書いて `fix run` で動作確認

## 4. Yaynu::Term::Ansi 実装 + 単体テスト

- [x] 4.1 `src/Yaynu/Term/Ansi.fix` を作成、`Color16` 型 (16 variants) を定義
- [x] 4.2 `esc`、`csi` 定数を実装
- [x] 4.3 画面制御定数 (`clear_screen`、`clear_line`、`clear_to_end_of_line`、`clear_to_start_of_line`、`enter_alternate_screen`、`leave_alternate_screen`) を実装
- [x] 4.4 カーソル制御 (`move_to`、`move_to_col`、`move_up`、`move_down`、`move_right`、`move_left`、`hide_cursor`、`show_cursor`、`save_cursor`、`restore_cursor`) を実装
- [x] 4.5 スタイル定数 (`reset`、`bold`、`dim`、`italic`、`underline`、`blink`、`reverse`) を実装
- [x] 4.6 `fg_color_16`、`bg_color_16` を 16 色全てで実装 (パターンマッチで各 variant → SGR コードへ)
- [x] 4.7 `fg_color_256`、`bg_color_256` を実装
- [x] 4.8 `fg_color_rgb`、`bg_color_rgb` を実装
- [x] 4.9 `paint` 関数を実装
- [x] 4.10 `tests/TermTest.fix` を作成し、`test_ansi_*` 関数群で 4.2〜4.9 の戻り値を期待文字列と突き合わせるテストを書く
- [x] 4.11 `fix test` を走らせて Ansi テストが全てグリーンであることを確認

## 5. Yaynu::Term::Key 実装 + 単体テスト

- [x] 5.1 `src/Yaynu/Term/Key.fix` を作成、`Key` 型と `KeyParseResult` 型を定義
- [x] 5.2 `parse : Array U8 -> KeyParseResult` の空入力ケース (`incomplete`) を実装
- [x] 5.3 印字可能 ASCII (`0x20..0x7E`) の `char` ケースを実装
- [x] 5.4 特殊制御バイト (`0x0D`/`0x0A` → enter、`0x09` → tab、`0x7F`/`0x08` → backspace) を実装
- [x] 5.5 `0x01..0x1A` のうち上記以外を `ctrl(<letter>)` に変換するロジックを実装
- [x] 5.6 UTF-8 マルチバイト (2/3/4 バイト) の組み立てロジックを実装 (incomplete / invalid / complete の三分岐)
- [x] 5.7 ESC + 印字可能 ASCII → `alt(<char>)` のケースを実装
- [x] 5.8 ESC のみ → `incomplete` のケースを実装
- [x] 5.9 CSI (`ESC [ ...`) 矢印 / ナビゲーション (A/B/C/D/H/F) のケースを実装
- [x] 5.10 CSI `n ~` 系 (1/2/3/4/5/6) のケースを実装 (home/insert/delete_key/end/page_up/page_down)
- [x] 5.11 CSI `1 5 ~` 〜 `2 4 ~` のファンクションキー F5..F12 のケースを実装
- [x] 5.12 SS3 (`ESC O P/Q/R/S`) で F1..F4 のケースを実装
- [x] 5.13 CSI 不完全 (`ESC [`、`ESC [ 1`、`ESC O` 等) → `incomplete` のケースを実装
- [x] 5.14 不正な CSI / SS3 終端子 → `invalid(n)` のケースを実装
- [x] 5.15 単独継続バイト (`0x80..0xBF`) → `invalid(1)` のケースを実装
- [x] 5.16 `to_string : Key -> String` を実装 (各 variant を区別できる表現)
- [x] 5.17 `tests/TermTest.fix` に `test_key_*` テスト群を追加 (印字可能 ASCII、特殊制御、Ctrl、Alt、矢印、ナビ、ファンクションキー、UTF-8 1/2/3/4 バイト、incomplete、invalid)
- [x] 5.18 `fix test` を走らせて Ansi + Key の全テストがグリーンであることを確認

## 6. Yaynu::Term::Mode 実装

- [x] 6.1 `src/Yaynu/Term/Mode.fix` を作成、`TermMode` 型 (`box struct { saved : Array U8 }`) を定義
- [x] 6.2 `save : IOFail TermMode` を実装 (`Yaynu::Term::Ffi::termios_size` でサイズ確保、`fix_term_get_attr(0, buf)` を呼ぶ、失敗時 `tcgetattr: errno=...` でフェイル)
- [x] 6.3 `restore : TermMode -> IOFail ()` を実装 (`fix_term_set_attr(0, TCSANOW, buf)`、失敗時フェイル)
- [x] 6.4 `enter_raw : IOFail TermMode` を実装 (`save` の結果を保存しつつ、コピーした buf を `fix_term_make_raw` で変更して `set_attr`)
- [x] 6.5 `enter_no_echo : IOFail TermMode` を実装
- [x] 6.6 `enter_raw_no_sig : IOFail TermMode` を実装
- [x] 6.7 `with_raw : [a] (() -> IOFail a) -> IOFail a` を実装。`enter_raw` → action 実行 → 成功/失敗いずれも `restore` を呼ぶ。`restore` のエラーは action のエラーに比べて優先度が低い
- [x] 6.8 `examples/echo_keys.fix` を作成。`with_raw` 内で `read_byte_timeout` (まだ未実装なので一旦は単純な `read_byte_nonblock` 相当を仮置きしてもよい) と `Key::parse` を組み合わせ、押されたキーを `Key::to_string` で表示。`Ctrl+C` で正常終了する想定。後続タスクで `Input` 実装後に完成させる

## 7. Yaynu::Term::Size 実装

- [x] 7.1 `src/Yaynu/Term/Size.fix` を作成、`TermSize = struct { rows : I64, cols : I64 }` を定義
- [x] 7.2 `get : IOFail TermSize` を実装 (`fix_term_get_size(1, &rows, &cols)`、失敗時 `ioctl TIOCGWINSZ: ...` でフェイル)
- [x] 7.3 `install_resize_handler : IO ()` を実装 (`fix_term_install_winch_handler` をラップ)
- [x] 7.4 `check_resized : IO Bool` を実装 (`fix_term_check_winch_flag` を呼び 1/0 を `Bool` 化)
- [x] 7.5 `examples/show_size.fix` を作成。`install_resize_handler` 後、メインループで 200ms 毎に `check_resized` をポーリングし、true なら `get` で再取得して表示。`Ctrl+C` で終了

## 8. Yaynu::Term::Input 実装

- [x] 8.1 `src/Yaynu/Term/Input.fix` を作成
- [x] 8.2 `read_byte_timeout : I64 -> IOFail (Option U8)` を実装 (`fix_term_read_byte_timeout(0, ms, &out)` を呼び 0/1/-1 を `None`/`Some`/フェイルにマップ)
- [x] 8.3 `read_byte_nonblock : IOFail (Option U8)` を `read_byte_timeout(0)` で実装
- [x] 8.4 `read_available : IOFail (Array U8)` を実装 (`read_byte_nonblock` を `None` が返るまで繰り返し、結果を `Array U8` に集める)
- [x] 8.5 `examples/echo_keys.fix` を完成させる (タスク 6.8 の仮実装を `read_byte_timeout` + 蓄積バッファ + `Key::parse` の本実装に置き換え、ESC 単独タイムアウトは 80ms で `Key::escape` を確定)

## 9. Yaynu::Term::Output 実装

- [x] 9.1 `src/Yaynu/Term/Output.fix` を作成
- [x] 9.2 `write : String -> IOFail ()` を実装 (`Std::IO::print` 相当 + 明示 flush)
- [x] 9.3 `write_batch : Array String -> IOFail ()` を実装 (`Array::to_iter` + `String::join("")` で連結してから単一 `write` + flush)
- [x] 9.4 既存 examples (`echo_keys`、`show_size`) の出力箇所を `Yaynu::Term::Output::write` / `write_batch` に置き換え

## 10. 公開ファサードと残り examples

- [x] 10.1 `src/Yaynu/Term.fix` を作成し、`Yaynu::Term::Ansi`、`Mode`、`Size`、`Input`、`Key`、`Output` を再エクスポート (`Yaynu::Term::Ffi` は含めない)
- [x] 10.2 `examples/colors.fix` を作成。16 色全て、256 色グリッド (16x16)、RGB グラデーション帯を表示
- [x] 10.3 `examples/cursor.fix` を作成。`clear_screen` + `move_to` + テキスト出力で 5x5 のグリッドを描画、`hide_cursor`/`show_cursor` のデモ、3 秒待って終了

## 11. 動作確認と完了条件

- [x] 11.1 `fix test` を走らせて Ansi と Key の全単体テストがグリーンであることを再確認
- [x] 11.2 `examples/echo_keys.fix` を実行し、矢印・ファンクションキー (F1〜F12)・Ctrl+a〜z・Alt+文字・UTF-8 文字 (例: あ、🙂) が正しく検出されることを目視確認
- [x] 11.3 `examples/show_size.fix` を実行し、ターミナルウィンドウをリサイズした際に表示が更新されることを目視確認
- [x] 11.4 `examples/colors.fix` を実行し、16 色 / 256 色 / RGB が全て期待通りに表示されることを目視確認
- [x] 11.5 `examples/cursor.fix` を実行し、カーソル移動と画面クリアが期待通りに動作することを目視確認
- [x] 11.6 raw mode 中に Ctrl+C で `echo_keys.fix` を強制終了させた際 (`enter_raw` 系)、ターミナルが正常状態 (canonical + echo on) に戻ることを確認
- [x] 11.7 README.md を作成 (スコープ・非スコープ、対応プラットフォーム、依存 (Std のみ)、基本的な使い方コード例、`with_raw` 推奨の理由、ESC タイムアウトの説明、`examples/` の動作確認手順、v0.1 の制限 (Windows 非対応)、初版は `Yaynu` 名前空間配下で将来 `Term::*` に rename 予定である旨)
