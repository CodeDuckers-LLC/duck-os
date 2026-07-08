#include "drivers/pci.h"
#include "block/block_device.h"
#include "desktop/app_registry.h"
#include "desktop/desktop.h"
#include "drivers/virtio.h"
#include "drivers/virtio_gpu.h"
#include "drivers/virtio_rng.h"
#include "fs/file.h"
#include "fs/logfs.h"
#include "fs/vfs.h"
#include "gfx/cursor.h"
#include "gfx/framebuffer.h"
#include "gui/app_file_browser.h"
#include "gui/app_text_viewer.h"
#include "gui/gui.h"
#include "kernel/console.h"
#include "kernel/input.h"
#include "kernel/klog.h"
#include "kernel/kmalloc.h"
#include "kernel/memory_layout.h"
#include "kernel/panic.h"
#include "kernel/shell.h"
#include "kernel/timer.h"
#include "kernel/user.h"
#include "lib/string.h"
#include "mm/pmm.h"

#define SHELL_FILE_BUFFER_MAX 128
#define SHELL_BLOCK_DUMP_BYTES 512
#define SHELL_HEXDUMP_BYTES_PER_LINE 16U
#define KERNEL_VERSION "duck-os"

typedef struct shell_capture_state
{
    shell_output_callback_t callback;
    void *user_data;
} shell_capture_state_t;

static const char *shell_read_token(const char *text, char *buffer, unsigned long buffer_size);
static int shell_parse_unsigned_long(const char *text, unsigned long *value_out);
static void shell_dump_hex_line(unsigned long offset, const unsigned char *buffer, unsigned int count);
static const char *shell_skip_spaces(const char *text);
static int shell_command_matches(const char *line, const char *command);
static void shell_print_hex_byte(unsigned char value);
static void shell_print_path_error(const char *action, const char *path);
static void shell_run_gfx_test(void);
static void shell_set_console_mode(const char *mode);
static void shell_set_input_mode(const char *mode);
static void shell_move_cursor(const char *args);
static void shell_run_gui_demo(void);
static void shell_run_gui_files(const char *args);
static void shell_run_gui_view(const char *args);
static void shell_run_desktop(const char *args);
static void shell_list_apps(void);
static void shell_open_desktop_file(const char *path);
static int shell_list_entry(const vfs_file_info_t *entry, void *context);
static void shell_run_command(const char *line);
static void shell_output_capture_char(char ch, void *user_data);

static void shell_output_capture_char(char ch, void *user_data)
{
    shell_capture_state_t *state;

    state = (shell_capture_state_t *)user_data;
    if (state == 0 || state->callback == 0)
    {
        return;
    }

    state->callback(&ch, 1U, state->user_data);
}

