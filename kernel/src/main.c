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
#include <atomic.h>
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

syscall_ptr_t sys_table[20] = { 0 };

void kernel_main_high(BootInfo *bi);

static void task_a() {
    int x = 0;
    while(x < 10) {
        kprintf("A");
        msleep(10);
        x++;
    }
    task_exit();
}

static void task_b() {
    int x = 0;
    while(x < 15) {
        kprintf("B");
        msleep(10);
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
    cpu_init_bsp();
    cpu_init_syscalls();
    init_sys_table();
    
    idt_init();

    kprintf("###   Greetings from Higher Half!   ###\n");
    
    kmalloc_init(); 
    kprintf("Heap initialized.\n");

    kprintf("Starting Coalescing Test\n");

    void* t1 = kmalloc(3000);
    void* t2 = kmalloc(4050);
    void* t3 = kmalloc(5500);

    kprintf("t1: %p\n", t1);
    kprintf("t2: %p\n", t2);
    kprintf("t3: %p\n", t3);

    kfree(t1);
    kfree(t2);
    
    void* t4 = kmalloc(4500);
    kprintf("t4 (should reuse t1 space): %p\n", t4);

    if (t4 == t1) {
        kprintf("SUCCESS: Coalescing works! t4 reused and merged t1+t2.\n");
    } else {
        kprintf("DEBUG: t4 is at: %p\n", t4);
    }

    strcpy((char*)t4, "## DATA SAVED IN T4 ##\n");
    kprintf("%s\n", (char*)t4);

    kmalloc_dump();

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

            lapic_timer_init(5, 32);

            ioapic_init(0xFEC00000); 
            ioapic_set_irq(1, 0, 33);
            i8042_init();

            sched_init();

            void* ramdisk_vaddr = (void*)phys_to_virt((uintptr_t)bi->ramdisk.ramdisk_addr);
            tar_init(ramdisk_vaddr, bi->ramdisk.ramdisk_size);

            kprintf("##### RAMDISK DIAGNOSTIC #####\n");
            kprintf("Address Phys: %p\n", bi->ramdisk.ramdisk_addr);
            kprintf("Address Virt: %p\n", ramdisk_vaddr);
            kprintf("Size:         %x bytes\n", bi->ramdisk.ramdisk_size);

            size_t size = 0;
            void* file = NULL;

            file = tar_lookup("init.elf", &size);
            if (!file) {
                kprintf("Trying alternative path...\n");
                file = tar_lookup("ramdisk/init.elf", &size);
            }

           if (file) {
                kprintf("SUCCESS! Found init.elf at: %p Size: %x\n", file, size);
                
                if (arch_task_spawn_elf(file)) {
                    kprintf("Launching init.elf (TID will be assigned)...\n");
                } else {
                    kprintf("ERROR: Failed to spawn init.elf - check VMM or ELF header.\n");
                }
            }

            kprintf("Starting SMP initialization...\n");
            smp_init(bi);

            if (get_cpu_count_test() > 1) {
                kprintf("BSP: IPI_TEST CPU 1...\n");
                lapic_send_ipi(1, IPI_VECTOR_TEST); 
            }

            kprintf("BSP: Broadcasting IPI...\n");
            lapic_broadcast_ipi(IPI_VECTOR_TEST);
        }
    }

    vmm_unmap_range(vmm_get_pml4(), 0x0, 0x100000); // UEFI/BIOS area
    vmm_unmap_range(vmm_get_pml4(), KERNEL_PHYS_BASE, 0x400000); // Kernel identity
    kprintf("Kernel isolated.\n");

    __asm__ volatile("sti");

    kprintf("Testing timer with msleep (5 seconds)...\n");
    
    for (int i = 1; i <= 5; i++) {
        msleep(1000);
        kprintf("Tick %ds...\n", i);
    }

    arch_task_create(task_a);
    arch_task_create(task_b);

    kprintf("Timer TEST PASSED! Uptime: %dms\n", get_uptime_ms());

    kprintf("###   Higher Half kernel is now idling.   ###\n");

    kmalloc_dump();
    
    while(1) {
        log_flush();
        __asm__ volatile("hlt");
    }
}
