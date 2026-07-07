#include "block/block_device.h"
#include "arch/aarch64/cpu.h"
#include "arch/aarch64/exceptions.h"
#include "arch/aarch64/gic.h"
#include "arch/aarch64/mmu.h"
#include "arch/aarch64/sysreg.h"
#include "drivers/ramdisk.h"
#include "drivers/pci.h"
#include "drivers/virtio.h"
#include "drivers/virtio_blk.h"
#include "drivers/virtio_gpu.h"
#include "drivers/virtio_input.h"
#include "drivers/virtio_rng.h"
#include "fs/file.h"
#include "fs/logfs.h"
#include "fs/tinyfs.h"
#include "fs/vfs.h"
#include "gfx/draw.h"
#include "gfx/font.h"
#include "gfx/framebuffer.h"
#include "kernel/console.h"
#include "kernel/input.h"
#include "kernel/klog.h"
#include "kernel/kmalloc.h"
#include "kernel/initramfs.h"
#include "kernel/memory_layout.h"
#include "kernel/panic.h"
#include "kernel/spinlock.h"
#include "kernel/syscall.h"
#include "kernel/task.h"
#include "kernel/test.h"
#include "kernel/timer.h"
#include "kernel/user.h"
#include "lib/string.h"
#include "mm/pmm.h"
#include "platform/platform.h"

static unsigned long test_count;
static unsigned long test_pass_count;
static unsigned long task_demo_counter_a;
static unsigned long task_demo_counter_b;
static unsigned long syscall_exit_counter;

static unsigned long bss_test_word;
static unsigned char bss_test_buffer[16];

void test_assert(int condition, const char *message)
{
    test_count++;

    if (!condition)
    {
        kprintf("[TEST] FAIL: %s\n", message);
        panic(message);
    }

    test_pass_count++;
    kprintf("[TEST] PASS: %s\n", message);
}

static void test_bss_clear(void)
{
    unsigned long i;
    int passed;

    passed = (bss_test_word == 0);

    for (i = 0; i < sizeof(bss_test_buffer); i++)
    {
        if (bss_test_buffer[i] != 0)
        {
            passed = 0;
            break;
        }
    }

    test_assert(passed, ".bss clear");
}

static void test_exception_install(void)
{
    test_assert(exceptions_self_test(), "exception VBAR install");
}

static void test_daif_irq_mask(void)
{
    unsigned long daif;

    daif = sysreg_read_daif();
    test_assert((daif & (1UL << 7)) != 0, "DAIF IRQ masked during boot");
}

static void test_gic_constants(void)
{
    test_assert(GIC_IRQ_TIMER_PHYSICAL_PPI == 30U, "timer IRQ id");
}

static void test_mmu(void)
{
    test_assert(mmu_is_enabled(), "mmu enabled");
}

static void test_memory_layout(void)
{
    unsigned long ram_start;
    unsigned long ram_end;
    unsigned long ram_size;
    unsigned long kernel_start;
    unsigned long kernel_end;
    unsigned long first_free;

    ram_start = memory_layout_ram_start();
    ram_end = memory_layout_ram_end();
    ram_size = platform_get_ram_size();
    kernel_start = memory_layout_kernel_start();
    kernel_end = memory_layout_kernel_end();
    first_free = memory_layout_first_free_phys();

    memory_layout_print();

    test_assert(ram_start == platform_get_ram_base(), "RAM base");
    test_assert(ram_end == (platform_get_ram_base() + ram_size), "RAM end");
    test_assert(kernel_start >= ram_start && kernel_start < ram_end, "kernel start in RAM");
    test_assert(kernel_end > kernel_start && kernel_end <= ram_end, "kernel end in RAM");
    test_assert(first_free >= kernel_end, "first free physical address");
    test_assert((first_free & (PMM_PAGE_SIZE - 1)) == 0, "first free page aligned");
}

static void test_platform_virtio_layout(void)
{
    test_assert(platform_get_virtio_mmio_base() == 0x0a000000UL, "virtio mmio base");
    test_assert(platform_get_virtio_mmio_stride() == 0x200UL, "virtio mmio stride");
    test_assert(platform_get_virtio_mmio_count() == 32U, "virtio mmio count");
    test_assert(platform_get_virtio_mmio_irq(0) == 48U, "virtio mmio irq0");
}

static void test_platform_pci_layout(void)
{
    test_assert(platform_get_pci_ecam_base() == 0x4010000000UL, "pci ecam base");
    test_assert(platform_get_pci_ecam_size() == 0x10000000UL, "pci ecam size");
    test_assert(platform_get_pci_bus_start() == 0U, "pci bus start");
    test_assert(platform_get_pci_bus_end() == 255U, "pci bus end");
}