static void shell_print_help(void)
{
    kprintf("duck-os shell commands\n");
    kprintf("\n");
    kprintf("filesystem:\n");
    kprintf("  help                 show this command reference\n");
    kprintf("  pwd                  print the current directory\n");
    kprintf("  ls [path]            list files in / or in a directory\n");
    kprintf("  cat <path>           print a text file to the console\n");
    kprintf("  stat <path>          show file type, size, and canonical path\n");
    kprintf("  hexdump <path>       print file contents in hex + ascii\n");
    kprintf("  open <path>          open file in desktop app by extension\n");
    kprintf("  fsinfo               show mounted root filesystem details\n");
    kprintf("\n");
    kprintf("graphics and gui:\n");
    kprintf("  ginfo                show framebuffer and gpu details\n");
    kprintf("  gclear               clear the graphics framebuffer\n");
    kprintf("  gtest                run an in-memory framebuffer self-test\n");
    kprintf("  gpudemo              redraw the built-in graphics demo\n");
    kprintf("  cursor <x> <y>       move the graphics cursor\n");
    kprintf("  desktop [once|state] run desktop mode, draw one frame, or print state\n");
    kprintf("  apps                 list built-in graphical apps\n");
    kprintf("  gui demo             draw demo GUI windows\n");
    kprintf("  gui files [index]    open the file browser and preselect an entry\n");
    kprintf("  gui view <path>      open a text file in the GUI viewer\n");
    kprintf("  console [mode]       show or set output: serial|graphics|both\n");
    kprintf("  input [mode]         show or set input: serial|keyboard|both\n");
    kprintf("\n");
    kprintf("system:\n");
    kprintf("  version              print kernel version\n");
    kprintf("  mem                  show RAM, heap, and PMM usage\n");
    kprintf("  uptime               show time since boot\n");
    kprintf("  ticks                show timer interrupt count\n");
    kprintf("  blk                  list block devices\n");
    kprintf("  blkread <dev> <n>    dump one block from a device\n");
    kprintf("  pci                  list PCI devices\n");
    kprintf("  virtio               list virtio devices\n");
    kprintf("  rng                  read a few random bytes\n");
    kprintf("\n");
    kprintf("programs and logs:\n");
    kprintf("  run <path>           run a user program image\n");
    kprintf("  runelf <path>        run a user ELF image\n");
    kprintf("  logls                list logfs files\n");
    kprintf("  logcat <file>        print a logfs file\n");
    kprintf("  logcreate <file>     create an empty logfs file\n");
    kprintf("  logappend <f> <txt>  append text to a logfs file\n");
    kprintf("  logfsinfo            show logfs mount details\n");
    kprintf("\n");
    kprintf("debug:\n");
    kprintf("  panic                trigger a kernel panic\n");
    kprintf("\n");
    kprintf("aliases:\n");
    kprintf("  gpuinfo -> ginfo\n");
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

static void shell_print_block_devices(void)
{
    block_list_devices();
}

static void shell_print_fsinfo(void)
{
    block_device_t *device;
    const vfs_file_info_t *root;

    if (!vfs_is_mounted())
    {
        kprintf("vfs not mounted\n");
        return;
    }

    device = vfs_root_device();
    root = vfs_stat("/");

    kprintf("fs: %s\n", vfs_root_fs_name());
    kprintf("backing device: %s\n", device->name);
    kprintf("access: ro\n");
    kprintf("files: %u\n", vfs_root_file_count());
    kprintf("mount: /\n");
    if (root != 0)
    {
        kprintf("root type: directory\n");
    }
}

static void shell_print_rng(void)
{
    unsigned char bytes[16];
    unsigned int count;
    unsigned int i;

    if (!virtio_rng_available())
    {
        kprintf("virtio-rng unavailable\n");
        return;
    }

    if (virtio_rng_fill(bytes, sizeof(bytes), &count) != 0)
    {
        kprintf("virtio-rng request failed\n");
        return;
    }

    kprintf("rng:");
    for (i = 0; i < count; i++)
    {
        kprintf(" %x", (unsigned int)bytes[i]);
    }
    kprintf("\n");
}

static void shell_print_gpuinfo(void)
{
    const framebuffer_t *fb;

    if (!virtio_gpu_available())
    {
        kprintf("virtio-gpu unavailable\n");
        return;
    }

    fb = virtio_gpu_framebuffer();
    if (fb == 0)
    {
        kprintf("virtio-gpu framebuffer unavailable\n");
        return;
    }

    kprintf("gpu: virtio-gpu\n");
    kprintf("resolution: %u x %u\n", virtio_gpu_width(), virtio_gpu_height());
    kprintf("pitch: %u\n", fb->pitch);
    kprintf("bytes/pixel: %u\n", fb->bytes_per_pixel);
    kprintf("framebuffer: %p\n", (void *)fb->buffer);
    kprintf("format: b8g8r8a8\n");
}

static void shell_clear_graphics(void)
{
    framebuffer_t *fb;

    fb = console_graphics_framebuffer();
    if (fb == 0 || !virtio_gpu_available())
    {
        kprintf("graphics framebuffer unavailable\n");
        return;
    }

    fb_clear(fb, 0xff102030U);
    if (virtio_gpu_flush() != 0)
    {
        kprintf("gclear failed\n");
        return;
    }

    kprintf("graphics framebuffer cleared\n");
}

static void shell_run_gpu_demo(void)
{
    if (virtio_gpu_redraw_demo() != 0)
    {
        kprintf("gpudemo failed\n");
    }
}

static void shell_set_console_mode(const char *mode)
{
    if (*mode == '\0')
    {
        unsigned int output_mode;

        output_mode = console_output_mode();
        if (output_mode == CONSOLE_SINK_SERIAL)
        {
            kprintf("console: serial\n");
        }
        else if (output_mode == CONSOLE_SINK_GRAPHICS)
        {
            kprintf("console: graphics\n");
        }
        else if (output_mode == (CONSOLE_SINK_SERIAL | CONSOLE_SINK_GRAPHICS))
        {
            kprintf("console: both\n");
        }
        else
        {
            kprintf("console: none\n");
        }
        return;
    }

    if (strcmp(mode, "serial") == 0)
    {
        console_set_output_mode(CONSOLE_SINK_SERIAL);
        return;
    }

    if (strcmp(mode, "graphics") == 0)
    {
        if (console_graphics_framebuffer() == 0)
        {
            kprintf("graphics console unavailable\n");
            return;
        }

        console_set_output_mode(CONSOLE_SINK_GRAPHICS);
        return;
    }

    if (strcmp(mode, "both") == 0)
    {
        if (console_graphics_framebuffer() == 0)
        {
            console_set_output_mode(CONSOLE_SINK_SERIAL);
            kprintf("graphics console unavailable\n");
            return;
        }

        console_set_output_mode(CONSOLE_SINK_SERIAL | CONSOLE_SINK_GRAPHICS);
        return;
    }

    kprintf("usage: console [serial|graphics|both]\n");
}

static void shell_set_input_mode(const char *mode)
{
    unsigned int current_mode;

    current_mode = console_input_mode();

    if (*mode == '\0')
    {
        if (current_mode == INPUT_SOURCE_SERIAL)
        {
            kprintf("input: serial\n");
        }
        else if (current_mode == INPUT_SOURCE_KEYBOARD)
        {
            kprintf("input: keyboard\n");
        }
        else if (current_mode == (INPUT_SOURCE_SERIAL | INPUT_SOURCE_KEYBOARD))
        {
            kprintf("input: both\n");
        }
        else
        {
            kprintf("input: none\n");
        }
        return;
    }

    if (strcmp(mode, "serial") == 0)
    {
        console_set_input_mode(INPUT_SOURCE_SERIAL);
        return;
    }

    if (strcmp(mode, "keyboard") == 0)
    {
        if (!input_keyboard_available())
        {
            kprintf("keyboard input unavailable\n");
            return;
        }

        console_set_input_mode(INPUT_SOURCE_KEYBOARD);
        return;
    }

    if (strcmp(mode, "both") == 0)
    {
        if (!input_keyboard_available())
        {
            console_set_input_mode(INPUT_SOURCE_SERIAL);
            kprintf("keyboard input unavailable\n");
            return;
        }

        console_set_input_mode(INPUT_SOURCE_SERIAL | INPUT_SOURCE_KEYBOARD);
        return;
    }

    kprintf("usage: input [serial|keyboard|both]\n");
}

static void shell_run_gfx_test(void)
{
    static framebuffer_t *test_fb;
    unsigned int base_color;
    unsigned int accent_color;
    unsigned int x;
    unsigned int y;

    if (test_fb == 0)
    {
        test_fb = fb_create_test(64U, 64U);
    }

    if (test_fb == 0)
    {
        kprintf("gtest failed: framebuffer allocation failed\n");
        return;
    }

    base_color = 0x00112233U;
    accent_color = 0x00abcdefU;

    fb_clear(test_fb, base_color);
    for (y = 0; y < test_fb->height; y++)
    {
        for (x = 0; x < test_fb->width; x++)
        {
            if (x == y || x + y == (test_fb->width - 1U))
            {
                fb_put_pixel(test_fb, x, y, accent_color);
            }
        }
    }

    if (fb_get_pixel(test_fb, 0U, 0U) != accent_color ||
        fb_get_pixel(test_fb, test_fb->width / 2U, test_fb->height / 2U) != accent_color ||
        fb_get_pixel(test_fb, 1U, 0U) != base_color ||
        fb_get_pixel(test_fb, test_fb->width - 1U, 0U) != accent_color)
    {
        kprintf("gtest failed: pixel verification failed\n");
        return;
    }

    kprintf("gtest ok: %ux%u pitch=%u bpp=%u format=%u\n",
            test_fb->width,
            test_fb->height,
            test_fb->pitch,
            test_fb->bytes_per_pixel,
            (unsigned int)test_fb->pixel_format);
}

static void shell_move_cursor(const char *args)
{
    char x_text[16];
    char y_text[16];
    framebuffer_t *fb;
    unsigned long x;
    unsigned long y;

    args = shell_read_token(args, x_text, sizeof(x_text));
    args = shell_read_token(args, y_text, sizeof(y_text));
    if (x_text[0] == '\0' || y_text[0] == '\0')
    {
        kprintf("usage: cursor <x> <y>\n");
        return;
    }

    if (shell_parse_unsigned_long(x_text, &x) != 0 || shell_parse_unsigned_long(y_text, &y) != 0)
    {
        kprintf("invalid cursor coordinates\n");
        return;
    }

    fb = console_graphics_framebuffer();
    if (fb == 0 || !virtio_gpu_available())
    {
        kprintf("graphics framebuffer unavailable\n");
        return;
    }

    kprintf("cursor: %u %u\n", (unsigned int)x, (unsigned int)y);
    gfx_cursor_attach(fb);
    gfx_cursor_move((unsigned int)x, (unsigned int)y);
    (void)virtio_gpu_flush();
}

static void shell_gui_demo_status_draw(window_t *window, framebuffer_t *fb)
{
    int x;
    int y;

    x = gui_window_content_x(window);
    y = gui_window_content_y(window);
    gui_draw_panel(fb, x, y, gui_window_content_width(window), 56U, 0xffeef4f8U, 0xff7b8fa1U);
    gui_draw_label(fb, x + 8, y + 8, "duck-os compositor", 0xff102030U, 0xffeef4f8U);
    gui_draw_label(fb, x + 8, y + 22, "immediate-mode widgets", 0xff355c7dU, 0xffeef4f8U);
    gui_draw_button(fb, x + 8, y + 36, 72U, 18U, "Launch", 0);
    gui_draw_button(fb, x + 88, y + 36, 72U, 18U, "Busy", 1);
}

static void shell_gui_demo_log_draw(window_t *window, framebuffer_t *fb)
{
    int x;
    int y;

    x = gui_window_content_x(window);
    y = gui_window_content_y(window);
    gui_draw_panel(fb, x, y, gui_window_content_width(window), gui_window_content_height(window), 0xfff4efe6U, 0xff9d7f67U);
    gui_draw_label(fb, x + 8, y + 8, "[ok] framebuffer online", 0xff3b2f2fU, 0xfff4efe6U);
    gui_draw_label(fb, x + 8, y + 20, "[ok] text rendering online", 0xff3b2f2fU, 0xfff4efe6U);
    gui_draw_label(fb, x + 8, y + 32, "[ok] cursor layer online", 0xff3b2f2fU, 0xfff4efe6U);
    gui_draw_button(fb, x + 8, y + 52, 96U, 18U, "Refresh", 0);
    gui_draw_button(fb, x + 112, y + 52, 80U, 18U, "Close", 0);
}

static void shell_run_gui_demo(void)
{
    static unsigned int status_window_id;
    static unsigned int log_window_id;
    framebuffer_t *fb;
    window_t *window;

    fb = console_graphics_framebuffer();
    if (fb == 0 || !virtio_gpu_available())
    {
        kprintf("graphics framebuffer unavailable\n");
        return;
    }

    gui_attach_framebuffer(fb);

    if (status_window_id != 0U)
    {
        gui_destroy_window(status_window_id);
        status_window_id = 0U;
    }
    if (log_window_id != 0U)
    {
        gui_destroy_window(log_window_id);
        log_window_id = 0U;
    }

    window = gui_create_window(32, 24, 224, 96, "Status", shell_gui_demo_status_draw);
    if (window == 0)
    {
        kprintf("gui demo failed\n");
        return;
    }
    status_window_id = window->id;

    window = gui_create_window(120, 144, 320, 120, "System", shell_gui_demo_log_draw);
    if (window == 0)
    {
        gui_destroy_window(status_window_id);
        status_window_id = 0U;
        kprintf("gui demo failed\n");
        return;
    }
    log_window_id = window->id;

    gui_draw_all();
    console_set_output_mode(CONSOLE_SINK_SERIAL);
    kprintf("gui demo drawn on graphics display; shell output moved to serial\n");
}

static void shell_run_gui_files(const char *args)
{
    framebuffer_t *fb;
    unsigned long selected_index;

    selected_index = 0UL;
    if (*args != '\0' && shell_parse_unsigned_long(args, &selected_index) != 0)
    {
        kprintf("usage: gui files [index]\n");
        return;
    }

    fb = console_graphics_framebuffer();
    if (fb == 0 || !virtio_gpu_available())
    {
        kprintf("graphics framebuffer unavailable\n");
        return;
    }

    gui_attach_framebuffer(fb);
    if (app_file_browser_open((unsigned int)selected_index) != 0)
    {
        kprintf("gui files failed\n");
        return;
    }

    console_set_output_mode(CONSOLE_SINK_SERIAL);
    kprintf("gui files drawn on graphics display; shell output moved to serial\n");
}

static void shell_run_gui_view(const char *args)
{
    framebuffer_t *fb;

    if (*args == '\0')
    {
        kprintf("usage: gui view <path>\n");
        return;
    }

    fb = console_graphics_framebuffer();
    if (fb == 0 || !virtio_gpu_available())
    {
        kprintf("graphics framebuffer unavailable\n");
        return;
    }

    gui_attach_framebuffer(fb);
    if (app_text_viewer_open(args) != 0)
    {
        kprintf("gui view failed: %s\n", args);
        return;
    }

    console_set_output_mode(CONSOLE_SINK_SERIAL);
    kprintf("gui view drawn on graphics display; shell output moved to serial\n");
}

static void shell_run_desktop(const char *args)
{
    framebuffer_t *fb;

    fb = console_graphics_framebuffer();
    if (fb == 0 || !virtio_gpu_available())
    {
        kprintf("graphics framebuffer unavailable\n");
        return;
    }

    if (desktop_init() != 0)
    {
        kprintf("desktop init failed\n");
        return;
    }

    if (strcmp(args, "once") == 0)
    {
        if (desktop_enter() != 0)
        {
            kprintf("desktop enter failed\n");
            return;
        }

        desktop_run_once();
        desktop_exit();
        kprintf("desktop frame drawn on graphics display\n");
        return;
    }

    if (strcmp(args, "state") == 0)
    {
        desktop_debug_print_session_state();
        return;
    }

    if (*args != '\0')
    {
        kprintf("usage: desktop [once|state]\n");
        return;
    }

    if (desktop_enter() != 0)
    {
        kprintf("desktop enter failed\n");
        return;
    }

    kprintf("desktop mode active; press Escape to exit\n");
    desktop_run();
    kprintf("desktop mode exited\n");
}

static void shell_list_apps(void)
{
    const desktop_app_t *apps;
    unsigned int app_count;
    unsigned int index;

    apps = desktop_list_apps(&app_count);
    if (apps == 0 || app_count == 0U)
    {
        kprintf("no desktop apps registered\n");
        return;
    }

    for (index = 0; index < app_count; index++)
    {
        kprintf("%s  %s\n", apps[index].name, apps[index].display_name);
    }
}

static void shell_open_desktop_file(const char *path)
{
    if (path == 0 || *path == '\0')
    {
        kprintf("usage: open <path>\n");
        return;
    }

    if (desktop_open_file(path) != 0)
    {
        kprintf("open failed: %s\n", path);
        return;
    }

    kprintf("opened: %s\n", path);
}

static void shell_print_logfsinfo(void)
{
    block_device_t *device;

    if (!logfs_is_mounted())
    {
        kprintf("logfs not mounted\n");
        return;
    }

    device = logfs_device();
    kprintf("fs: logfs\n");
    kprintf("backing device: %s\n", device->name);
    kprintf("access: append-only\n");
    kprintf("files: %u\n", logfs_file_count());
}

static void shell_log_list(void)
{
    unsigned int index;

    if (!logfs_is_mounted())
    {
        kprintf("logfs not mounted\n");
        return;
    }

    for (index = 0; index < logfs_file_count(); index++)
    {
        const logfs_file_info_t *file;

        file = logfs_get_file(index);
        if (file != 0)
        {
            kprintf("%s\n", file->name);
        }
    }
}

static void shell_log_cat(const char *name)
{
    unsigned char buffer[SHELL_FILE_BUFFER_MAX];
    const logfs_file_info_t *file;
    unsigned int offset;
    unsigned char last_char;

    if (*name == '\0')
    {
        kprintf("usage: logcat <file>\n");
        return;
    }

    file = logfs_stat(name);
    if (file == 0)
    {
        kprintf("file not found: %s\n", name);
        return;
    }

    offset = 0;
    last_char = '\0';
    while (offset < file->size)
    {
        int read_size;
        unsigned int index;

        read_size = logfs_read_file_part(name, offset, buffer, sizeof(buffer));
        if (read_size <= 0)
        {
            kprintf("logcat failed: %s\n", name);
            return;
        }

        for (index = 0; index < (unsigned int)read_size; index++)
        {
            console_putc((char)buffer[index]);
        }
        last_char = buffer[read_size - 1];
        offset += (unsigned int)read_size;
    }

    if (file->size == 0U || last_char != '\n')
    {
        console_putc('\n');
    }
}

static void shell_log_create(const char *name)
{
    if (*name == '\0')
    {
        kprintf("usage: logcreate <file>\n");
        return;
    }

    if (logfs_create(name) != 0)
    {
        kprintf("logcreate failed: %s\n", name);
    }
}

static void shell_log_append(const char *args)
{
    char file_name[LOGFS_NAME_MAX];
    const char *text;

    text = shell_read_token(args, file_name, sizeof(file_name));
    text = shell_skip_spaces(text);
    if (file_name[0] == '\0' || *text == '\0')
    {
        kprintf("usage: logappend <file> <text>\n");
        return;
    }

    if (logfs_append(file_name, text, strlen(text)) != 0)
    {
        kprintf("logappend failed: %s\n", file_name);
    }
}

static void shell_print_virtio(void)
{
    virtio_print_devices();
}

static void shell_print_pci(void)
{
    pci_print_devices();
}

static void shell_block_read(const char *args)
{
    char device_name[16];
    char block_text[16];
    block_device_t *device;
    unsigned long block_number;
    unsigned char buffer[SHELL_BLOCK_DUMP_BYTES];
    unsigned int offset;

    args = shell_read_token(args, device_name, sizeof(device_name));
    args = shell_read_token(args, block_text, sizeof(block_text));
    if (device_name[0] == '\0' || block_text[0] == '\0')
    {
        kprintf("usage: blkread <device> <block>\n");
        return;
    }

    device = block_find_device(device_name);
    if (device == 0)
    {
        kprintf("device not found: %s\n", device_name);
        return;
    }

    if (shell_parse_unsigned_long(block_text, &block_number) != 0)
    {
        kprintf("invalid block: %s\n", block_text);
        return;
    }

    if (device->block_size > sizeof(buffer))
    {
        kprintf("block too large: %u\n", device->block_size);
        return;
    }

    if (device->read_blocks(device, block_number, 1, buffer) != 0)
    {
        kprintf("blkread failed: %s block %u\n", device->name, (unsigned int)block_number);
        return;
    }

    for (offset = 0; offset < device->block_size; offset += 16U)
    {
        unsigned int count;

        count = device->block_size - offset;
        if (count > 16U)
        {
            count = 16U;
        }
        shell_dump_hex_line(offset, buffer + offset, count);
    }
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

static int shell_command_matches(const char *line, const char *command)
{
    unsigned long length;

    length = strlen(command);
    if (!shell_starts_with(line, command))
    {
        return 0;
    }

    return line[length] == '\0' || line[length] == ' ';
}

static const char *shell_skip_spaces(const char *text)
{
    while (*text == ' ')
    {
        text++;
    }

    return text;
}

static const char *shell_read_token(const char *text, char *buffer, unsigned long buffer_size)
{
    unsigned long length;

    text = shell_skip_spaces(text);
    if (*text == '\0')
    {
        if (buffer_size > 0U)
        {
            buffer[0] = '\0';
        }
        return text;
    }

    length = 0;
    while (*text != '\0' && *text != ' ')
    {
        if ((length + 1U) < buffer_size)
        {
            buffer[length] = *text;
        }
        length++;
        text++;
    }

    if (buffer_size > 0U)
    {
        if (length >= buffer_size)
        {
            length = buffer_size - 1U;
        }
        buffer[length] = '\0';
    }

    return text;
}

static int shell_parse_unsigned_long(const char *text, unsigned long *value_out)
{
    unsigned long value;

    if (text == 0 || *text == '\0' || value_out == 0)
    {
        return -1;
    }

    value = 0;
    while (*text != '\0')
    {
        if (*text < '0' || *text > '9')
        {
            return -1;
        }

        value = (value * 10UL) + (unsigned long)(*text - '0');
        text++;
    }

    *value_out = value;
    return 0;
}

static void shell_print_hex_byte(unsigned char value)
{
    static const char digits[] = "0123456789abcdef";

    console_putc(digits[(value >> 4) & 0x0fU]);
    console_putc(digits[value & 0x0fU]);
}

static void shell_dump_hex_line(unsigned long offset, const unsigned char *buffer, unsigned int count)
{
    unsigned int index;
    unsigned int padding;

    kprintf("%x:", (unsigned int)offset);
    for (index = 0; index < count; index++)
    {
        console_putc(' ');
        shell_print_hex_byte(buffer[index]);
    }
    for (padding = count; padding < SHELL_HEXDUMP_BYTES_PER_LINE; padding++)
    {
        kprintf("   ");
    }

    kprintf("  |");
    for (index = 0; index < count; index++)
    {
        unsigned char ch;

        ch = buffer[index];
        if (ch >= 32U && ch <= 126U)
        {
            console_putc((char)ch);
        }
        else
        {
            console_putc('.');
        }
    }
    kprintf("|\n");
}

static void shell_print_path_error(const char *action, const char *path)
{
    if (path == 0 || *path == '\0')
    {
        kprintf("%s failed\n", action);
        return;
    }

    kprintf("%s failed: %s\n", action, path);
}

static void shell_list_files(const char *path)
{
    const char *list_path;
    const vfs_file_info_t *info;

    list_path = path;
    if (list_path == 0 || *list_path == '\0')
    {
        list_path = "/";
    }

    info = vfs_stat(list_path);
    if (info == 0)
    {
        shell_print_path_error("ls", list_path);
        return;
    }

    if ((info->flags & VFS_FILE_FLAG_DIRECTORY) == 0U)
    {
        kprintf("%s\n", info->name);
        return;
    }

    if (vfs_list_entries(list_path, shell_list_entry, 0) != 0)
    {
        shell_print_path_error("ls", list_path);
    }
}

static int shell_list_entry(const vfs_file_info_t *entry, void *context)
{
    (void)context;

    if (entry == 0)
    {
        return -1;
    }

    if ((entry->flags & VFS_FILE_FLAG_DIRECTORY) != 0U)
    {
        kprintf("%s/\n", entry->name);
    }
    else
    {
        kprintf("%s\n", entry->name);
    }

    return 0;
}

static void shell_cat_file(const char *name)
{
    file_t *file;
    unsigned char buffer[SHELL_FILE_BUFFER_MAX];
    int read_size;
    int any_read;
    unsigned char last_char;
    unsigned long i;

    if (*name == '\0')
    {
        kprintf("usage: cat <path>\n");
        return;
    }

    file = file_open(name);
    if (file == 0)
    {
        shell_print_path_error("cat", name);
        return;
    }

    any_read = 0;
    last_char = '\0';
    while (1)
    {
        read_size = file_read(file, buffer, sizeof(buffer));
        if (read_size < 0)
        {
            shell_print_path_error("cat", name);
            file_close(file);
            return;
        }

        if (read_size == 0)
        {
            break;
        }

        for (i = 0; i < (unsigned long)read_size; i++)
        {
            console_putc((char)buffer[i]);
        }

        any_read = 1;
        last_char = buffer[read_size - 1];
    }

    if (!any_read)
    {
        console_putc('\n');
    }
    else if (last_char != '\n')
    {
        console_putc('\n');
    }

    file_close(file);
}

static void shell_print_pwd(void)
{
    kprintf("/\n");
}

static void shell_print_stat(const char *path)
{
    const vfs_file_info_t *info;
    const char *type;

    if (*path == '\0')
    {
        kprintf("usage: stat <path>\n");
        return;
    }

    info = vfs_stat(path);
    if (info == 0)
    {
        shell_print_path_error("stat", path);
        return;
    }

    type = ((info->flags & VFS_FILE_FLAG_DIRECTORY) != 0U) ? "directory" : "file";
    kprintf("path: %s\n", info->name);
    kprintf("type: %s\n", type);
    kprintf("size: %u bytes\n", info->size);
    kprintf("flags: 0x%x\n", info->flags);
}

static void shell_hexdump_file(const char *path)
{
    const vfs_file_info_t *info;
    file_t *file;
    unsigned char buffer[SHELL_FILE_BUFFER_MAX];
    unsigned long offset;

    if (*path == '\0')
    {
        kprintf("usage: hexdump <path>\n");
        return;
    }

    info = vfs_stat(path);
    if (info == 0 || (info->flags & VFS_FILE_FLAG_DIRECTORY) != 0U)
    {
        shell_print_path_error("hexdump", path);
        return;
    }

    file = file_open(path);
    if (file == 0)
    {
        shell_print_path_error("hexdump", path);
        return;
    }

    offset = 0UL;
    while (offset < info->size)
    {
        int read_size;
        unsigned int line_offset;

        read_size = file_read(file, buffer, sizeof(buffer));
        if (read_size < 0)
        {
            shell_print_path_error("hexdump", path);
            file_close(file);
            return;
        }

        if (read_size == 0)
        {
            break;
        }

        for (line_offset = 0U; line_offset < (unsigned int)read_size; line_offset += SHELL_HEXDUMP_BYTES_PER_LINE)
        {
            unsigned int count;

            count = (unsigned int)read_size - line_offset;
            if (count > SHELL_HEXDUMP_BYTES_PER_LINE)
            {
                count = SHELL_HEXDUMP_BYTES_PER_LINE;
            }
            shell_dump_hex_line(offset + line_offset, buffer + line_offset, count);
        }

        offset += (unsigned long)read_size;
    }

    file_close(file);
}

static void shell_run_user_file(const char *name)
{
    if (*name == '\0')
    {
        kprintf("usage: run <file>\n");
        return;
    }

    if (user_run_file(name) != 0)
    {
        kprintf("run failed: %s\n", name);
    }
}

static void shell_run_elf_file(const char *name)
{
    if (*name == '\0')
    {
        kprintf("usage: runelf <file>\n");
        return;
    }

    if (user_run_file(name) != 0)
    {
        kprintf("runelf failed: %s\n", name);
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

    if (strcmp(line, "pwd") == 0)
    {
        shell_print_pwd();
        return;
    }

    if (shell_command_matches(line, "ls"))
    {
        shell_list_files(shell_skip_spaces(line + 2));
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

    if (strcmp(line, "blk") == 0)
    {
        shell_print_block_devices();
        return;
    }

    if (shell_command_matches(line, "blkread"))
    {
        shell_block_read(shell_skip_spaces(line + 7));
        return;
    }

    if (strcmp(line, "fsinfo") == 0)
    {
        shell_print_fsinfo();
        return;
    }

    if (strcmp(line, "ginfo") == 0 || strcmp(line, "gpuinfo") == 0)
    {
        shell_print_gpuinfo();
        return;
    }

    if (strcmp(line, "gclear") == 0)
    {
        shell_clear_graphics();
        return;
    }

    if (strcmp(line, "gpudemo") == 0)
    {
        shell_run_gpu_demo();
        return;
    }

    if (shell_command_matches(line, "cursor"))
    {
        shell_move_cursor(shell_skip_spaces(line + 6));
        return;
    }

    if (strcmp(line, "gui demo") == 0)
    {
        shell_run_gui_demo();
        return;
    }

    if (strcmp(line, "apps") == 0)
    {
        shell_list_apps();
        return;
    }

    if (shell_command_matches(line, "desktop"))
    {
        shell_run_desktop(shell_skip_spaces(line + 7));
        return;
    }

    if (shell_command_matches(line, "gui files"))
    {
        shell_run_gui_files(shell_skip_spaces(line + 9));
        return;
    }

    if (shell_command_matches(line, "gui view"))
    {
        shell_run_gui_view(shell_skip_spaces(line + 8));
        return;
    }

    if (shell_command_matches(line, "console"))
    {
        shell_set_console_mode(shell_skip_spaces(line + 7));
        return;
    }

    if (shell_command_matches(line, "input"))
    {
        shell_set_input_mode(shell_skip_spaces(line + 5));
        return;
    }

    if (strcmp(line, "gtest") == 0)
    {
        shell_run_gfx_test();
        return;
    }

    if (strcmp(line, "logls") == 0)
    {
        shell_log_list();
        return;
    }

    if (shell_command_matches(line, "logcat"))
    {
        shell_log_cat(shell_skip_spaces(line + 6));
        return;
    }

    if (shell_command_matches(line, "logcreate"))
    {
        shell_log_create(shell_skip_spaces(line + 9));
        return;
    }

    if (shell_command_matches(line, "logappend"))
    {
        shell_log_append(shell_skip_spaces(line + 9));
        return;
    }

    if (strcmp(line, "logfsinfo") == 0)
    {
        shell_print_logfsinfo();
        return;
    }

    if (strcmp(line, "virtio") == 0)
    {
        shell_print_virtio();
        return;
    }

    if (strcmp(line, "pci") == 0)
    {
        shell_print_pci();
        return;
    }

    if (strcmp(line, "rng") == 0)
    {
        shell_print_rng();
        return;
    }

    if (strcmp(line, "panic") == 0)
    {
        panic("panic command");
    }

    if (shell_command_matches(line, "stat"))
    {
        shell_print_stat(shell_skip_spaces(line + 4));
        return;
    }

    if (shell_command_matches(line, "hexdump"))
    {
        shell_hexdump_file(shell_skip_spaces(line + 7));
        return;
    }

    if (shell_command_matches(line, "open"))
    {
        shell_open_desktop_file(shell_skip_spaces(line + 4));
        return;
    }

    if (shell_command_matches(line, "cat"))
    {
        shell_cat_file(shell_skip_spaces(line + 3));
        return;
    }

    if (shell_command_matches(line, "runelf"))
    {
        shell_run_elf_file(shell_skip_spaces(line + 6));
        return;
    }

    if (shell_command_matches(line, "run"))
    {
        shell_run_user_file(shell_skip_spaces(line + 3));
        return;
    }

    kprintf("unknown command: %s\n", line);
}

void shell_execute_line(const char *command, shell_output_callback_t output_callback, void *user_data)
{
    shell_capture_state_t capture_state;

    if (command == 0)
    {
        return;
    }

    capture_state.callback = output_callback;
    capture_state.user_data = user_data;

    if (output_callback != 0)
    {
        console_set_output_capture(shell_output_capture_char, &capture_state);
    }

    shell_run_command(command);

    if (output_callback != 0)
    {
        console_set_output_capture(0, 0);
    }
}
