#include "arch/aarch64/mmu.h"
#include "arch/aarch64/sysreg.h"
#include "fs/file.h"
#include "kernel/klog.h"
#include "kernel/panic.h"
#include "kernel/user.h"
#include "lib/string.h"
#include "mm/pmm.h"

extern unsigned char _binary_build_user_hello_bin_start[];
extern unsigned char _binary_build_user_hello_bin_end[];

extern unsigned long arch_enter_user(unsigned long entry, unsigned long user_sp);
extern char arch_user_resume_el1[];

#define USER_LOAD_CHUNK_SIZE 256U
#define USER_FLAT_BINARY_MAX_PAGES 16U
#define USER_MAX_PROGRAM_PAGES ((USER_STACK_TOP - USER_CODE_VA - PMM_PAGE_SIZE) / PMM_PAGE_SIZE)

#define ELF_MAGIC0 0x7fU
#define ELF_MAGIC1 'E'
#define ELF_MAGIC2 'L'
#define ELF_MAGIC3 'F'

#define ELF_EI_CLASS 4U
#define ELF_EI_DATA 5U
#define ELF_EI_VERSION 6U

#define ELF_CLASS_64 2U
#define ELF_DATA_LITTLE_ENDIAN 1U
#define ELF_VERSION_CURRENT 1U

#define ELF_TYPE_EXECUTABLE 2U
#define ELF_MACHINE_AARCH64 183U

#define ELF_PROGRAM_TYPE_LOAD 1U
#define ELF_PROGRAM_FLAG_WRITE 0x2U

typedef struct __attribute__((packed)) elf64_header
{
    unsigned char ident[16];
    unsigned short type;
    unsigned short machine;
    unsigned int version;
    unsigned long entry;
    unsigned long phoff;
    unsigned long shoff;
    unsigned int flags;
    unsigned short ehsize;
    unsigned short phentsize;
    unsigned short phnum;
    unsigned short shentsize;
    unsigned short shnum;
    unsigned short shstrndx;
} elf64_header_t;

typedef struct __attribute__((packed)) elf64_program_header
{
    unsigned int type;
    unsigned int flags;
    unsigned long offset;
    unsigned long vaddr;
    unsigned long paddr;
    unsigned long filesz;
    unsigned long memsz;
    unsigned long align;
} elf64_program_header_t;

static int user_active;
static unsigned long user_resume_spsr;
static unsigned long user_program_pages[USER_MAX_PROGRAM_PAGES];
static unsigned char user_program_page_writable[USER_MAX_PROGRAM_PAGES];

static unsigned long user_blob_size(void)
{
    return (unsigned long)(_binary_build_user_hello_bin_end -
                           _binary_build_user_hello_bin_start);
}

static void user_prepare_pages(unsigned long *code_pages,
                               unsigned int code_page_count,
                               unsigned long stack_page)
{
    unsigned long program_size;
    unsigned int page_index;

    for (page_index = 0; page_index < code_page_count; page_index++)
    {
        memset((void *)code_pages[page_index], 0, PMM_PAGE_SIZE);
    }
    memset((void *)stack_page, 0, PMM_PAGE_SIZE);

    program_size = user_blob_size();
    if (program_size > PMM_PAGE_SIZE * code_page_count)
    {
        panic("user program too large");
    }

    memcpy((void *)code_pages[0], _binary_build_user_hello_bin_start, program_size);
}

static unsigned int user_code_page_count(unsigned long program_size)
{
    return (unsigned int)((program_size + PMM_PAGE_SIZE - 1UL) / PMM_PAGE_SIZE);
}

static int user_program_range_valid(unsigned long image_base, unsigned int page_count)
{
    if (page_count == 0 || page_count > USER_MAX_PROGRAM_PAGES)
    {
        return 0;
    }

    if (image_base < USER_CODE_VA)
    {
        return 0;
    }

    if (image_base + ((unsigned long)page_count * PMM_PAGE_SIZE) > (USER_STACK_TOP - PMM_PAGE_SIZE))
    {
        return 0;
    }

    return 1;
}

