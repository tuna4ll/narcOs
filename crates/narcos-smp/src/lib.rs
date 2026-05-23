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

// External C printing functions so Rust can write output to the screen or serial port.
unsafe extern "C" {
    fn serial_write(str: *const u8);
    fn vga_print_color(str: *const u8, color: u8);
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

/// Helper function to write to the VGA display from Rust.
fn log_vga(msg: &str, color: u8) {
    let mut buf = [0u8; 128];
    let len = core::cmp::min(msg.len(), 127);
    buf[..len].copy_from_slice(&msg.as_bytes()[..len]);
    buf[len] = 0; // null-terminate for C
    unsafe {
        vga_print_color(buf.as_ptr(), color);
    }
}

/// Wakes up AP cores and initializes SMP on narcOs.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn smp_init(rsdp_addr: u32) -> i32 {
    log_serial("[smp] starting rust-smp module initialization...\n");

    let parsed_config = match unsafe { parse_acpi_tables(rsdp_addr) } {
        Some(c) => c,
        None => {
            log_vga("[smp-err] failed to parse ACPI tables!\n", 0x0C);
            log_serial("[smp-err] failed to parse ACPI tables!\n");
            return -1;
        }
    };

    // Store the configuration globally (makes a copy since AcpiConfig is Copy).
    {
        let mut global_cfg = CONFIG.lock();
        *global_cfg = parsed_config;
    }

    let core_count = parsed_config.core_count;
    let lapic_addr = parsed_config.lapic_address;

    log_serial("[smp] acpi successfully parsed\n");

    // Initialize Local APIC on BSP (Bootstrap Processor - Core 0)
    let lapic = unsafe { LocalApic::new(lapic_addr) };
    unsafe { lapic.init_on_core() };

    let current_id = unsafe { lapic.get_id() };

    log_serial("[smp] local apic enabled on bsp core\n");

    if core_count > 1 {
        log_vga("[smp] multiple cpu cores detected! waking up APs...\n", 0x0A);
        
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
        log_vga("[smp] single core mode enabled\n", 0x0F);
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
    let lapic_addr = {
        let cfg = CONFIG.lock();
        cfg.lapic_address
    };
    if lapic_addr == 0 {
        return 0;
    }
    let lapic = unsafe { LocalApic::new(lapic_addr) };
    unsafe { lapic.get_id() }
}

/// The bare-metal panic handler in no_std environment.
#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    log_serial("[smp-panic] rust-smp module kernel panic occurred!\n");
    log_vga("[smp-panic] rust smp panic!\n", 0x0C);
    loop {
        unsafe {
            core::arch::asm!("hlt");
        }
    }
}