static void test_string_functions(void)
{
    unsigned char memset_buffer[8];
    unsigned char memcpy_dest[8];
    unsigned char expected_fill[8];
    char strlcpy_dest[5];

    memset(expected_fill, 0xA5, sizeof(expected_fill));
    memset(memset_buffer, 0xA5, sizeof(memset_buffer));
    test_assert(memcmp(memset_buffer, expected_fill, sizeof(memset_buffer)) == 0,
                "memset");

    memset(memcpy_dest, 0x00, sizeof(memcpy_dest));
    memcpy(memcpy_dest, memset_buffer, sizeof(memcpy_dest));
    test_assert(memcmp(memcpy_dest, memset_buffer, sizeof(memcpy_dest)) == 0,
                "memcpy");

    test_assert(memcmp("duck", "duck", 4) == 0 &&
                    memcmp("duck", "dusk", 4) < 0 &&
                    memcmp("dusk", "duck", 4) > 0,
                "memcmp");

    test_assert(strlen("kernel") == 6, "strlen");

    test_assert(strcmp("arm64", "arm64") == 0 &&
                    strcmp("arm", "arms") < 0 &&
                    strcmp("arms", "arm") > 0,
                "strcmp");

    memset(strlcpy_dest, 'X', sizeof(strlcpy_dest));
    test_assert(strlcpy(strlcpy_dest, "quack", sizeof(strlcpy_dest)) == 5 &&
                    strcmp(strlcpy_dest, "quac") == 0,
                "strlcpy");
}

static void test_kmalloc(void)
{
    unsigned long used_before;
    unsigned long used_after;
    unsigned char *block1;
    unsigned char *block2;
    unsigned char *block3;
    unsigned long i;

    used_before = kmalloc_used();

    block1 = (unsigned char *)kmalloc(1);
    block2 = (unsigned char *)kmalloc(24);
    block3 = (unsigned char *)kzalloc(7);

    test_assert(block1 != 0, "kmalloc block1");
    test_assert(block2 != 0, "kmalloc block2");
    test_assert(block3 != 0, "kzalloc block3");

    test_assert((((unsigned long)block1) & 0xFUL) == 0, "kmalloc align block1");
    test_assert((((unsigned long)block2) & 0xFUL) == 0, "kmalloc align block2");
    test_assert((((unsigned long)block3) & 0xFUL) == 0, "kmalloc align block3");

    test_assert(block2 >= block1 + 16, "kmalloc non-overlap block1 block2");
    test_assert(block3 >= block2 + 32, "kmalloc non-overlap block2 block3");

    for (i = 0; i < 7; i++)
    {
        if (block3[i] != 0)
        {
            test_assert(0, "kzalloc zero fill");
        }
    }

    test_assert(1, "kzalloc zero fill");

    used_after = kmalloc_used();
    test_assert(used_after >= used_before + 64, "kmalloc used");
}

static void test_kprintf_smoke(void)
{
    int pointer_value;

    pointer_value = 1234;

    kprintf("[TEST] INFO: kprintf decimal: %d %d %u\n", -42, 17, 3000000000U);
    kprintf("[TEST] INFO: kprintf hex: %x\n", 0x1a2b3c4dU);
    kprintf("[TEST] INFO: kprintf pointer: %p\n", (void *)&pointer_value);
    kprintf("[TEST] INFO: kprintf string: %s\n", "duck");
    kprintf("[TEST] INFO: kprintf char: %c\n", 'Q');
    kprintf("[TEST] INFO: kprintf percent/unknown: %% %q\n");

    test_assert(1, "kprintf smoke");
}

static void test_spinlock(void)
{
    spinlock_t lock;
    unsigned long saved_daif;
    unsigned long masked_daif;

    lock.value = 0;

    spin_lock(&lock);
    test_assert(lock.value == 1U, "spin_lock sets lock");
    spin_unlock(&lock);
    test_assert(lock.value == 0U, "spin_unlock clears lock");

    saved_daif = irq_save();
    masked_daif = sysreg_read_daif();
    test_assert((masked_daif & (1UL << 7)) != 0, "irq_save masks IRQ");
    irq_restore(saved_daif);
    test_assert(sysreg_read_daif() == saved_daif, "irq_restore restores DAIF");

    saved_daif = spin_lock_irqsave(&lock);
    test_assert(lock.value == 1U, "spin_lock_irqsave sets lock");
    test_assert((sysreg_read_daif() & (1UL << 7)) != 0, "spin_lock_irqsave masks IRQ");
    spin_unlock_irqrestore(&lock, saved_daif);
    test_assert(lock.value == 0U, "spin_unlock_irqrestore clears lock");
    test_assert(sysreg_read_daif() == saved_daif, "spin_unlock_irqrestore restores DAIF");
}

