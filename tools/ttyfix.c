/*
 * ttyfix - 串口/终端 termios 修复与诊断工具 (T113 Tina Linux)
 *
 * 背景: 板上没有 stty, 串口控制台 ttyS3 的 ISIG 被关闭后,
 *       Ctrl+C / Ctrl+Z 不再产生信号, 无法中断前台程序。
 *
 * 用法 (板上执行):
 *   ttyfix            修复当前终端 (从串口控制台运行, 修复自己的 tty)
 *   ttyfix /dev/ttyS3 修复指定 tty (从 adb 运行, 修串口控制台)
 *   ttyfix dump       打印当前终端 termios 标志
 *   ttyfix dump /dev/ttyS3
 *   ttyfix inject 03 /dev/ttyS3   向 tty 输入队列注入一个字节 (测试 ISIG)
 *
 * 修复内容 = 恢复标准控制台状态:
 *   c_lflag |= ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE
 *   VINTR=0x03(^C)  VSUSP=0x1a(^Z)  VERASE=0x7f(退格)  VKILL=0x15(^U)  VEOF=0x04(^D)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>

static void dump_flags(const char *name, int fd)
{
    struct termios t;
    if (tcgetattr(fd, &t) < 0) {
        perror("tcgetattr");
        return;
    }
    printf("[%s]\n", name);
    printf("  c_iflag=0x%08x c_oflag=0x%08x c_cflag=0x%08x c_lflag=0x%08x\n",
           t.c_iflag, t.c_oflag, t.c_cflag, t.c_lflag);
    printf("  ISIG=%d ICANON=%d ECHO=%d ECHOE=%d ECHOCTL=%d IXON=%d IXOFF=%d\n",
           !!(t.c_lflag & ISIG), !!(t.c_lflag & ICANON), !!(t.c_lflag & ECHO),
           !!(t.c_lflag & ECHOE), !!(t.c_lflag & ECHOCTL),
           !!(t.c_iflag & IXON), !!(t.c_iflag & IXOFF));
    printf("  VINTR=0x%02x(^C) VERASE=0x%02x VKILL=0x%02x(^U) VSUSP=0x%02x(^Z) VEOF=0x%02x(^D)\n",
           t.c_cc[VINTR], t.c_cc[VERASE], t.c_cc[VKILL],
           t.c_cc[VSUSP], t.c_cc[VEOF]);
    printf("  -> %s\n",
           (t.c_lflag & ISIG) ? "ISIG 已开, Ctrl+C 应能中断前台进程"
                              : "ISIG 已关, Ctrl+C 只回显不产生信号 (需要修复)");
}

static int open_tty(const char *path)
{
    int fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        perror("open");
        return -1;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) & ~O_NONBLOCK);
    return fd;
}

static int do_fix(const char *path)
{
    int fd = open_tty(path);
    if (fd < 0) return 1;

    struct termios t;
    if (tcgetattr(fd, &t) < 0) {
        perror("tcgetattr");
        close(fd);
        return 1;
    }

    t.c_lflag |= ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE;
    t.c_cc[VINTR]   = 0x03; /* ^C  */
    t.c_cc[VSUSP]   = 0x1a; /* ^Z  */
    t.c_cc[VERASE]  = 0x7f; /* 退格 */
    t.c_cc[VKILL]   = 0x15; /* ^U  */
    t.c_cc[VEOF]    = 0x04; /* ^D  */

    if (tcsetattr(fd, TCSANOW, &t) < 0) {
        perror("tcsetattr");
        close(fd);
        return 1;
    }

    printf("[ttyfix] %s: 已恢复 ISIG|ICANON|ECHO 等标准控制台标志\n", path);
    dump_flags(path, fd);
    close(fd);
    return 0;
}

static int do_inject(const char *path, unsigned char c)
{
    int fd = open_tty(path);
    if (fd < 0) return 1;

    /* TIOCSTI: 把字节注入 tty 的输入队列, 等效于用户在键盘上按下该键 */
    if (ioctl(fd, TIOCSTI, &c) < 0) {
        perror("TIOCSTI");
        close(fd);
        return 1;
    }
    printf("[ttyfix] 已注入 0x%02x 到 %s (等效按下一个键)\n", c, path);
    close(fd);
    return 0;
}

int main(int argc, char *argv[])
{
    /* 无参数: 修复当前终端 (fd 0) */
    if (argc == 1) {
        printf("[ttyfix] 修复当前终端 (stdin)\n");
        dump_flags("before", 0);
        struct termios t;
        if (tcgetattr(0, &t) < 0) { perror("tcgetattr"); return 1; }
        t.c_lflag |= ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE;
        t.c_cc[VINTR] = 0x03; t.c_cc[VSUSP] = 0x1a;
        t.c_cc[VERASE] = 0x7f; t.c_cc[VKILL] = 0x15; t.c_cc[VEOF] = 0x04;
        if (tcsetattr(0, TCSANOW, &t) < 0) { perror("tcsetattr"); return 1; }
        printf("[ttyfix] 已恢复 ISIG|ICANON|ECHO\n");
        dump_flags("after", 0);
        return 0;
    }

    if (strcmp(argv[1], "dump") == 0) {
        const char *path = (argc > 2) ? argv[2] : "/dev/ttyS3";
        int fd = open_tty(path);
        if (fd < 0) return 1;
        dump_flags(path, fd);
        close(fd);
        return 0;
    }

    if (strcmp(argv[1], "inject") == 0 && argc >= 3) {
        const char *path = (argc > 3) ? argv[3] : "/dev/ttyS3";
        return do_inject(path, (unsigned char) strtol(argv[2], NULL, 16));
    }

    /* 默认: fix <path> */
    return do_fix(argv[1]);
}
