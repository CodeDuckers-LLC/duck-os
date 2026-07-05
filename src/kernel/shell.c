#include "kernel/console.h"
#include "kernel/initramfs.h"
#include "kernel/klog.h"
#include "kernel/kmalloc.h"
#include "kernel/memory_layout.h"
#include "kernel/panic.h"
#include "kernel/shell.h"
#include "kernel/timer.h"
#include "lib/string.h"
#include "mm/pmm.h"

#define SHELL_LINE_MAX 64
#define KERNEL_VERSION "duck-os"

static void shell_print_help(void)
{
    kprintf("help\n");
    kprintf("version\n");
    kprintf("ls\n");
    kprintf("cat <file>\n");
    kprintf("mem\n");
    kprintf("uptime\n");
    kprintf("ticks\n");
    kprintf("panic\n");
}

static void shell_print_version(void)
{
    kprintf("%s\n", KERNEL_VERSION);
}

static void shell_print_mem(void)
{
    unsigned long ram_start;
    unsigned long ram_end;
    unsigned long kernel_start;
    unsigned long kernel_end;
    unsigned long heap_used;
    unsigned long heap_free;

    ram_start = memory_layout_ram_start();
    ram_end = memory_layout_ram_end();
    kernel_start = memory_layout_kernel_start();
    kernel_end = memory_layout_kernel_end();
    heap_used = kmalloc_used();
    heap_free = ram_end - kernel_end - heap_used;

    kprintf("RAM start: %p\n", (void *)ram_start);
    kprintf("RAM end: %p\n", (void *)ram_end);
    kprintf("kernel start: %p\n", (void *)kernel_start);
    kprintf("kernel end: %p\n", (void *)kernel_end);
    kprintf("heap used: %u bytes\n", (unsigned int)heap_used);
    kprintf("heap free: %u bytes\n", (unsigned int)heap_free);
    kprintf("pmm total pages: %u\n", (unsigned int)pmm_total_pages());
    kprintf("pmm used pages: %u\n", (unsigned int)pmm_used_pages());
    kprintf("pmm free pages: %u\n", (unsigned int)pmm_free_pages());
}

static void shell_print_uptime(void)
{
    unsigned long uptime_ms;
    unsigned long seconds;
    unsigned long milliseconds;

    uptime_ms = timer_uptime_ms();
    seconds = uptime_ms / 1000UL;
    milliseconds = uptime_ms % 1000UL;

    kprintf("uptime: %u.", (unsigned int)seconds);
    if (milliseconds < 100UL)
    {
        console_putc('0');
    }
    if (milliseconds < 10UL)
    {
        console_putc('0');
    }
    kprintf("%u s\n", (unsigned int)milliseconds);
}

static void shell_print_ticks(void)
{
    kprintf("ticks: %u\n", (unsigned int)timer_irq_count());
}

static int shell_starts_with(const char *text, const char *prefix)
{
    while (*prefix != '\0')
    {
        if (*text != *prefix)
        {
            return 0;
        }

        text++;
        prefix++;
    }

    return 1;
}

static const char *shell_skip_spaces(const char *text)
{
    while (*text == ' ')
    {
        text++;
    }

    return text;
}

static void shell_list_files(void)
{
    initramfs_list();
}

static void shell_cat_file(const char *name)
{
    const struct initramfs_file *file;
    const unsigned char *data;
    unsigned long i;

    if (*name == '\0')
    {
        kprintf("usage: cat <file>\n");
        return;
    }

    file = initramfs_find(name);
    if (file == 0)
    {
        kprintf("file not found: %s\n", name);
        return;
    }

    data = initramfs_read(file);
    for (i = 0; i < file->size; i++)
    {
        console_putc((char)data[i]);
    }

    if (file->size == 0 || data[file->size - 1] != '\n')
    {
        console_putc('\n');
    }
}

static void shell_run_command(const char *line)
{
    if (strcmp(line, "") == 0)
    {
        return;
    }

    if (strcmp(line, "help") == 0)
    {
        shell_print_help();
        return;
    }

    if (strcmp(line, "version") == 0)
    {
        shell_print_version();
        return;
    }

    if (strcmp(line, "ls") == 0)
    {
        shell_list_files();
        return;
    }

    if (strcmp(line, "mem") == 0)
    {
        shell_print_mem();
        return;
    }

    if (strcmp(line, "uptime") == 0)
    {
        shell_print_uptime();
        return;
    }

    if (strcmp(line, "ticks") == 0)
    {
        shell_print_ticks();
        return;
    }

    if (strcmp(line, "panic") == 0)
    {
        panic("panic command");
    }

    if (shell_starts_with(line, "cat"))
    {
        shell_cat_file(shell_skip_spaces(line + 3));
        return;
    }

    kprintf("unknown command: %s\n", line);
}

void shell_run(void)
{
    char line[SHELL_LINE_MAX];

    klog_info("shell ready");

    while (1)
    {
        console_write("duck-os@dev > ");
        console_read_line(line, sizeof(line));
        shell_run_command(line);
    }
}
