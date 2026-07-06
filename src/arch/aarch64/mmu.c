#include "arch/aarch64/gic.h"
#include "arch/aarch64/mmu.h"
#include "arch/aarch64/sysreg.h"
#include "kernel/klog.h"
#include "lib/string.h"
#include "platform/platform.h"

#define MMU_TABLE_ENTRIES 512UL
#define MMU_L1_BLOCK_SIZE 0x40000000UL
#define MMU_L2_BLOCK_SIZE 0x00200000UL
#define MMU_L3_PAGE_SIZE 0x00001000UL

#define MMU_DESC_VALID (1UL << 0)
#define MMU_DESC_TABLE (1UL << 1)
#define MMU_DESC_BLOCK MMU_DESC_VALID
#define MMU_DESC_PAGE (MMU_DESC_VALID | MMU_DESC_TABLE)
#define MMU_DESC_AF (1UL << 10)
#define MMU_DESC_SH_INNER (3UL << 8)
#define MMU_DESC_ATTR_DEVICE (0UL << 2)
#define MMU_DESC_ATTR_NORMAL (1UL << 2)
#define MMU_DESC_AP_USER_RW (1UL << 6)
#define MMU_DESC_AP_USER_RO (3UL << 6)
#define MMU_DESC_PXN (1UL << 53)
#define MMU_DESC_UXN (1UL << 54)

#define MAIR_ATTR_DEVICE_nGnRnE 0x00UL
#define MAIR_ATTR_NORMAL_WBWA 0xffUL

#define TCR_T0SZ_39BIT 25UL
#define TCR_IRGN0_WBWA (1UL << 8)
#define TCR_ORGN0_WBWA (1UL << 10)
#define TCR_SH0_INNER (3UL << 12)
#define TCR_TG0_4K (0UL << 14)
#define TCR_EPD1_DISABLE (1UL << 23)
#define TCR_IPS_1TB (2UL << 32)

#define SCTLR_M (1UL << 0)
#define SCTLR_C (1UL << 2)
#define SCTLR_I (1UL << 12)

static unsigned long mmu_l1_table[MMU_TABLE_ENTRIES] __attribute__((aligned(4096)));
static unsigned long mmu_l2_low_table[MMU_TABLE_ENTRIES] __attribute__((aligned(4096)));
static unsigned long mmu_l2_ram_table[MMU_TABLE_ENTRIES] __attribute__((aligned(4096)));
static unsigned long mmu_l3_user_table[MMU_TABLE_ENTRIES] __attribute__((aligned(4096)));
static int mmu_initialized;

static void mmu_dsb_ish(void)
{
    asm volatile("dsb ish");
}

static void mmu_dsb_ishst(void)
{
    asm volatile("dsb ishst");
}

static void mmu_isb(void)
{
    asm volatile("isb");
}

static void mmu_tlbi_vmalle1(void)
{
    asm volatile("tlbi vmalle1");
}

static unsigned long mmu_l1_index(unsigned long address)
{
    return (address / MMU_L1_BLOCK_SIZE) & 0x1ffUL;
}

static unsigned long mmu_l2_index(unsigned long address)
{
    return (address / MMU_L2_BLOCK_SIZE) & 0x1ffUL;
}

static unsigned long mmu_l3_index(unsigned long address)
{
    return (address / MMU_L3_PAGE_SIZE) & 0x1ffUL;
}

static void mmu_map_l1_table(unsigned long virtual_address, unsigned long *table)
{
    mmu_l1_table[mmu_l1_index(virtual_address)] =
        ((unsigned long)table & ~0xfffUL) | MMU_DESC_VALID | MMU_DESC_TABLE;
}

static void mmu_map_l2_block(unsigned long *table,
                             unsigned long address,
                             unsigned long attributes)
{
    table[mmu_l2_index(address)] =
        (address & ~(MMU_L2_BLOCK_SIZE - 1)) | attributes | MMU_DESC_BLOCK;
}

static void mmu_map_l2_table(unsigned long *table,
                             unsigned long virtual_address,
                             unsigned long *next_table)
{
    table[mmu_l2_index(virtual_address)] =
        ((unsigned long)next_table & ~0xfffUL) | MMU_DESC_VALID | MMU_DESC_TABLE;
}

static void mmu_map_l3_page(unsigned long *table,
                            unsigned long virtual_address,
                            unsigned long physical_address,
                            unsigned long attributes)
{
    table[mmu_l3_index(virtual_address)] =
        (physical_address & ~(MMU_L3_PAGE_SIZE - 1)) | attributes | MMU_DESC_PAGE;
}

static void mmu_tlbi_vaae1(unsigned long address)
{
    asm volatile("tlbi vaae1is, %0" : : "r"(address >> 12));
}

static void mmu_ic_ivau(unsigned long address)
{
    asm volatile("ic ivau, %0" : : "r"(address));
}

static void mmu_dc_cvau(unsigned long address)
{
    asm volatile("dc cvau, %0" : : "r"(address));
}

static void mmu_dc_civac(unsigned long address)
{
    asm volatile("dc civac, %0" : : "r"(address));
}

static void mmu_dc_ivac(unsigned long address)
{
    asm volatile("dc ivac, %0" : : "r"(address));
}