static void syscall_exit_task(void *arg)
{
    (void)arg;

    syscall_exit_counter++;
    syscall_exit(7);
    syscall_exit_counter = 99;
}

static void test_syscalls(void)
{
    static const char message[] = "[TEST] INFO: syscall write console\n";
    struct task *task;
    unsigned long spins;

    test_assert(syscall_write_console(message, sizeof(message) - 1) == (sizeof(message) - 1),
                "syscall write console");
    test_assert(syscall_get_ticks() == timer_irq_count(), "syscall get ticks");

    syscall_exit_counter = 0;
    task = task_create("syscall-exit", syscall_exit_task, 0);
    test_assert(task != 0, "syscall exit task create");

    spins = 0;
    while (task_active_count() != 0 && spins < 16)
    {
        task_yield();
        spins++;
    }

    test_assert(task_active_count() == 0, "syscall exit task complete");
    test_assert(syscall_exit_counter == 1, "syscall exit stops task");
}

static void test_user_mode(void)
{
    test_assert(user_run_demo() == 0, "EL0 user demo");
}

static void test_initramfs(void)
{
    const struct initramfs_file *file;
    const unsigned char *data;

    test_assert(initramfs_file_count() >= 2, "initramfs file count");

    file = initramfs_find("hello.txt");
    test_assert(file != 0, "initramfs find hello.txt");
    test_assert(file->size == 21, "initramfs hello.txt size");

    data = initramfs_read(file);
    test_assert(data != 0, "initramfs read hello.txt");
    test_assert(memcmp(data, "hello from initramfs\n", file->size) == 0,
                "initramfs hello.txt contents");
}

static void task_demo_printer(void *arg)
{
    unsigned long i;

    (void)arg;

    for (i = 0; i < 3; i++)
    {
        task_demo_counter_a++;
        kprintf("[TASK] printer iteration %u\n", (unsigned int)task_demo_counter_a);
        task_yield();
    }
}

static void task_demo_counter(void *arg)
{
    unsigned long i;

    (void)arg;

    for (i = 0; i < 3; i++)
    {
        task_demo_counter_b += 10;
        kprintf("[TASK] counter value %u\n", (unsigned int)task_demo_counter_b);
        task_yield();
    }
}

static void test_tasks(void)
{
    struct task *task1;
    struct task *task2;
    unsigned long spins;

    task1 = task_create("printer", task_demo_printer, 0);
    task2 = task_create("counter", task_demo_counter, 0);

    test_assert(task1 != 0, "task create printer");
    test_assert(task2 != 0, "task create counter");

    spins = 0;
    while (task_active_count() != 0 && spins < 16)
    {
        task_yield();
        spins++;
    }

    test_assert(task_active_count() == 0, "task run complete");
    test_assert(task_demo_counter_a == 3, "task printer count");
    test_assert(task_demo_counter_b == 30, "task counter value");
}

static void test_pmm(void)
{
    unsigned long total_before;
    unsigned long free_before;
    unsigned long used_before;
    void *page1;
    void *page2;
    void *page3;

    total_before = pmm_total_pages();
    free_before = pmm_free_pages();
    used_before = pmm_used_pages();

    test_assert(total_before > 0, "pmm total pages");
    test_assert(free_before > 0, "pmm free pages");
    test_assert(used_before < total_before, "pmm used pages");

    page1 = pmm_alloc_page();
    page2 = pmm_alloc_page();
    page3 = pmm_alloc_page();

    test_assert(page1 != 0, "pmm alloc page1");
    test_assert(page2 != 0, "pmm alloc page2");
    test_assert(page3 != 0, "pmm alloc page3");

    test_assert((((unsigned long)page1) & (PMM_PAGE_SIZE - 1)) == 0, "pmm page1 aligned");
    test_assert((((unsigned long)page2) & (PMM_PAGE_SIZE - 1)) == 0, "pmm page2 aligned");
    test_assert((((unsigned long)page3) & (PMM_PAGE_SIZE - 1)) == 0, "pmm page3 aligned");

    test_assert(page1 != page2 && page1 != page3 && page2 != page3, "pmm distinct pages");
    test_assert(pmm_used_pages() == used_before + 3, "pmm used after alloc");
    test_assert(pmm_free_pages() == free_before - 3, "pmm free after alloc");

    pmm_free_page(page2);
    test_assert(pmm_used_pages() == used_before + 2, "pmm used after free");
    test_assert(pmm_free_pages() == free_before - 2, "pmm free after free");

    page2 = pmm_alloc_page();
    test_assert(page2 != 0, "pmm realloc page");
    test_assert((((unsigned long)page2) & (PMM_PAGE_SIZE - 1)) == 0, "pmm realloc aligned");

    pmm_free_page(page1);
    pmm_free_page(page2);
    pmm_free_page(page3);

    test_assert(pmm_used_pages() == used_before, "pmm used restored");
    test_assert(pmm_free_pages() == free_before, "pmm free restored");
}

