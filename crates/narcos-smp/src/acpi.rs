/// ACPI table parsing structures and logic for detecting processor cores.

#[repr(C, packed)]
struct RsdpHeader {
    signature: [u8; 8],
    checksum: u8,
    oem_id: [u8; 6],
    revision: u8,
    rsdt_address: u32,
}

#[repr(C, packed)]
struct SdtHeader {
    signature: [u8; 4],
    length: u32,
    revision: u8,
    checksum: u8,
    oem_id: [u8; 6],
    oem_table_id: [u8; 8],
    oem_revision: u32,
    creator_id: u32,
    creator_revision: u32,
}

/// Information about a CPU core detected from MADT.
#[derive(Copy, Clone, Debug)]
pub struct CpuCoreInfo {
    pub processor_id: u8,
    pub apic_id: u8,
    pub flags: u32,
}

/// The final parsed ACPI and MADT configuration.
#[derive(Copy, Clone)]
pub struct AcpiConfig {
    pub lapic_address: u32,
    pub cores: [Option<CpuCoreInfo>; 32],
    pub core_count: usize,
}

impl AcpiConfig {
    pub const fn empty() -> Self {
        Self {
            lapic_address: 0,
            cores: [None; 32],
            core_count: 0,
        }
    }
}

unsafe extern "C" {
    fn serial_write(str: *const u8);
}

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

/// Safely parse ACPI tables given the RSDP physical address.
pub unsafe fn parse_acpi_tables(rsdp_addr: u32) -> Option<AcpiConfig> {
    log_serial("[acpi] entering parse_acpi_tables\n");
    log_hex("[acpi] rsdp_addr: ", rsdp_addr);
    if rsdp_addr == 0 {
        log_serial("[acpi-err] rsdp_addr is 0\n");
        return None;
    }

    log_serial("[acpi] casting rsdp_ptr\n");
    let rsdp_ptr = rsdp_addr as *const RsdpHeader;
    
    // Verify signature "RSD PTR " using safe unaligned reads
    log_serial("[acpi] reading rsdp signature\n");
    let signature = unsafe { core::ptr::addr_of!((*rsdp_ptr).signature).read_unaligned() };
    log_serial("[acpi] verifying rsdp signature\n");
    if signature != *b"RSD PTR " {
        log_serial("[acpi-err] RSDP signature mismatch\n");
        return None;
    }

    log_serial("[acpi] reading rsdt_addr\n");
    let rsdt_addr = unsafe { core::ptr::addr_of!((*rsdp_ptr).rsdt_address).read_unaligned() };
    log_hex("[acpi] rsdt_addr: ", rsdt_addr);
    if rsdt_addr == 0 {
        log_serial("[acpi-err] rsdt_addr is 0\n");
        return None;
    }

    log_serial("[acpi] casting rsdt_ptr\n");
    let rsdt_ptr = rsdt_addr as *const SdtHeader;
    log_serial("[acpi] reading rsdt signature\n");
    let rsdt_sig = unsafe { core::ptr::addr_of!((*rsdt_ptr).signature).read_unaligned() };
    log_serial("[acpi] verifying rsdt signature\n");
    if rsdt_sig != *b"RSDT" {
        log_serial("[acpi-err] RSDT signature mismatch\n");
        return None;
    }

    log_serial("[acpi] reading rsdt length\n");
    let rsdt_len = unsafe { core::ptr::addr_of!((*rsdt_ptr).length).read_unaligned() };

    // Number of pointers inside RSDT
    let entries = (rsdt_len - core::mem::size_of::<SdtHeader>() as u32) / 4;
    let array_ptr = (rsdt_addr + core::mem::size_of::<SdtHeader>() as u32) as *const u32;

    log_serial("[acpi] loops through entries\n");
    let mut config = AcpiConfig::empty();

    for i in 0..entries {
        let table_addr = unsafe { core::ptr::read_unaligned(array_ptr.add(i as usize)) };
        if table_addr == 0 {
            continue;
        }

        let header_ptr = table_addr as *const SdtHeader;
        let header_sig = unsafe { core::ptr::addr_of!((*header_ptr).signature).read_unaligned() };
        if header_sig == *b"APIC" {
            log_serial("[acpi] found MADT (APIC) table\n");
            // Found MADT (Multiple APIC Description Table)
            unsafe { parse_madt(table_addr, &mut config) };
            return Some(config);
        }
    }

    log_serial("[acpi-err] MADT table not found\n");
    None
}

/// Parse Multiple APIC Description Table (MADT).
unsafe fn parse_madt(madt_addr: u32, config: &mut AcpiConfig) {
    let header_ptr = madt_addr as *const SdtHeader;
    let limit = unsafe { core::ptr::addr_of!((*header_ptr).length).read_unaligned() };

    let lapic_address_ptr = (madt_addr + core::mem::size_of::<SdtHeader>() as u32) as *const u32;
    config.lapic_address = unsafe { core::ptr::read_unaligned(lapic_address_ptr) };

    let mut offset = core::mem::size_of::<SdtHeader>() as u32 + 8; // Skip LAPIC address and flags

    while offset < limit {
        let entry_ptr = (madt_addr + offset) as *const u8;
        let entry_type = unsafe { core::ptr::read_unaligned(entry_ptr) };
        let entry_len = unsafe { core::ptr::read_unaligned(entry_ptr.add(1)) };

        if entry_len == 0 {
            break;
        }

        match entry_type {
            0 => {
                // Processor Local APIC
                let processor_id = unsafe { core::ptr::read_unaligned(entry_ptr.add(2)) };
                let apic_id = unsafe { core::ptr::read_unaligned(entry_ptr.add(3)) };
                let flags = unsafe { core::ptr::read_unaligned(entry_ptr.add(4) as *const u32) };

                // Check if the core is enabled (bit 0 of flags) or online capable (bit 1)
                if (flags & 1) != 0 && config.core_count < 32 {
                    config.cores[config.core_count] = Some(CpuCoreInfo {
                        processor_id,
                        apic_id,
                        flags,
                    });
                    config.core_count += 1;
                }
            }
            _ => {}
        }

        offset += entry_len as u32;
    }
}
