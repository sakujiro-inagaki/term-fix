/* term_shim.c — tiny C helpers for term-fix (Yaynu::Term).
 *
 * Only functions Fix's FFI cannot express directly live here:
 *   - termios struct snapshots are too messy to model in Fix; the C
 *     side exposes an opaque size and accessor helpers operating on a
 *     caller-provided byte buffer
 *   - sigaction(2) registration for SIGWINCH; the handler does only
 *     async-signal-safe work (set a `volatile sig_atomic_t` flag)
 *   - select(2)-based single-byte read with timeout
 *
 * Targets POSIX (Linux / macOS). Windows is unsupported in v0.1.
 */

#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

/* ---- forward declarations ---- */
int fix_term_termios_size(void);
int fix_term_get_attr(int fd, void *termios_buf);
int fix_term_set_attr(int fd, int when, void *termios_buf);
int fix_term_make_raw(void *termios_buf);
int fix_term_make_no_echo(void *termios_buf);
int fix_term_make_raw_no_sig(void *termios_buf);
int fix_term_get_size(int fd, int *rows, int *cols);
int fix_term_install_winch_handler(void);
int fix_term_check_winch_flag(void);
int fix_term_read_byte_timeout(int fd, int timeout_ms, unsigned char *out);
int fix_term_strerror(char *out, int cap);

/* ---- termios snapshot / restore ---- */

int fix_term_termios_size(void) {
    return (int)sizeof(struct termios);
}

int fix_term_get_attr(int fd, void *termios_buf) {
    return tcgetattr(fd, (struct termios *)termios_buf);
}

int fix_term_set_attr(int fd, int when, void *termios_buf) {
    return tcsetattr(fd, when, (struct termios *)termios_buf);
}

/* ---- mode mutators (operate on a saved termios buffer in place) ---- */

/* Raw-mode flavour that keeps ISIG enabled so Ctrl+C still raises SIGINT.
 * Disables canonical input (ICANON) and echo (ECHO). Leaves c_cc[VMIN]
 * and c_cc[VTIME] at their saved values; the high-level wrapper uses a
 * select(2) timeout, not VMIN/VTIME, for input pacing. */
int fix_term_make_raw(void *termios_buf) {
    struct termios *t = (struct termios *)termios_buf;
    t->c_lflag &= ~(tcflag_t)(ICANON | ECHO);
    return 0;
}

/* Disable echo only — suitable for password prompts. Canonical input
 * (line buffering) and signal generation remain on. */
int fix_term_make_no_echo(void *termios_buf) {
    struct termios *t = (struct termios *)termios_buf;
    t->c_lflag &= ~(tcflag_t)ECHO;
    return 0;
}

/* Raw mode AND signal-generation disabled. Ctrl+C is delivered to
 * read(2) as the byte 0x03 rather than raising SIGINT. Use for TUI
 * applications that handle Ctrl+C themselves. */
int fix_term_make_raw_no_sig(void *termios_buf) {
    struct termios *t = (struct termios *)termios_buf;
    t->c_lflag &= ~(tcflag_t)(ICANON | ECHO | ISIG);
    return 0;
}

/* ---- terminal size via TIOCGWINSZ ---- */

int fix_term_get_size(int fd, int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(fd, TIOCGWINSZ, &ws) != 0) {
        return -1;
    }
    if (rows != 0) { *rows = (int)ws.ws_row; }
    if (cols != 0) { *cols = (int)ws.ws_col; }
    return 0;
}

/* ---- SIGWINCH handler: async-signal-safe flag setter ---- */

static volatile sig_atomic_t g_winch_flag = 0;

static void fix_term__winch_handler(int sig) {
    (void)sig;
    g_winch_flag = 1;
}

int fix_term_install_winch_handler(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = fix_term__winch_handler;
    sigemptyset(&sa.sa_mask);
    /* SA_RESTART so blocking syscalls (e.g. select) restart rather than
     * fail with EINTR after a window resize. The caller still observes
     * the resize via fix_term_check_winch_flag. */
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGWINCH, &sa, 0) != 0) {
        return -1;
    }
    return 0;
}

/* Test-and-clear the flag. The flag is `sig_atomic_t` so simple
 * read/write is async-signal-safe; at most one resize can be lost if
 * a signal arrives between the read and the write below. The spec
 * permits coalescing repeated resizes into a single `true`. */
int fix_term_check_winch_flag(void) {
    if (g_winch_flag != 0) {
        g_winch_flag = 0;
        return 1;
    }
    return 0;
}

/* ---- single-byte read with timeout via select(2) ---- */

/* Returns 1 if a byte was read (written to *out), 0 on timeout, -1 on
 * syscall error (errno is left as-is by the failing syscall). A
 * timeout_ms of 0 polls without blocking. Negative timeout_ms is not
 * supported; behaviour is undefined in v0.1. */
int fix_term_read_byte_timeout(int fd, int timeout_ms, unsigned char *out) {
    fd_set readfds;
    struct timeval tv;
    int rc;

    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);

    tv.tv_sec  = (long)(timeout_ms / 1000);
    tv.tv_usec = (long)((timeout_ms % 1000) * 1000);

    do {
        rc = select(fd + 1, &readfds, 0, 0, &tv);
    } while (rc < 0 && errno == EINTR);

    if (rc < 0) { return -1; }
    if (rc == 0) { return 0; }

    if (!FD_ISSET(fd, &readfds)) { return 0; }

    ssize_t n;
    do {
        n = read(fd, out, 1);
    } while (n < 0 && errno == EINTR);

    if (n < 0) { return -1; }
    if (n == 0) {
        /* EOF on a pipe/redirect — treat as syscall error per spec
         * (raw mode is documented as TTY-only in v0.1). */
        errno = EIO;
        return -1;
    }
    return 1;
}

/* Copies strerror(errno) into the caller's buffer (NUL-terminated,
 * truncated to fit). Returns the errno value at entry.
 *
 * Intended to be called immediately after a failing shim function so
 * that errno still reflects the originating syscall. Fix code routes
 * through this rather than reading errno directly because the Fix
 * runtime offers no portable errno accessor. */
int fix_term_strerror(char *out, int cap) {
    int err = errno;
    if (out != 0 && cap > 0) {
        const char *msg = strerror(err);
        if (msg == 0) {
            out[0] = '\0';
        } else {
            size_t len = strlen(msg);
            if ((int)len >= cap) { len = (size_t)(cap - 1); }
            memcpy(out, msg, len);
            out[len] = '\0';
        }
    }
    return err;
}