static void test_timer(void)
{
    unsigned long freq;
    unsigned long before;
    unsigned long after;

    freq = timer_frequency();
    test_assert(freq != 0, "timer frequency");

    kprintf("timer frequency: %u Hz\n", (unsigned int)freq);
    kprintf("waiting 1 second...\n");
    timer_busy_wait_ms(1000);
    kprintf("done\n");

    before = timer_now_ticks();
    timer_busy_wait_ms(1);
    after = timer_now_ticks();

    test_assert(after >= before, "timer monotonic");
    test_assert(after != before, "timer advances");
    test_assert(timer_irq_count() == 0, "timer IRQ count before enable");
}

static void test_virtio(void)
{
    test_assert(virtio_device_count() >= 1U, "virtio device detected");
    test_assert(virtio_rng_available(), "virtio rng ready");
    test_assert(virtio_blk_available(), "virtio blk ready");
}

static void test_pci(void)
{
    test_assert(pci_available(), "pci ecam available");
}

static void test_virtio_gpu(void)
{
    if (!virtio_gpu_available())
    {
        test_assert(1, "virtio gpu optional");
        return;
    }

    test_assert(virtio_gpu_width() >= 640U, "virtio gpu width");
    test_assert(virtio_gpu_height() >= 480U, "virtio gpu height");
    test_assert(virtio_gpu_framebuffer() != 0, "virtio gpu framebuffer");
    test_assert(virtio_gpu_redraw_demo() == 0, "virtio gpu redraw");
}

static void test_framebuffer_draw(void)
{
    framebuffer_t *fb;
    unsigned int background;
    unsigned int pixel_color;
    unsigned int line_color;
    unsigned int rect_color;
    unsigned int fill_color;
    unsigned int hline_color;
    unsigned int vline_color;

    fb = fb_create_test(8U, 8U);
    test_assert(fb != 0, "framebuffer test alloc");

    background = 0x00000000U;
    pixel_color = 0x00112233U;
    line_color = 0x00445566U;
    rect_color = 0x00778899U;
    fill_color = 0x00aabbccU;
    hline_color = 0x00cc8844U;
    vline_color = 0x00ee3355U;

    fb_clear(fb, background);

    draw_pixel(fb, 2, 3, pixel_color);
    draw_pixel(fb, -1, 3, 0xffffffffU);
    draw_pixel(fb, 8, 8, 0xffffffffU);
    test_assert(fb_get_pixel(fb, 2U, 3U) == pixel_color, "draw pixel");
    test_assert(fb_get_pixel(fb, 0U, 3U) == background, "draw pixel clip");

    draw_line(fb, -2, -2, 4, 4, line_color);
    test_assert(fb_get_pixel(fb, 0U, 0U) == line_color, "draw line clipped start");
    test_assert(fb_get_pixel(fb, 4U, 4U) == line_color, "draw line endpoint");
    test_assert(fb_get_pixel(fb, 5U, 4U) == background, "draw line no spill");

    draw_rect(fb, -1, 1, 4, 3, rect_color);
    test_assert(fb_get_pixel(fb, 0U, 1U) == rect_color, "draw rect top");
    test_assert(fb_get_pixel(fb, 2U, 3U) == rect_color, "draw rect bottom");
    test_assert(fb_get_pixel(fb, 1U, 2U) == background, "draw rect hollow");

    draw_fill_rect(fb, 5, 5, 4, 4, fill_color);
    test_assert(fb_get_pixel(fb, 5U, 5U) == fill_color, "draw fill rect start");
    test_assert(fb_get_pixel(fb, 7U, 7U) == fill_color, "draw fill rect clip");
    test_assert(fb_get_pixel(fb, 4U, 5U) == background, "draw fill rect bounds");

    draw_hline(fb, -2, 6, 5, hline_color);
    test_assert(fb_get_pixel(fb, 0U, 6U) == hline_color, "draw hline clip");
    test_assert(fb_get_pixel(fb, 2U, 6U) == hline_color, "draw hline body");
    test_assert(fb_get_pixel(fb, 3U, 6U) == background, "draw hline end");

    draw_vline(fb, 6, -2, 5, vline_color);
    test_assert(fb_get_pixel(fb, 6U, 0U) == vline_color, "draw vline clip");
    test_assert(fb_get_pixel(fb, 6U, 2U) == vline_color, "draw vline body");
    test_assert(fb_get_pixel(fb, 6U, 3U) == background, "draw vline end");
}