static int user_alloc_program_pages(unsigned long *program_pages, unsigned int page_count)
{
    unsigned int index;

    for (index = 0; index < page_count; index++)
    {
        program_pages[index] = (unsigned long)pmm_alloc_page();
        if (program_pages[index] == 0)
        {
            return -1;
        }

        memset((void *)program_pages[index], 0, PMM_PAGE_SIZE);
    }

    return 0;
}

static int user_run_loaded_pages(const char *label,
                                 unsigned long image_base,
                                 unsigned long entry_point,
                                 unsigned long *program_pages,
                                 const unsigned char *page_writable,
                                 unsigned int page_count,
                                 unsigned long stack_page,
                                 unsigned long program_size)
{
    unsigned int page_index;
    unsigned long result;

    for (page_index = 0; page_index < page_count; page_index++)
    {
        unsigned long virtual_address;

        virtual_address = image_base + ((unsigned long)page_index * PMM_PAGE_SIZE);
        if (page_writable != 0 && page_writable[page_index] != 0U)
        {
            mmu_map_user_data_page(virtual_address, program_pages[page_index]);
        }
        else
        {
            mmu_map_user_code_page(virtual_address, program_pages[page_index]);
        }
    }
    mmu_map_user_data_page(USER_STACK_TOP - PMM_PAGE_SIZE, stack_page);

    for (page_index = 0; page_index < page_count; page_index++)
    {
        mmu_sync_for_exec((void *)program_pages[page_index], PMM_PAGE_SIZE);
    }

    user_resume_spsr = (sysreg_read_daif() & 0x3c0UL) | 0x5UL;

    kprintf("[INFO] user: loading %s base=%p entry=%p size=%u pages=%u\n",
            label,
            (void *)image_base,
            (void *)entry_point,
            (unsigned int)program_size,
            page_count);
    klog_info("user: entering EL0");
    user_active = 1;
    result = arch_enter_user(entry_point, USER_STACK_TOP);
    user_active = 0;
    kprintf("[INFO] user: exited EL0 status=%u\n", (unsigned int)result);

    return (int)result;
}

static int user_load_file_into_pages(file_t *file,
                                     unsigned long *code_pages,
                                     unsigned int code_page_count)
{
    unsigned char buffer[USER_LOAD_CHUNK_SIZE];
    unsigned int total_read;

    total_read = 0;
    while (1)
    {
        int chunk_size;
        unsigned int remaining;
        unsigned int copy_offset;
        unsigned int page_index;
        unsigned int page_offset;

        chunk_size = file_read(file, buffer, sizeof(buffer));
        if (chunk_size < 0)
        {
            return -1;
        }

        if (chunk_size == 0)
        {
            break;
        }

        remaining = (unsigned int)chunk_size;
        copy_offset = 0;
        while (remaining > 0)
        {
            unsigned int copy_size;

            page_index = total_read / PMM_PAGE_SIZE;
            page_offset = total_read % PMM_PAGE_SIZE;
            if (page_index >= code_page_count)
            {
                return -1;
            }

            copy_size = (unsigned int)(PMM_PAGE_SIZE - page_offset);
            if (copy_size > remaining)
            {
                copy_size = remaining;
            }

            memcpy((void *)(code_pages[page_index] + page_offset), buffer + copy_offset, copy_size);
            total_read += copy_size;
            copy_offset += copy_size;
            remaining -= copy_size;
        }
    }

    return 0;
}

static int user_read_exact(file_t *file, void *buffer, unsigned int size)
{
    unsigned char *cursor;
    unsigned int remaining;

    cursor = (unsigned char *)buffer;
    remaining = size;
    while (remaining > 0U)
    {
        int read_size;

        read_size = file_read(file, cursor, remaining);
        if (read_size <= 0)
        {
            return -1;
        }

        cursor += read_size;
        remaining -= (unsigned int)read_size;
    }

    return 0;
}

static int user_read_exact_at(file_t *file, unsigned int offset, void *buffer, unsigned int size)
{
    if (file_seek(file, offset) != 0)
    {
        return -1;
    }

    return user_read_exact(file, buffer, size);
}