static void mmu_build_tables(void)
{
    unsigned long address;
    unsigned long ram_start;
    unsigned long ram_end;

    memset(mmu_l1_table, 0, sizeof(mmu_l1_table));
    memset(mmu_l2_low_table, 0, sizeof(mmu_l2_low_table));
    memset(mmu_l2_ram_table, 0, sizeof(mmu_l2_ram_table));
    memset(mmu_l3_user_table, 0, sizeof(mmu_l3_user_table));

    mmu_map_l1_table(0x00000000UL, mmu_l2_low_table);
    mmu_map_l1_table(platform_get_ram_base(), mmu_l2_ram_table);
    mmu_map_l2_table(mmu_l2_low_table, 0x00000000UL, mmu_l3_user_table);

    mmu_map_l2_block(mmu_l2_low_table,
                     GICD_BASE,
                     MMU_DESC_AF | MMU_DESC_ATTR_DEVICE | MMU_DESC_PXN | MMU_DESC_UXN);
    mmu_map_l2_block(mmu_l2_low_table,
                     platform_get_uart0_base(),
                     MMU_DESC_AF | MMU_DESC_ATTR_DEVICE | MMU_DESC_PXN | MMU_DESC_UXN);
    mmu_map_l2_block(mmu_l2_low_table,
                     platform_get_virtio_mmio_base(),
                     MMU_DESC_AF | MMU_DESC_ATTR_DEVICE | MMU_DESC_PXN | MMU_DESC_UXN);

    ram_start = platform_get_ram_base();
    ram_end = ram_start + platform_get_ram_size();

    for (address = ram_start; address < ram_end; address += MMU_L2_BLOCK_SIZE)
    {
        mmu_map_l2_block(mmu_l2_ram_table,
                         address,
                         MMU_DESC_AF | MMU_DESC_SH_INNER | MMU_DESC_ATTR_NORMAL);
    }
}

static void mmu_map_user_page(unsigned long virtual_address,
                              unsigned long physical_address,
                              unsigned long attributes)
{
    mmu_map_l3_page(mmu_l3_user_table, virtual_address, physical_address, attributes);
    mmu_dsb_ishst();
    mmu_tlbi_vaae1(virtual_address);
    mmu_dsb_ish();
    mmu_isb();
}

int mmu_is_enabled(void)
{
    return (sysreg_read_sctlr_el1() & SCTLR_M) != 0;
}

void mmu_init(void)
{
    unsigned long mair;
    unsigned long tcr;
    unsigned long sctlr;

    if (mmu_initialized || mmu_is_enabled())
    {
        mmu_initialized = 1;
        return;
    }

    klog_info("mmu: enabling");

    mmu_build_tables();
    mmu_dsb_ishst();

    mair = (MAIR_ATTR_DEVICE_nGnRnE << 0) | (MAIR_ATTR_NORMAL_WBWA << 8);
    tcr = TCR_T0SZ_39BIT |
          TCR_IRGN0_WBWA |
          TCR_ORGN0_WBWA |
          TCR_SH0_INNER |
          TCR_TG0_4K |
          TCR_EPD1_DISABLE |
          TCR_IPS_1TB;

    sysreg_write_mair_el1(mair);
    sysreg_write_tcr_el1(tcr);
    sysreg_write_ttbr0_el1((unsigned long)mmu_l1_table);

    mmu_isb();
    mmu_tlbi_vmalle1();
    mmu_dsb_ish();
    mmu_isb();

    sctlr = sysreg_read_sctlr_el1();
    sctlr |= SCTLR_M | SCTLR_C | SCTLR_I;
    sysreg_write_sctlr_el1(sctlr);
    mmu_isb();

    mmu_initialized = 1;
    klog_info("mmu: enabled");
}

void mmu_map_user_code_page(unsigned long virtual_address, unsigned long physical_address)
{
    mmu_map_user_page(virtual_address,
                      physical_address,
                      MMU_DESC_AF |
                          MMU_DESC_SH_INNER |
                          MMU_DESC_ATTR_NORMAL |
                          MMU_DESC_AP_USER_RO |
                          MMU_DESC_PXN);
}

void mmu_map_user_data_page(unsigned long virtual_address, unsigned long physical_address)
{
    mmu_map_user_page(virtual_address,
                      physical_address,
                      MMU_DESC_AF |
                          MMU_DESC_SH_INNER |
                          MMU_DESC_ATTR_NORMAL |
                          MMU_DESC_AP_USER_RW |
                          MMU_DESC_PXN |
                          MMU_DESC_UXN);
}

void mmu_sync_for_exec(void *address, unsigned long size)
{
    unsigned long current;
    unsigned long end;

    current = (unsigned long)address & ~63UL;
    end = ((unsigned long)address + size + 63UL) & ~63UL;

    for (; current < end; current += 64UL)
    {
        mmu_dc_cvau(current);
    }

    mmu_dsb_ish();

    current = (unsigned long)address & ~63UL;
    for (; current < end; current += 64UL)
    {
        mmu_ic_ivau(current);
    }

    mmu_dsb_ish();
    mmu_isb();
}

void mmu_sync_for_device(void *address, unsigned long size)
{
    unsigned long current;
    unsigned long end;

    current = (unsigned long)address & ~63UL;
    end = ((unsigned long)address + size + 63UL) & ~63UL;

    for (; current < end; current += 64UL)
    {
        mmu_dc_civac(current);
    }

    mmu_dsb_ish();
}

void mmu_sync_for_cpu(void *address, unsigned long size)
{
    unsigned long current;
    unsigned long end;

    current = (unsigned long)address & ~63UL;
    end = ((unsigned long)address + size + 63UL) & ~63UL;

    for (; current < end; current += 64UL)
    {
        mmu_dc_ivac(current);
    }

    mmu_dsb_ish();
    mmu_isb();
}