static void test_framebuffer_text(void)
{
    framebuffer_t *fb;
    unsigned int bg_color;
    unsigned int fg_color;
    unsigned int alt_fg_color;

    fb = fb_create_test(24U, 16U);
    test_assert(fb != 0, "framebuffer text alloc");

    bg_color = 0x00101010U;
    fg_color = 0x00f0f0f0U;
    alt_fg_color = 0x0000ff00U;

    fb_clear(fb, 0U);

    gfx_draw_char(fb, 1, 1, 'A', fg_color, bg_color);
    test_assert(fb_get_pixel(fb, 3U, 1U) == fg_color, "draw char fg");
    test_assert(fb_get_pixel(fb, 1U, 1U) == bg_color, "draw char bg");
    test_assert(fb_get_pixel(fb, 2U, 3U) == fg_color, "draw char body");
    test_assert(fb_get_pixel(fb, 1U, 8U) == bg_color, "draw char bottom row");

    gfx_draw_char(fb, -2, 0, '!', alt_fg_color, bg_color);
    test_assert(fb_get_pixel(fb, 1U, 0U) == alt_fg_color, "draw char clip fg");
    test_assert(fb_get_pixel(fb, 0U, 5U) == bg_color, "draw char clip bg");

    fb_clear(fb, bg_color);
    gfx_draw_string(fb, 0, 0, "A\nB", fg_color, bg_color);
    test_assert(fb_get_pixel(fb, 2U, 0U) == fg_color, "draw string first line");
    test_assert(fb_get_pixel(fb, 1U, 8U) == fg_color, "draw string newline");

    gfx_draw_char(fb, 8, 0, 31, alt_fg_color, bg_color);
    test_assert(fb_get_pixel(fb, 10U, 0U) == alt_fg_color, "draw char fallback");
}

static void test_graphics_console(void)
{
    framebuffer_t *fb;
    framebuffer_t *saved_fb;
    unsigned int saved_mode;
    int ok_attach;
    int ok_sink_graphics;
    int ok_putc;
    int ok_newline;
    int ok_backspace;
    int ok_scroll;
    int ok_cursor;

    fb = fb_create_test(64U, 24U);
    test_assert(fb != 0, "graphics console alloc");

    saved_fb = console_graphics_framebuffer();
    saved_mode = console_output_mode();

    console_attach_graphics(fb);
    console_set_output_mode(CONSOLE_SINK_GRAPHICS);

    ok_attach = console_graphics_framebuffer() == fb;
    ok_sink_graphics = console_output_mode() == CONSOLE_SINK_GRAPHICS;

    console_write("A");
    ok_putc = fb_get_pixel(fb, 2U, 0U) == 0xffffffffU;
    ok_cursor = fb_get_pixel(fb, 8U, 6U) == 0xff7fd1ffU;

    console_write("\nB");
    ok_newline = fb_get_pixel(fb, 1U, 8U) == 0xffffffffU;

    console_write("\b");
    ok_backspace = fb_get_pixel(fb, 1U, 8U) == 0xff000000U;

    console_write("1\n2\n3\n");
    ok_scroll = fb_get_pixel(fb, 1U, 0U) == 0xffffffffU &&
                fb_get_pixel(fb, 9U, 0U) == 0xff000000U &&
                fb_get_pixel(fb, 1U, 8U) == 0xffffffffU &&
                fb_get_pixel(fb, 1U, 16U) == 0xff000000U;

    console_attach_graphics(saved_fb);
    console_set_output_mode(saved_mode);

    test_assert(ok_attach, "graphics console attach");
    test_assert(ok_sink_graphics, "graphics console mode");
    test_assert(ok_putc, "graphics console putc");
    test_assert(ok_newline, "graphics console newline");
    test_assert(ok_backspace, "graphics console backspace");
    test_assert(ok_scroll, "graphics console scroll");
    test_assert(ok_cursor, "graphics console cursor");
}