static int user_copy_file_range_into_pages(file_t *file,
                                           unsigned int file_offset,
                                           unsigned long destination_va,
                                           unsigned long size,
                                           unsigned long image_base,
                                           unsigned long *program_pages,
                                           unsigned int page_count)
{
    unsigned char buffer[USER_LOAD_CHUNK_SIZE];
    unsigned long copied;

    if (file_seek(file, file_offset) != 0)
    {
        return -1;
    }

    copied = 0;
    while (copied < size)
    {
        unsigned int chunk_size;
        unsigned long destination_offset;
        unsigned int page_index;
        unsigned int page_offset;
        unsigned int copy_size;
        int read_size;

        chunk_size = USER_LOAD_CHUNK_SIZE;
        if ((unsigned long)chunk_size > (size - copied))
        {
            chunk_size = (unsigned int)(size - copied);
        }

        read_size = file_read(file, buffer, chunk_size);
        if (read_size != (int)chunk_size)
        {
            return -1;
        }

        destination_offset = (destination_va - image_base) + copied;
        page_index = (unsigned int)(destination_offset / PMM_PAGE_SIZE);
        page_offset = (unsigned int)(destination_offset % PMM_PAGE_SIZE);
        if (page_index >= page_count)
        {
            return -1;
        }

        copy_size = PMM_PAGE_SIZE - page_offset;
        if (copy_size > chunk_size)
        {
            copy_size = chunk_size;
        }

        memcpy((void *)(program_pages[page_index] + page_offset), buffer, copy_size);
        copied += copy_size;

        if (copy_size < chunk_size)
        {
            unsigned int remainder;

            page_index++;
            if (page_index >= page_count)
            {
                return -1;
            }

            remainder = chunk_size - copy_size;
            memcpy((void *)program_pages[page_index], buffer + copy_size, remainder);
            copied += remainder;
        }
    }

    return 0;
}

static int user_zero_range_in_pages(unsigned long start_va,
                                    unsigned long size,
                                    unsigned long image_base,
                                    unsigned long *program_pages,
                                    unsigned int page_count)
{
    unsigned long cleared;

    cleared = 0;
    while (cleared < size)
    {
        unsigned long destination_offset;
        unsigned int page_index;
        unsigned int page_offset;
        unsigned int clear_size;

        destination_offset = (start_va - image_base) + cleared;
        page_index = (unsigned int)(destination_offset / PMM_PAGE_SIZE);
        page_offset = (unsigned int)(destination_offset % PMM_PAGE_SIZE);
        if (page_index >= page_count)
        {
            return -1;
        }

        clear_size = PMM_PAGE_SIZE - page_offset;
        if ((unsigned long)clear_size > (size - cleared))
        {
            clear_size = (unsigned int)(size - cleared);
        }

        memset((void *)(program_pages[page_index] + page_offset), 0, clear_size);
        cleared += clear_size;
    }

    return 0;
}

static int user_elf_header_valid(const elf64_header_t *header, unsigned int file_size)
{
    unsigned long program_headers_end;

    if (file_size < sizeof(*header))
    {
        return 0;
    }

    if (header->ident[0] != ELF_MAGIC0 ||
        header->ident[1] != ELF_MAGIC1 ||
        header->ident[2] != ELF_MAGIC2 ||
        header->ident[3] != ELF_MAGIC3)
    {
        return 0;
    }

    if (header->ident[ELF_EI_CLASS] != ELF_CLASS_64)
    {
        return 0;
    }

    if (header->ident[ELF_EI_DATA] != ELF_DATA_LITTLE_ENDIAN)
    {
        return 0;
    }

    if (header->ident[ELF_EI_VERSION] != ELF_VERSION_CURRENT ||
        header->version != ELF_VERSION_CURRENT)
    {
        return 0;
    }

    if (header->machine != ELF_MACHINE_AARCH64)
    {
        return 0;
    }

    if (header->type != ELF_TYPE_EXECUTABLE)
    {
        return 0;
    }

    if (header->ehsize != sizeof(*header) ||
        header->phentsize != sizeof(elf64_program_header_t) ||
        header->phnum == 0U)
    {
        return 0;
    }

    program_headers_end = header->phoff +
                          ((unsigned long)header->phnum * header->phentsize);
    if (program_headers_end < header->phoff || program_headers_end > file_size)
    {
        return 0;
    }

    return 1;
}

