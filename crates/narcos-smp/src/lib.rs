#![no_std]

mod sync;
mod acpi;
mod apic;

use core::panic::PanicInfo;
pub use sync::Spinlock;
use acpi::{AcpiConfig, parse_acpi_tables};
use apic::LocalApic;

// Global ACPI and SMP configuration protected by our safe Spinlock.
static CONFIG: Spinlock<AcpiConfig> = Spinlock::new(AcpiConfig::empty());

// External C functions so Rust can write output to serial port and map MMIO regions.
unsafe extern "C" {
    fn serial_write(str: *const u8);
    fn paging_map_physical(phys_addr: usize, size: usize, flags: u64) -> *mut u8;
}

/// Helper function to write to the serial log from Rust.
fn log_serial(msg: &str) {
    let mut buf = [0u8; 128];
    let len = core::cmp::min(msg.len(), 127);
    buf[..len].copy_from_slice(&msg.as_bytes()[..len]);
    buf[len] = 0; // null-terminate for C
    unsafe {
        serial_write(buf.as_ptr());
    }
}

fn log_hex(label: &str, val: u32) {
    log_serial(label);
    let mut buf = [0u8; 11];
    buf[0] = b'0';
    buf[1] = b'x';
    let chars = b"0123456789ABCDEF";
    for i in 0..8 {
        buf[9 - i] = chars[((val >> (i * 4)) & 0x0F) as usize];
    }
    buf[10] = 0; // null-terminate
    unsafe {
        serial_write(buf.as_ptr());
    }
    log_serial("\n");
}

/// Wakes up AP cores and initializes SMP on narcOs.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn smp_init(rsdp_addr: u32) -> i32 {
    log_serial("[smp] starting rust-smp module initialization...\n");

    let parsed_config = match unsafe { parse_acpi_tables(rsdp_addr) } {
        Some(c) => c,
        None => {
            log_serial("[smp-err] failed to parse ACPI tables!\n");
            return -1;
        }
    };

    let core_count = parsed_config.core_count;
    let lapic_addr = parsed_config.lapic_address;

    log_serial("[smp] acpi successfully parsed\n");
    log_hex("[smp] physical lapic address: ", lapic_addr);

    // Map the LAPIC physical address to a virtual address using the kernel's paging system.
    // PAGING_FLAG_WRITE (0x02) | PAGING_FLAG_CACHE_DISABLE (0x10) = 0x12
    log_serial("[smp] mapping local apic mmio region...\n");
    let lapic_virt = unsafe { paging_map_physical(lapic_addr as usize, 4096, 0x12) };
    if lapic_virt.is_null() {
        log_serial("[smp-err] failed to map local apic mmio region!\n");
        return -1;
    }
    let lapic_virt_addr = lapic_virt as u32;
    log_hex("[smp] local apic mapped to virtual address: ", lapic_virt_addr);

    // Store the full configuration (with mapped virtual LAPIC address) globally.
    {
        let mut global_cfg = CONFIG.lock();
        *global_cfg = parsed_config;
        global_cfg.lapic_virt_address = lapic_virt_addr;
    }

    // Initialize Local APIC on BSP (Bootstrap Processor - Core 0) using the mapped virtual address
    let lapic = unsafe { LocalApic::new(lapic_virt_addr) };
    unsafe { lapic.init_on_core() };

    let current_id = unsafe { lapic.get_id() };

    log_serial("[smp] local apic enabled on bsp core\n");

    if core_count > 1 {
        log_serial("[smp] multiple cpu cores detected! waking up APs...\n");
        
        // Wake up each application processor (AP) core
        for i in 0..core_count {
            if let Some(core) = parsed_config.cores[i] {
                if core.apic_id != current_id {
                    // Send waking signals to target APIC using trampoline vector at 0x08 (0x08 * 4096 = 32KB boundary)
                    unsafe { lapic.wakeup_ap_core(core.apic_id, 0x08) };
                    log_serial("[smp] triggered startup sequence on core\n");
                }
            }
        }
    } else {
        log_serial("[smp] single core mode enabled\n");
    }

    core_count as i32
}

/// Returns the total number of detected processor cores.
#[unsafe(no_mangle)]
pub extern "C" fn smp_get_core_count() -> i32 {
    let cfg = CONFIG.lock();
    cfg.core_count as i32
}

/// Returns the LAPIC ID of the calling CPU core.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn smp_get_current_core_id() -> u8 {
    let lapic_virt = {
        let cfg = CONFIG.lock();
        cfg.lapic_virt_address
    };
    if lapic_virt == 0 {
        return 0;
    }
    let lapic = unsafe { LocalApic::new(lapic_virt) };
    unsafe { lapic.get_id() }
}

/// The bare-metal panic handler in no_std environment.
#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    log_serial("[smp-panic] rust-smp module kernel panic occurred!\n");
    loop {
        unsafe {
            core::arch::asm!("hlt");
        }
    }
}