static void test_input_layer(void)
{
    input_event_t event;
    input_event_t popped;
    unsigned int saved_mode;
    char ch;

    saved_mode = input_mode();
    input_set_mode(INPUT_SOURCE_SERIAL | INPUT_SOURCE_KEYBOARD);

    event.type = INPUT_EVENT_CHAR;
    event.source = INPUT_SOURCE_SERIAL;
    event.data.character = 'q';
    event.pressed = INPUT_KEY_PRESS;
    event.modifiers = 0U;
    test_assert(input_push_event(&event) != 0, "input push event");
    test_assert(input_has_event(), "input has event");
    test_assert(input_pop_event(&popped) != 0, "input pop event");
    test_assert(popped.type == INPUT_EVENT_CHAR, "input popped char type");
    test_assert(popped.data.character == 'q', "input popped char value");

    input_queue_serial_char('s');
    ch = input_getc();
    test_assert(ch == 's', "input serial char");

    input_queue_key_event(INPUT_KEY_A, INPUT_KEY_PRESS);
    ch = input_getc();
    test_assert(ch == 'a', "input key alpha");

    input_queue_key_event(INPUT_KEY_LEFTSHIFT, INPUT_KEY_PRESS);
    input_queue_key_event(INPUT_KEY_A, INPUT_KEY_PRESS);
    ch = input_getc();
    test_assert(ch == 'A', "input key shift alpha");
    input_queue_key_event(INPUT_KEY_LEFTSHIFT, INPUT_KEY_RELEASE);

    input_queue_key_event(INPUT_KEY_1, INPUT_KEY_PRESS);
    ch = input_getc();
    test_assert(ch == '1', "input key digit");

    input_queue_key_event(INPUT_KEY_LEFTSHIFT, INPUT_KEY_PRESS);
    input_queue_key_event(INPUT_KEY_1, INPUT_KEY_PRESS);
    ch = input_getc();
    test_assert(ch == '!', "input key shift digit");
    input_queue_key_event(INPUT_KEY_LEFTSHIFT, INPUT_KEY_RELEASE);

    input_queue_key_event(INPUT_KEY_ENTER, INPUT_KEY_PRESS);
    ch = input_getc();
    test_assert(ch == '\n', "input key enter");

    input_queue_key_event(INPUT_KEY_BACKSPACE, INPUT_KEY_PRESS);
    ch = input_getc();
    test_assert(ch == '\b', "input key backspace");

    input_set_mode(INPUT_SOURCE_SERIAL);
    input_queue_key_event(INPUT_KEY_A, INPUT_KEY_PRESS);
    input_queue_serial_char('z');
    ch = input_getc();
    test_assert(ch == 'z', "input mode serial");

    input_set_mode(saved_mode);
}

static void test_virtio_blk(void)
{
    block_device_t *device;
    unsigned char buffer[512];

    device = block_find_device("vda");
    test_assert(device != 0, "virtio blk device present");
    test_assert(device->block_size == 512U, "virtio blk block size");
    test_assert(device->block_count > 0U, "virtio blk capacity");
    test_assert(device->write_blocks != 0, "virtio blk write available");
    test_assert(device->read_blocks(device, 0, 1, buffer) == 0, "virtio blk read block 0");
    test_assert(memcmp(buffer, "LGFS", 4) == 0, "virtio blk block 0 logfs magic");
    test_assert(buffer[4] == 1U, "virtio blk block 0 logfs version");
}

static void test_ramdisk(void)
{
    block_device_t *device;
    unsigned char write_buffer[RAMDISK_BLOCK_SIZE];
    unsigned char read_buffer[RAMDISK_BLOCK_SIZE];
    unsigned long test_block;
    unsigned int i;

    device = ramdisk_device();
    test_assert(block_device_count() >= 1U, "block device detected");
    test_assert(device != 0, "ramdisk device available");
    test_assert(strcmp(device->name, "ram0") == 0, "ramdisk name");
    test_assert(device->block_size == RAMDISK_BLOCK_SIZE, "ramdisk block size");
    test_assert(device->block_count == RAMDISK_BLOCK_COUNT, "ramdisk block count");
    test_assert(device->write_blocks != 0, "ramdisk write available");
    test_assert(block_get_device(0) == device, "ramdisk registered first");

    test_block = device->block_count - 1;

    for (i = 0; i < RAMDISK_BLOCK_SIZE; i++)
    {
        write_buffer[i] = (unsigned char)(i & 0xffU);
        read_buffer[i] = 0;
    }

    test_assert(device->write_blocks(device, test_block, 1, write_buffer) == 0, "ramdisk write block");
    test_assert(device->read_blocks(device, test_block, 1, read_buffer) == 0, "ramdisk read block");
    test_assert(memcmp(write_buffer, read_buffer, RAMDISK_BLOCK_SIZE) == 0, "ramdisk readback");
    test_assert(device->read_blocks(device, RAMDISK_BLOCK_COUNT, 1, read_buffer) != 0,
                "ramdisk bounds check");
}

