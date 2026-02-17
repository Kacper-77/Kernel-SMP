#include <boot_info.h>
#include <serial.h>
#include <acpi.h>
#include <gdt.h>
#include <idt.h>
#include <pmm.h>
#include <vmm.h>
#include <apic.h>
#include <smp.h>
#include <std_funcs.h>
#include <cpu.h>
#include <test.h>
#include <timer.h>
#include <kmalloc.h>
#include <spinlock.h>
#include <panic.h>
#include <sched.h>
#include <userlib.h>
#include <syscall.h>
#include <ioapic.h>
#include <i8042.h>
#include <io.h>
#include <tar.h>
#include <elf.h>

#include <stdint.h>
#include <stddef.h>

// Global spinlock flag
int g_lock_enabled = 0;

syscall_ptr_t sys_table[10] = { 0 };

// Forward declaration
void kernel_main_high(BootInfo *bi);

static void user_test_task() {
    volatile uint64_t counter = 0;

    while(1) {
        counter++;
        if (counter % 100000000 == 0) {
            __asm__ volatile (
                "syscall"
                : : "a"(2) : "rcx", "r11"
            );
        }
    }
}

static void user_test_task_2() {
    char msg[] = {'T', 'a', 's', 'k', ' ', '2', '\n', 0};
    char msg2[] = {'D', 'o', 'n', 'e', '\n', 0};
    
    volatile uint64_t counter = 0;
    while(counter < 5) {
        u_print(msg);
        u_sleep(10);
        counter++;
    }
    u_print(msg2);
    u_exit();
}


static void user_test_task_echo() {
    char msg[] = {'S','H','E','L','L','\n', 0};
    u_print(msg);

    while(1) {
        char c = u_read_kbd();
        if (c != 0) {
            char buf[2] = {c, 0};
            u_print(buf);
        }
        u_sleep(50);
    }
}

static void task_a() {
    int x = 0;
    while(x < 10) {
        kprint("A");
        msleep(100);
        x++;
    }
    task_exit();
}

static void task_b() {
    int x = 0;
    while(x < 15) {
        kprint("B");
        msleep(100);
        x++;
    }
    task_exit();
}

// Low-half entry point (Bootstrap)
__attribute__((sysv_abi, section(".text.entry")))
void kernel_main(BootInfo *bi) {
    __asm__ volatile(
        "and $-16, %%rsp\n\t"
        "sub $8, %%rsp"
        ::: "rsp"
    );
    __asm__ volatile("cli");
    pmm_init(bi);
    init_serial();
    vmm_init(bi);

    BootInfo* bi_virt = (BootInfo*)phys_to_virt((uintptr_t)bi);

    __asm__ __volatile__ (
        "movabs $kernel_main_high, %%rax \n\t"
        "jmp *%%rax"
        : : "D"(bi_virt) : "rax"
    );

    while(1);
}

