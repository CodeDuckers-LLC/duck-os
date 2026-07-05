#include "arch/aarch64/mmu.h"
#include "arch/aarch64/sysreg.h"
#include "kernel/klog.h"
#include "kernel/panic.h"
#include "kernel/user.h"
#include "lib/string.h"
#include "mm/pmm.h"

extern unsigned char _binary_build_user_hello_bin_start[];
extern unsigned char _binary_build_user_hello_bin_end[];

extern unsigned long arch_enter_user(unsigned long entry, unsigned long user_sp);
extern char arch_user_resume_el1[];

static int user_active;
static unsigned long user_resume_spsr;

static unsigned long user_blob_size(void)
{
    return (unsigned long)(_binary_build_user_hello_bin_end -
                           _binary_build_user_hello_bin_start);
}

static void user_prepare_pages(unsigned long code_page, unsigned long stack_page)
{
    unsigned long program_size;

    memset((void *)code_page, 0, PMM_PAGE_SIZE);
    memset((void *)stack_page, 0, PMM_PAGE_SIZE);

    program_size = user_blob_size();
    if (program_size > PMM_PAGE_SIZE)
    {
        panic("user program too large");
    }

    memcpy((void *)code_page, _binary_build_user_hello_bin_start, program_size);
    mmu_sync_for_exec((void *)code_page, program_size);
}

int user_exception_active(void)
{
    return user_active;
}

struct exception_trap_frame *user_exit_from_exception(struct exception_trap_frame *frame,
                                                      unsigned long status)
{
    if (!user_active)
    {
        frame->x[0] = ~0UL;
        return frame;
    }

    user_active = 0;
    frame->x[0] = status;
    frame->elr_el1 = (unsigned long)arch_user_resume_el1;
    frame->spsr_el1 = user_resume_spsr;
    return frame;
}

int user_run_demo(void)
{
    unsigned long code_page;
    unsigned long stack_page;
    unsigned long result;

    code_page = (unsigned long)pmm_alloc_page();
    stack_page = (unsigned long)pmm_alloc_page();

    if (code_page == 0 || stack_page == 0)
    {
        panic("user page allocation failed");
    }

    user_prepare_pages(code_page, stack_page);

    mmu_map_user_code_page(USER_CODE_VA, code_page);
    mmu_map_user_data_page(USER_STACK_TOP - PMM_PAGE_SIZE, stack_page);

    user_resume_spsr = (sysreg_read_daif() & 0x3c0UL) | 0x5UL;

    klog_info("user: entering EL0");
    user_active = 1;
    result = arch_enter_user(USER_CODE_VA, USER_STACK_TOP);
    user_active = 0;
    kprintf("[INFO] user: exited EL0 status=%u\n", (unsigned int)result);

    return (int)result;
}