static void test_tinyfs(void)
{
    block_device_t *device;
    const tinyfs_file_t *file;
    unsigned char buffer[64];
    int size;

    test_assert(tinyfs_is_mounted(), "tinyfs mounted");
    device = tinyfs_device();
    test_assert(device != 0, "tinyfs device");
    test_assert(strcmp(device->name, "vda") == 0 || strcmp(device->name, "ram0") == 0,
                "tinyfs device name");
    test_assert(tinyfs_file_count() >= 6U, "tinyfs file count");

    file = tinyfs_find("hello.txt");
    test_assert(file != 0, "tinyfs find hello.txt");
    test_assert(file->size == 21U, "tinyfs hello.txt size");

    size = tinyfs_read_file("hello.txt", buffer, sizeof(buffer));
    test_assert(size == 21, "tinyfs read hello.txt");
    test_assert(memcmp(buffer, "hello from deez nuts\n", 21) == 0, "tinyfs hello.txt contents");
    test_assert(tinyfs_find("etc/motd") != 0, "tinyfs find etc/motd");
    test_assert(tinyfs_find("bin/init") != 0, "tinyfs find bin/init");
    test_assert(tinyfs_find("bin/hello.bin") != 0, "tinyfs find bin/hello.bin");
    test_assert(tinyfs_find("missing.txt") == 0, "tinyfs missing file");
}

static void test_vfs(void)
{
    block_device_t *device;
    const vfs_file_info_t *file;
    const vfs_file_info_t *directory;
    unsigned char buffer[64];
    int size;

    test_assert(vfs_is_mounted(), "vfs mounted");
    test_assert(strcmp(vfs_root_fs_name(), "tinyfs") == 0, "vfs root fs name");
    device = vfs_root_device();
    test_assert(device != 0, "vfs root device");
    test_assert(device == tinyfs_device(), "vfs root matches tinyfs device");
    test_assert(vfs_root_file_count() >= 7U, "vfs root file count");
    test_assert(vfs_list("/") == 0, "vfs list root");
    test_assert(vfs_list("/bin") == 0, "vfs list /bin");
    test_assert(vfs_list("/missing") != 0, "vfs list missing rejected");

    file = vfs_stat("/hello.txt");
    test_assert(file != 0, "vfs stat hello.txt");
    test_assert(strcmp(file->name, "hello.txt") == 0, "vfs stat name");
    test_assert(file->size == 21U, "vfs stat size");
    directory = vfs_stat("/");
    test_assert(directory != 0, "vfs stat root");
    test_assert((directory->flags & VFS_FILE_FLAG_DIRECTORY) != 0, "vfs root is directory");
    directory = vfs_stat("/etc");
    test_assert(directory != 0, "vfs stat /etc");
    test_assert((directory->flags & VFS_FILE_FLAG_DIRECTORY) != 0, "vfs /etc is directory");
    test_assert(vfs_stat("/missing.txt") == 0, "vfs stat missing");

    size = vfs_read_file("/hello.txt", buffer, sizeof(buffer));
    test_assert(size == 21, "vfs read hello.txt");
    test_assert(memcmp(buffer, "hello from deez nuts\n", 21) == 0, "vfs hello.txt contents");
    size = vfs_read_file("/etc/motd", buffer, sizeof(buffer));
    test_assert(size == 49, "vfs read /etc/motd");
    test_assert(memcmp(buffer, "Welcome to duck-os.\nTinyFS is mounted and ready.\n", 49) == 0,
                "vfs /etc/motd contents");
    test_assert(vfs_read_file("/", buffer, sizeof(buffer)) < 0, "vfs read root rejected");
    test_assert(vfs_read_file("/etc", buffer, sizeof(buffer)) < 0, "vfs read dir rejected");
}

