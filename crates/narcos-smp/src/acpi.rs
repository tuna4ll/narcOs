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

/// Safely parse ACPI tables given the RSDP physical address.
pub unsafe fn parse_acpi_tables(rsdp_addr: u32) -> Option<AcpiConfig> {
    if rsdp_addr == 0 {
        return None;
    }

    let rsdp = unsafe { &*(rsdp_addr as *const RsdpHeader) };
    
    // Verify signature "RSD PTR "
    if rsdp.signature != *b"RSD PTR " {
        return None;
    }

    let rsdt_addr = rsdp.rsdt_address;
    if rsdt_addr == 0 {
        return None;
    }

    let rsdt = unsafe { &*(rsdt_addr as *const SdtHeader) };
    if rsdt.signature != *b"RSDT" {
        return None;
    }

    // Number of pointers inside RSDT
    let entries = (rsdt.length - core::mem::size_of::<SdtHeader>() as u32) / 4;
    let array_ptr = (rsdt_addr + core::mem::size_of::<SdtHeader>() as u32) as *const u32;

    let mut config = AcpiConfig::empty();

    for i in 0..entries {
        let table_addr = unsafe { *array_ptr.add(i as usize) };
        if table_addr == 0 {
            continue;
        }

        let header = unsafe { &*(table_addr as *const SdtHeader) };
        if header.signature == *b"APIC" {
            // Found MADT (Multiple APIC Description Table)
            unsafe { parse_madt(table_addr, &mut config) };
            return Some(config);
        }
    }

    None
}

/// Parse Multiple APIC Description Table (MADT).
unsafe fn parse_madt(madt_addr: u32, config: &mut AcpiConfig) {
    let header = unsafe { &*(madt_addr as *const SdtHeader) };
    let lapic_address_ptr = (madt_addr + core::mem::size_of::<SdtHeader>() as u32) as *const u32;
    config.lapic_address = unsafe { *lapic_address_ptr };

    let mut offset = core::mem::size_of::<SdtHeader>() as u32 + 8; // Skip LAPIC address and flags
    let limit = header.length;

    while offset < limit {
        let entry_ptr = (madt_addr + offset) as *const u8;
        let entry_type = unsafe { *entry_ptr };
        let entry_len = unsafe { *entry_ptr.add(1) };

        if entry_len == 0 {
            break;
        }

        match entry_type {
            0 => {
                // Processor Local APIC
                let processor_id = unsafe { *entry_ptr.add(2) };
                let apic_id = unsafe { *entry_ptr.add(3) };
                let flags = unsafe { *(entry_ptr.add(4) as *const u32) };

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