static int user_run_flat_file(file_t *file, const char *path)
{
    unsigned long code_pages[USER_FLAT_BINARY_MAX_PAGES];
    unsigned int program_size;
    unsigned long stack_page;
    unsigned int code_page_count;
    int result;

    program_size = file->size;
    code_page_count = user_code_page_count(program_size);
    if (!user_program_range_valid(USER_CODE_VA, code_page_count) ||
        code_page_count > USER_FLAT_BINARY_MAX_PAGES)
    {
        return -1;
    }

    if (user_alloc_program_pages(code_pages, code_page_count) != 0)
    {
        return -1;
    }

    stack_page = (unsigned long)pmm_alloc_page();
    if (stack_page == 0)
    {
        return -1;
    }

    memset((void *)stack_page, 0, PMM_PAGE_SIZE);

    if (user_load_file_into_pages(file, code_pages, code_page_count) != 0)
    {
        return -1;
    }

    result = user_run_loaded_pages(path,
                                   USER_CODE_VA,
                                   USER_CODE_VA,
                                   code_pages,
                                   0,
                                   code_page_count,
                                   stack_page,
                                   program_size);
    return result;
}

static int user_run_elf_file(file_t *file, const char *path)
{
    elf64_header_t header;
    unsigned int program_header_index;
    unsigned long image_base;
    unsigned long image_end;
    unsigned int page_count;
    unsigned long stack_page;
    unsigned long program_size;
    int found_load_segment;
    int entry_mapped;
    int result;

    if (user_read_exact_at(file, 0, &header, sizeof(header)) != 0)
    {
        return -1;
    }

    if (!user_elf_header_valid(&header, file->size))
    {
        return -1;
    }

    image_base = ~0UL;
    image_end = 0UL;
    found_load_segment = 0;
    for (program_header_index = 0; program_header_index < header.phnum; program_header_index++)
    {
        elf64_program_header_t program_header;
        unsigned int header_offset;
        unsigned long segment_start;
        unsigned long segment_end;

        header_offset = (unsigned int)(header.phoff +
                                       ((unsigned long)program_header_index * header.phentsize));
        if (user_read_exact_at(file, header_offset, &program_header, sizeof(program_header)) != 0)
        {
            return -1;
        }

        if (program_header.type != ELF_PROGRAM_TYPE_LOAD || program_header.memsz == 0UL)
        {
            continue;
        }

        if (program_header.memsz < program_header.filesz)
        {
            return -1;
        }

        if (program_header.offset + program_header.filesz < program_header.offset ||
            program_header.offset + program_header.filesz > file->size)
        {
            return -1;
        }

        if (program_header.vaddr + program_header.memsz < program_header.vaddr)
        {
            return -1;
        }

        segment_start = program_header.vaddr & ~(PMM_PAGE_SIZE - 1UL);
        segment_end = (program_header.vaddr + program_header.memsz + PMM_PAGE_SIZE - 1UL) &
                      ~(PMM_PAGE_SIZE - 1UL);
        if (segment_start < USER_CODE_VA || segment_end > (USER_STACK_TOP - PMM_PAGE_SIZE))
        {
            return -1;
        }

        if (!found_load_segment || segment_start < image_base)
        {
            image_base = segment_start;
        }
        if (!found_load_segment || segment_end > image_end)
        {
            image_end = segment_end;
        }

        found_load_segment = 1;
    }

    if (!found_load_segment || image_end <= image_base)
    {
        return -1;
    }

    page_count = user_code_page_count(image_end - image_base);
    if (!user_program_range_valid(image_base, page_count))
    {
        return -1;
    }

    if (header.entry < image_base || header.entry >= image_end)
    {
        return -1;
    }

    if (user_alloc_program_pages(user_program_pages, page_count) != 0)
    {
        return -1;
    }

    memset(user_program_page_writable, 0, page_count);

    entry_mapped = 0;
    for (program_header_index = 0; program_header_index < header.phnum; program_header_index++)
    {
        elf64_program_header_t program_header;
        unsigned int header_offset;
        unsigned long segment_offset;
        unsigned int first_page_index;
        unsigned int last_page_index;
        unsigned int page_index;

        header_offset = (unsigned int)(header.phoff +
                                       ((unsigned long)program_header_index * header.phentsize));
        if (user_read_exact_at(file, header_offset, &program_header, sizeof(program_header)) != 0)
        {
            return -1;
        }

        if (program_header.type != ELF_PROGRAM_TYPE_LOAD || program_header.memsz == 0UL)
        {
            continue;
        }

        if (header.entry >= program_header.vaddr &&
            header.entry < (program_header.vaddr + program_header.memsz))
        {
            entry_mapped = 1;
        }

        segment_offset = program_header.vaddr - image_base;
        first_page_index = (unsigned int)(segment_offset / PMM_PAGE_SIZE);
        last_page_index =
            (unsigned int)((segment_offset + program_header.memsz - 1UL) / PMM_PAGE_SIZE);
        if (last_page_index >= page_count)
        {
            return -1;
        }

        if ((program_header.flags & ELF_PROGRAM_FLAG_WRITE) != 0U)
        {
            for (page_index = first_page_index; page_index <= last_page_index; page_index++)
            {
                user_program_page_writable[page_index] = 1U;
            }
        }

        if (program_header.filesz > 0UL &&
            user_copy_file_range_into_pages(file,
                                            (unsigned int)program_header.offset,
                                            program_header.vaddr,
                                            program_header.filesz,
                                            image_base,
                                            user_program_pages,
                                            page_count) != 0)
        {
            return -1;
        }

        if (program_header.memsz > program_header.filesz &&
            user_zero_range_in_pages(program_header.vaddr + program_header.filesz,
                                     program_header.memsz - program_header.filesz,
                                     image_base,
                                     user_program_pages,
                                     page_count) != 0)
        {
            return -1;
        }
    }

    if (!entry_mapped)
    {
        return -1;
    }

    stack_page = (unsigned long)pmm_alloc_page();
    if (stack_page == 0)
    {
        return -1;
    }

    memset((void *)stack_page, 0, PMM_PAGE_SIZE);

    program_size = image_end - image_base;
    result = user_run_loaded_pages(path,
                                   image_base,
                                   header.entry,
                                   user_program_pages,
                                   user_program_page_writable,
                                   page_count,
                                   stack_page,
                                   program_size);
    return result;
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
    unsigned long code_pages[1];
    unsigned long stack_page;
    unsigned long program_size;

    code_pages[0] = (unsigned long)pmm_alloc_page();
    stack_page = (unsigned long)pmm_alloc_page();

    if (code_pages[0] == 0 || stack_page == 0)
    {
        panic("user page allocation failed");
    }

    program_size = user_blob_size();
    user_prepare_pages(code_pages, 1, stack_page);
    return user_run_loaded_pages("demo",
                                 USER_CODE_VA,
                                 USER_CODE_VA,
                                 code_pages,
                                 0,
                                 1,
                                 stack_page,
                                 program_size);
}

int user_run_file(const char *path)
{
    file_t *file;
    elf64_header_t header;
    int result;

    file = file_open(path);
    if (file == 0)
    {
        return -1;
    }

    if (user_read_exact_at(file, 0, &header, sizeof(header)) == 0 &&
        header.ident[0] == ELF_MAGIC0 &&
        header.ident[1] == ELF_MAGIC1 &&
        header.ident[2] == ELF_MAGIC2 &&
        header.ident[3] == ELF_MAGIC3)
    {
        result = user_run_elf_file(file, path);
    }
    else
    {
        if (file_seek(file, 0) != 0)
        {
            file_close(file);
            return -1;
        }
        result = user_run_flat_file(file, path);
    }

    file_close(file);
    return result;
}