// High-half entry point
void kernel_main_high(BootInfo *bi) {
    kprint("BootInfo pointer: "); kprint_hex((uintptr_t)bi); kprint("\n");
    cpu_init_bsp();
    cpu_init_syscalls();
    init_sys_table();
    
    idt_init();

    kprint("###   Greetings from Higher Half!   ###\n");
    
    kmalloc_init(); kprint("Heap initialized.\n");

    kprint("Starting Coalescing Test\n");

    void* t1 = kmalloc(100);
    void* t2 = kmalloc(100);
    void* t3 = kmalloc(100);

    kprint("t1: "); kprint_hex((uintptr_t)t1); kprint("\n");
    kprint("t2: "); kprint_hex((uintptr_t)t2); kprint("\n");
    kprint("t3: "); kprint_hex((uintptr_t)t3); kprint("\n");

    kfree(t1);
    kfree(t2);
    
    void* t4 = kmalloc(200);
    kprint("t4 (should reuse t1 space): "); kprint_hex((uintptr_t)t4); kprint("\n");

    if (t4 == t1) {
        kprint("SUCCESS: Coalescing works! t4 reused and merged t1+t2.\n");
    } else {
        kprint("DEBUG: t4 is at: "); kprint_hex((uintptr_t)t4); kprint("\n");
    }

    strcpy((char*)t4, "## DATA SAVED IN T4 ##\n");
    kprint((char*)t4); kprint("\n");

    kmalloc_dump();

    // BSP
    draw_test_squares_safe(1, 
                           (uint32_t*)bi->fb.framebuffer_base, 
                           bi->fb.pixels_per_scanline);

    if (bi->acpi.rsdp != 0) {
        acpi_rsdp_t *rsdp = (acpi_rsdp_t*)phys_to_virt((uintptr_t)bi->acpi.rsdp);
        acpi_madt_t *madt = (acpi_madt_t*)acpi_find_table(rsdp, "APIC");

        if (madt) {
            uintptr_t v_lapic = (uintptr_t)vmm_map_device(vmm_get_pml4(), 
                                                (uintptr_t)phys_to_virt(madt->local_apic_address),
                                                (uintptr_t)madt->local_apic_address, 
                                                4096);
            lapic_init(v_lapic);
            g_lock_enabled = 1;

            lapic_timer_calibrate();
            lapic_timer_init(10, 32);

            ioapic_init(0xFEC00000); 
            ioapic_set_irq(1, 0, 33);
            i8042_init();

            sched_init();

            void* ramdisk_vaddr = (void*)phys_to_virt((uintptr_t)bi->ramdisk_addr);
            tar_init(ramdisk_vaddr, bi->ramdisk_size);

            kprint("##### RAMDISK DIAGNOSTIC #####\n");
            kprint("Address Phys: "); kprint_hex((uintptr_t)bi->ramdisk_addr); kprint("\n");
            kprint("Address Virt: "); kprint_hex((uintptr_t)ramdisk_vaddr); kprint("\n");
            kprint("Size:         "); kprint_hex(bi->ramdisk_size); kprint(" bytes\n");

            size_t size = 0;
            void* file = NULL;

            file = tar_lookup("init.elf", &size);
            if (!file) {
                kprint("Trying alternative path...\n");
                file = tar_lookup("ramdisk/init.elf", &size);
            }

           if (file) {
                kprint("SUCCESS! Found init.elf at: "); kprint_hex((uintptr_t)file);
                kprint(" Size: "); kprint_hex(size); kprint("\n");
                
                if (arch_task_spawn_elf(file)) {
                    kprint("Launching init.elf (TID will be assigned)...\n");
                } else {
                    kprint("ERROR: Failed to spawn init.elf - check VMM or ELF header.\n");
                }
            }

            kprint("Starting SMP initialization...\n");
            smp_init(bi);

            if (get_cpu_count_test() > 1) {
                kprint("BSP: IPI_TEST CPU 1...\n");
               lapic_send_ipi(1, IPI_VECTOR_TEST); 
            }

            kprint("BSP: Broadcasting IPI...\n");
            lapic_broadcast_ipi(IPI_VECTOR_TEST);

            kmalloc_dump();

            // panic("End of boot test - halting system.");
        }
    }

    vmm_unmap_range(vmm_get_pml4(), 0x0, 0x100000); // UEFI/BIOS area
    vmm_unmap_range(vmm_get_pml4(), KERNEL_PHYS_BASE, 0x400000); // Kernel identity
    kprint("Kernel isolated.\n");

    arch_task_create_user(user_test_task);
    arch_task_create_user(user_test_task_2);
    arch_task_create_user(user_test_task_echo);

    __asm__ volatile("sti");

    kprint("Testing timer with msleep (5 seconds)...\n");
    
    for (int i = 1; i <= 5; i++) {
        msleep(1000);
        kprint("Tick ");
        kprint_hex(i);
        kprint("s...\n");
    }
    // arch_task_create(task_a);
    // arch_task_create(task_b);

    kprint("Timer TEST PASSED! Uptime: ");
    kprint_hex(get_uptime_ms());
    kprint(" ms\n");

    kprint("###   Higher Half kernel is now idling.   ###\n");

    while(1) {
        sched_reap();
        __asm__ volatile("hlt");
        sched_yield();
    }
}
