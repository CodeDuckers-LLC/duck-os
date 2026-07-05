#include "arch/aarch64/cpu.h"
#include "arch/aarch64/exceptions.h"
#include "arch/aarch64/gic.h"
#include "arch/aarch64/sysreg.h"
#include "kernel/klog.h"
#include "kernel/kmalloc.h"
#include "kernel/memory_layout.h"
#include "kernel/panic.h"
#include "kernel/test.h"
#include "kernel/timer.h"
#include "lib/string.h"
#include "mm/pmm.h"
#include "platform/platform.h"

static unsigned long test_count;
static unsigned long test_pass_count;

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
    test_memory_layout();
    test_string_functions();
    test_kmalloc();
    test_pmm();
    test_timer();
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
