/// Local APIC (LAPIC) register mappings and operations.

const LAPIC_ID: u32 = 0x20;
const LAPIC_TPR: u32 = 0x80;
const LAPIC_EOI: u32 = 0xB0;
const LAPIC_SVR: u32 = 0xF0;
const LAPIC_ICR_LOW: u32 = 0x300;
const LAPIC_ICR_HIGH: u32 = 0x310;

/// The Local APIC driver.
pub struct LocalApic {
    base_addr: u32,
}

impl LocalApic {
    pub const unsafe fn new(base_addr: u32) -> Self {
        Self { base_addr }
    }

    /// Read a value from a Local APIC register.
    unsafe fn read(&self, reg: u32) -> u32 {
        let ptr = (self.base_addr + reg) as *const u32;
        unsafe { core::ptr::read_volatile(ptr) }
    }

    /// Write a value to a Local APIC register.
    unsafe fn write(&self, reg: u32, val: u32) {
        let ptr = (self.base_addr + reg) as *mut u32;
        unsafe { core::ptr::write_volatile(ptr, val) };
    }

    /// Read the ID of the current processor.
    pub unsafe fn get_id(&self) -> u8 {
        unsafe { (self.read(LAPIC_ID) >> 24) as u8 }
    }

    /// Signal End of Interrupt (EOI) to the Local APIC.
    pub unsafe fn signal_eoi(&self) {
        unsafe { self.write(LAPIC_EOI, 0) };
    }

    /// Initialize the Local APIC on the current processor core.
    pub unsafe fn init_on_core(&self) {
        // Clear Task Priority Register to enable all interrupts.
        unsafe { self.write(LAPIC_TPR, 0) };

        // Map spurious interrupt vector and enable the APIC (bit 8 of SVR).
        let svr = unsafe { self.read(LAPIC_SVR) };
        unsafe { self.write(LAPIC_SVR, svr | 0x100 | 0xFF) };
    }

    /// Send an Inter-Processor Interrupt (IPI) to wake up an AP core.
    pub unsafe fn send_ipi(&self, apic_id: u8, icr_low: u32) {
        // Wait for any previous IPI delivery to complete.
        while unsafe { (self.read(LAPIC_ICR_LOW) & (1 << 12)) != 0 } {
            core::hint::spin_loop();
        }

        // Set the destination in the high register.
        unsafe { self.write(LAPIC_ICR_HIGH, (apic_id as u32) << 24) };

        // Fire the interrupt via the low register.
        unsafe { self.write(LAPIC_ICR_LOW, icr_low) };
    }

    /// Wake up a dormant application processor (AP) core.
    pub unsafe fn wakeup_ap_core(&self, apic_id: u8, trampoline_vector: u8) {
        // 1. Send INIT IPI (assert)
        unsafe { self.send_ipi(apic_id, 0x00500) }; // Trigger mode: Level, level: Assert, Delivery mode: INIT

        // Wait 10ms (roughly simulated using spin wait loop in no_std)
        for _ in 0..10_000_000 {
            core::hint::spin_loop();
        }

        // 2. Send STARTUP IPI (SIPI) with the vector pointing to 4KB aligned trampoline page (vector * 4096)
        unsafe { self.send_ipi(apic_id, 0x00600 | (trampoline_vector as u32)) };

        // Wait 200us
        for _ in 0..200_000 {
            core::hint::spin_loop();
        }

        // Send a second STARTUP IPI just to be absolutely sure the AP boots reliably.
        unsafe { self.send_ipi(apic_id, 0x00600 | (trampoline_vector as u32)) };
    }
}
