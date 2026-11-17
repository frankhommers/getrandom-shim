/*
 * install - copies /getrandom_shim.so to /shim/getrandom_shim.so (mode 0644)
 * and exits. Static, no libc, so the image can be FROM scratch.
 */

#include "sys.h"

#define SRC "/getrandom_shim.so"
#define DST "/shim/getrandom_shim.so"

#if defined(__x86_64__)
__asm__(".globl _start\n_start:\n"
        "  xor %rbp, %rbp\n  and $-16, %rsp\n  call cmain\n"
        "  mov %eax, %edi\n  mov $60, %eax\n  syscall\n");
#elif defined(__aarch64__)
__asm__(".globl _start\n_start:\n"
        "  mov x29, #0\n  bl cmain\n  mov x8, #93\n  svc 0\n");
#endif

static void say(const char *s)
{
    size_t n = 0;
    while (s[n])
        n++;
    sys4(SYS_write, 2, (long)s, (long)n, 0);
}

static long retry(long n, long a, long b, long c, long d)
{
    long r;
    do
        r = sys4(n, a, b, c, d);
    while (r == -E_INTR);
    return r;
}

int cmain(void)
{
    static unsigned char buf[65536];
    long in, out, r;

    in = retry(SYS_openat, AT_FDCWD_, (long)SRC, O_RDONLY_ | O_CLOEXEC_, 0);
    if (in < 0) {
        say("install: cannot open " SRC "\n");
        return 1;
    }
    out = retry(SYS_openat, AT_FDCWD_, (long)DST,
                O_WRONLY_ | O_CREAT_ | O_TRUNC_ | O_CLOEXEC_, 0644);
    if (out < 0) {
        say("install: cannot create " DST " (is a volume mounted on /shim?)\n");
        return 1;
    }
    while ((r = retry(SYS_read, in, (long)buf, sizeof buf, 0)) > 0) {
        long off = 0;
        while (off < r) {
            long w = retry(SYS_write, out, (long)(buf + off), r - off, 0);
            if (w <= 0) {
                say("install: write failed\n");
                return 1;
            }
            off += w;
        }
    }
    if (r < 0) {
        say("install: read failed\n");
        return 1;
    }
    /* O_CREAT mode is masked by umask; set 0644 explicitly. */
    sys4(SYS_fchmod, out, 0644, 0, 0);
    if (sys4(SYS_close, out, 0, 0, 0) < 0) {
        say("install: close failed\n");
        return 1;
    }
    say("getrandom_shim.so installed in /shim\n");
    return 0;
}