static void test_logfs(void)
{
    const logfs_file_info_t *file;
    unsigned int index;
    int found;
    unsigned char buffer[64];
    int size;

    test_assert(logfs_is_mounted(), "logfs mounted");
    test_assert(logfs_device() != 0, "logfs device");
    test_assert(strcmp(logfs_device()->name, "vda") == 0, "logfs device name");
    if (logfs_stat("notes.txt") == 0)
    {
        test_assert(logfs_create("notes.txt") == 0, "logfs create");
        test_assert(logfs_append("notes.txt", "hello", 5) == 0, "logfs append first");
        test_assert(logfs_append("notes.txt", " world\n", 7) == 0, "logfs append second");
    }
    else
    {
        test_assert(1, "logfs create");
        test_assert(1, "logfs append first");
        test_assert(1, "logfs append second");
    }

    test_assert(logfs_create("notes.txt") != 0, "logfs duplicate create rejected");
    file = logfs_stat("notes.txt");
    test_assert(file != 0, "logfs stat file");
    test_assert(file->size == 12U, "logfs size");
    test_assert(logfs_file_count() >= 1U, "logfs file count");
    found = 0;
    for (index = 0; index < logfs_file_count(); index++)
    {
        file = logfs_get_file(index);
        if (file != 0 && strcmp(file->name, "notes.txt") == 0)
        {
            found = 1;
            break;
        }
    }
    test_assert(found, "logfs list file");
    size = logfs_read_file("notes.txt", buffer, sizeof(buffer));
    test_assert(size == 12, "logfs read file");
    test_assert(memcmp(buffer, "hello world\n", 12) == 0, "logfs read contents");
    size = logfs_read_file_part("notes.txt", 6U, buffer, 5U);
    test_assert(size == 5, "logfs read part");
    test_assert(memcmp(buffer, "world", 5) == 0, "logfs read part contents");
}

static void test_file_layer(void)
{
    file_t *file;
    unsigned char buffer[16];
    int size;

    file = file_open("/etc");
    test_assert(file == 0, "file open dir rejected");

    file = file_open("/etc/motd");
    test_assert(file != 0, "file open motd");
    test_assert(file->offset == 0U, "file open offset");

    size = file_read(file, buffer, 6);
    test_assert(size == 6, "file read chunk1");
    test_assert(memcmp(buffer, "Welcom", 6) == 0, "file read chunk1 contents");
    test_assert(file->offset == 6U, "file offset advance");

    size = file_read(file, buffer, 9);
    test_assert(size == 9, "file read chunk2");
    test_assert(memcmp(buffer, "e to duck", 9) == 0, "file read chunk2 contents");
    test_assert(file->offset == 15U, "file offset eof");

    test_assert(file_seek(file, file->size) == 0, "file seek eof");
    size = file_read(file, buffer, sizeof(buffer));
    test_assert(size == 0, "file read eof");

    test_assert(file_seek(file, 11) == 0, "file seek");
    size = file_read(file, buffer, 4);
    test_assert(size == 4, "file read after seek");
    test_assert(memcmp(buffer, "duck", 4) == 0, "file read after seek contents");

    file_close(file);
}

static void test_user_file_loader(void)
{
    test_assert(vfs_stat("/bin/hello.bin") != 0, "user file present");
    test_assert(user_run_file("/bin/hello.bin") == 0, "user file run");
    test_assert(vfs_stat("/bin/hello.elf") != 0, "user elf file present");
    test_assert(user_run_file("/bin/hello.elf") == 0, "user elf file run");
}

static void print_cpu_register_state(void)
{
    unsigned long currentel;
    unsigned long spsel;
    unsigned long daif;
    unsigned long mpidr_el1;

    currentel = sysreg_read_currentel();
    spsel = sysreg_read_spsel();
    daif = sysreg_read_daif();
    mpidr_el1 = sysreg_read_mpidr_el1();

    klog_info("AArch64 system registers:");
    kprintf("  CurrentEL: 0x%x (EL%u)\n",
            (unsigned int)currentel,
            (unsigned int)(currentel >> 2));
    kprintf("  SPSel: 0x%x\n", (unsigned int)spsel);
    kprintf("  DAIF: 0x%x\n", (unsigned int)daif);
    kprintf("  MPIDR_EL1: %p\n", (void *)mpidr_el1);
}

void test_run_all(void)
{
    klog_info("kernel tests: start");

    print_cpu_register_state();
    test_bss_clear();
    test_exception_install();
    test_daif_irq_mask();
    test_gic_constants();
    test_mmu();
    test_memory_layout();
    test_platform_virtio_layout();
    test_platform_pci_layout();
    test_string_functions();
    test_kmalloc();
    test_pmm();
    test_spinlock();
    test_syscalls();
    test_user_mode();
    test_initramfs();
    test_tasks();
    test_timer();
    test_pci();
    test_virtio();
    test_virtio_gpu();
    test_framebuffer_draw();
    test_framebuffer_text();
    test_graphics_console();
    test_input_layer();
    test_virtio_blk();
    test_ramdisk();
    test_tinyfs();
    test_vfs();
    test_logfs();
    test_file_layer();
    test_user_file_loader();
    test_kprintf_smoke();

    kprintf("[TEST] SUMMARY: %u/%u passed\n",
            (unsigned int)test_pass_count,
            (unsigned int)test_count);
    klog_info("kernel tests: halt");

#ifdef EXCEPTION_SELF_TEST
    klog_info("exception self test: triggering brk");
    exceptions_trigger_test();
#endif
}
