#ifndef APIC_H
#define APIC_H

#include <stdint.h>

// Default Physical Address of the Local APIC
#define LAPIC_DEFAULT_BASE 0xFEE00000

// LAPIC Register Offsets
#define LAPIC_ID            0x0020   // Local APIC ID
#define LAPIC_VER           0x0030   // Local APIC Version
#define LAPIC_TPR           0x0080   // Task Priority Register
#define LAPIC_APR           0x0090   // Arbitration Priority Register
#define LAPIC_PPR           0x00A0   // Processor Priority Register 
#define LAPIC_EOI           0x00B0   // End of Interrupt Register
#define LAPIC_RRD           0x00C0   // Remote Read Register
#define LAPIC_LDR           0x00D0   // Logical Destination Register
#define LAPIC_DFR           0x00E0   // Destination Format Register
#define LAPIC_SVR           0x00F0   // Spurious Interrupt Vector Register
#define LAPIC_ISR           0x0100   // In-Service Register (0x100-0x170)
#define LAPIC_TMR           0x0180   // Trigger Mode Register (0x180-0x1F0)
#define LAPIC_IRR           0x0200   // Interrupt Request Register (0x200-0x270)
#define LAPIC_ESR           0x0280   // Error Status Register 
#define LAPIC_ICR_LOW       0x0300   // Interrupt Command Register (0-31)
#define LAPIC_ICR_HIGH      0x0310   // Interrupt Command Register (32-63)
#define LAPIC_LVT_TIMER     0x0320   // LVT Timer Register
#define LAPIC_LVT_THERMAL   0x0330   // LVT Thermal Sensor Register
#define LAPIC_LVT_PERF      0x0340   // LVT Performance Counter Register
#define LAPIC_LVT_LINT0     0x0350   // LVT LINT0 Register 
#define LAPIC_LVT_LINT1     0x0360   // LVT LINT1 Register 
#define LAPIC_LVT_ERROR     0x0370   // LVT Error Register 
#define LAPIC_TICR          0x0380   // Timer Initial Count Register 
#define LAPIC_TCCR          0x0390   // Timer Current Count Register 
#define LAPIC_TDCR          0x03E0   // Timer Divide Configuration Register 
#define LAPIC_SVR_ENABLE    0x100    // Unit Enable Bit

//
// ICR
//

// Delivery Mode
#define ICR_FIXED              0x000
#define ICR_LOWEST_PRIORITY    0x100
#define ICR_SMI                0x200
#define ICR_NMI                0x400
#define ICR_INIT               0x500
#define ICR_STARTUP            0x600

// Destination Mode & Status
#define ICR_PHYSICAL           0x0000
#define ICR_LOGICAL            0x0800
#define ICR_IDLE               0x0000
#define ICR_SEND_PENDING       0x1000

// Level & Trigger
#define ICR_DEASSERT           0x0000
#define ICR_ASSERT             0x4000
#define ICR_EDGE               0x0000
#define ICR_LEVEL              0x8000

// Destination Shorthand
#define ICR_SHORTHAND_NONE     0x00000
#define ICR_SHORTHAND_SELF     0x40000
#define ICR_SHORTHAND_ALL      0x80000
#define ICR_SHORTHAND_OTHERS   0xC0000

// Channels
#define IPI_VECTOR_TEST        0xFD
#define IPI_VECTOR_HALT        0xFE  

// Driver Functions
void lapic_init(uintptr_t virt_addr);
void lapic_init_ap();
void lapic_write(uint32_t reg, uint32_t data);
uint32_t lapic_read(uint32_t reg);
void lapic_send_eoi();
void lapic_timer_init(uint32_t ms_interval, uint8_t vector);
void lapic_timer_calibrate();

void lapic_wait_for_delivery();
void lapic_send_ipi(uint8_t target_lapic_id, uint8_t vector);
void lapic_broadcast_ipi(uint8_t vector);

#endif
