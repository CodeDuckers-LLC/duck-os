#include "kernel/console.h"
#include "kernel/klog.h"
#include "kernel/shell.h"

#define SHELL_LINE_MAX 64U

void shell_run(void)
{
    char line[SHELL_LINE_MAX];

    klog_info("shell ready");

    while (1)
    {
        console_write("duck-os@dev > ");
        console_read_line(line, sizeof(line));
        shell_execute_line(line, 0, 0);
    }
}
