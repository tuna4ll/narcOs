#include "ata.h"
#include "serial.h"
#include "string.h"

#define ATA_SR_ERR  0x01U
#define ATA_SR_DRQ  0x08U
#define ATA_SR_BSY  0x80U

#define ATA_CMD_READ_SECTORS    0x20U
#define ATA_CMD_WRITE_SECTORS   0x30U
#define ATA_CMD_CACHE_FLUSH     0xE7U
#define ATA_CMD_IDENTIFY        0xECU
#define ATA_CMD_IDENTIFY_PACKET 0xA1U
#define ATA_CMD_PACKET          0xA0U

#define ATAPI_CMD_READ_12       0xA8U

#define ATA_WAIT_LIMIT          1000000U
#define ATAPI_SECTOR_SIZE       2048U
#define ATAPI_BOOT_RECORD_LBA   17U
#define ATAPI_BOOT_CATALOG_OFF  71U

typedef enum {
    ATA_DEVICE_NONE = 0,
    ATA_DEVICE_DISK,
    ATA_DEVICE_ATAPI
} ata_device_type_t;

typedef struct {
    uint16_t io;
    uint16_t ctrl;
    uint8_t slave;
    ata_device_type_t type;
    uint32_t boot_image_lba;
    uint32_t cache_lba;
    uint8_t cache_valid;
    uint8_t cache[ATAPI_SECTOR_SIZE];
} ata_device_t;

static ata_device_t ata_active;

static inline void outb_port(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb_port(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw_port(uint16_t port, uint16_t val) {
    asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw_port(uint16_t port) {
    uint16_t ret;
    asm volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void ata_io_delay(const ata_device_t* dev) {
    (void)inb_port(dev->ctrl);
    (void)inb_port(dev->ctrl);
    (void)inb_port(dev->ctrl);
    (void)inb_port(dev->ctrl);
}

static void ata_select(const ata_device_t* dev, uint8_t head) {
    outb_port(dev->io + 6U, (uint8_t)(0xA0U | (dev->slave ? 0x10U : 0U) | head));
    ata_io_delay(dev);
}

static int ata_wait_clear_bsy(const ata_device_t* dev) {
    for (uint32_t i = 0; i < ATA_WAIT_LIMIT; i++) {
        if ((inb_port(dev->io + 7U) & ATA_SR_BSY) == 0U) return 0;
    }
    return -1;
}

static int ata_wait_drq(const ata_device_t* dev) {
    for (uint32_t i = 0; i < ATA_WAIT_LIMIT; i++) {
        uint8_t status = inb_port(dev->io + 7U);
        if ((status & ATA_SR_ERR) != 0U) return -1;
        if ((status & ATA_SR_BSY) == 0U && (status & ATA_SR_DRQ) != 0U) return 0;
    }
    return -1;
}

static void ata_read_words(const ata_device_t* dev, void* buffer, uint32_t words) {
    uint8_t* out = (uint8_t*)buffer;

    for (uint32_t i = 0; i < words; i++) {
        uint16_t w = inw_port(dev->io);
        out[i * 2U] = (uint8_t)(w & 0xFFU);
        out[i * 2U + 1U] = (uint8_t)(w >> 8);
    }
}

static void ata_write_words(const ata_device_t* dev, const void* buffer, uint32_t words) {
    const uint8_t* in = (const uint8_t*)buffer;

    for (uint32_t i = 0; i < words; i++) {
        uint16_t w = (uint16_t)in[i * 2U] | ((uint16_t)in[i * 2U + 1U] << 8);
        outw_port(dev->io, w);
    }
}

static int ata_identify_packet(ata_device_t* dev) {
    uint16_t words[256];

    outb_port(dev->io + 7U, ATA_CMD_IDENTIFY_PACKET);
    if (ata_wait_clear_bsy(dev) != 0 || ata_wait_drq(dev) != 0) return -1;
    ata_read_words(dev, words, 256U);
    dev->type = ATA_DEVICE_ATAPI;
    return 0;
}

static int ata_probe_device(uint16_t io, uint16_t ctrl, uint8_t slave, ata_device_t* out_dev) {
    ata_device_t dev;
    uint8_t status;
    uint8_t cl;
    uint8_t ch;
    uint16_t words[256];

    memset(&dev, 0, sizeof(dev));
    dev.io = io;
    dev.ctrl = ctrl;
    dev.slave = slave;
    dev.cache_lba = 0xFFFFFFFFU;

    ata_select(&dev, 0U);
    status = inb_port(io + 7U);
    if (status == 0xFFU || status == 0x00U) return -1;

    outb_port(io + 2U, 0U);
    outb_port(io + 3U, 0U);
    outb_port(io + 4U, 0U);
    outb_port(io + 5U, 0U);
    outb_port(io + 7U, ATA_CMD_IDENTIFY);
    if (ata_wait_clear_bsy(&dev) != 0) return -1;

    status = inb_port(io + 7U);
    if (status == 0U) return -1;

    cl = inb_port(io + 4U);
    ch = inb_port(io + 5U);
    if (cl == 0x14U && ch == 0xEBU) {
        if (ata_identify_packet(&dev) != 0) return -1;
    } else {
        if (ata_wait_drq(&dev) != 0) return -1;
        ata_read_words(&dev, words, 256U);
        dev.type = ATA_DEVICE_DISK;
    }

    *out_dev = dev;
    return 0;
}

static int atapi_read_cd_sector(uint32_t cd_lba, uint8_t* buffer) {
    uint8_t packet[12];
    ata_device_t* dev = &ata_active;
    uint32_t copied = 0U;

    if (!buffer || dev->type != ATA_DEVICE_ATAPI) return -1;
    memset(buffer, 0, ATAPI_SECTOR_SIZE);
    ata_select(dev, 0U);
    if (ata_wait_clear_bsy(dev) != 0) return -1;
    outb_port(dev->io + 1U, 0U);
    outb_port(dev->io + 4U, (uint8_t)(ATAPI_SECTOR_SIZE & 0xFFU));
    outb_port(dev->io + 5U, (uint8_t)(ATAPI_SECTOR_SIZE >> 8));
    outb_port(dev->io + 7U, ATA_CMD_PACKET);
    if (ata_wait_drq(dev) != 0) return -1;

    memset(packet, 0, sizeof(packet));
    packet[0] = ATAPI_CMD_READ_12;
    packet[2] = (uint8_t)(cd_lba >> 24);
    packet[3] = (uint8_t)(cd_lba >> 16);
    packet[4] = (uint8_t)(cd_lba >> 8);
    packet[5] = (uint8_t)cd_lba;
    packet[9] = 1U;
    ata_write_words(dev, packet, 6U);

    while (copied < ATAPI_SECTOR_SIZE) {
        uint8_t status;
        uint32_t byte_count;
        uint32_t remaining;

        if (ata_wait_clear_bsy(dev) != 0) return -1;
        status = inb_port(dev->io + 7U);
        if ((status & ATA_SR_ERR) != 0U) return -1;
        if ((status & ATA_SR_DRQ) == 0U) return -1;

        byte_count = (uint32_t)inb_port(dev->io + 4U) |
                     ((uint32_t)inb_port(dev->io + 5U) << 8);
        remaining = ATAPI_SECTOR_SIZE - copied;
        if (byte_count == 0U || byte_count > remaining) byte_count = remaining;
        if ((byte_count & 1U) != 0U) byte_count++;

        ata_read_words(dev, buffer + copied, byte_count / 2U);
        copied += byte_count;
    }
    (void)ata_wait_clear_bsy(dev);
    return 0;
}

static uint32_t read_le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void atapi_probe_boot_image(void) {
    uint8_t sector[ATAPI_SECTOR_SIZE];
    uint32_t catalog_lba;

    ata_active.boot_image_lba = 0U;
    if (atapi_read_cd_sector(ATAPI_BOOT_RECORD_LBA, sector) != 0) return;
    if (sector[0] != 0U || memcmp(sector + 1U, "CD001", 5U) != 0) return;
    if (memcmp(sector + 7U, "EL TORITO SPECIFICATION", 23U) != 0) return;
    catalog_lba = read_le32(sector + ATAPI_BOOT_CATALOG_OFF);
    if (catalog_lba == 0U) return;
    if (atapi_read_cd_sector(catalog_lba, sector) != 0) return;
    if (sector[0x20] != 0x88U) return;
    ata_active.boot_image_lba = read_le32(sector + 0x28U);
}

int ata_init(void) {
    static const uint16_t channels[][2] = {
        {0x1F0U, 0x3F6U},
        {0x170U, 0x376U}
    };
    ata_device_t first_disk;
    ata_device_t first_atapi;
    int have_disk = 0;
    int have_atapi = 0;

    memset(&ata_active, 0, sizeof(ata_active));
    memset(&first_disk, 0, sizeof(first_disk));
    memset(&first_atapi, 0, sizeof(first_atapi));
    for (uint32_t c = 0; c < sizeof(channels) / sizeof(channels[0]); c++) {
        for (uint8_t slave = 0; slave <= 1U; slave++) {
            ata_device_t dev;
            if (ata_probe_device(channels[c][0], channels[c][1], slave, &dev) != 0) continue;
            if (dev.type == ATA_DEVICE_ATAPI) {
                ata_active = dev;
                atapi_probe_boot_image();
                dev = ata_active;
                serial_write("[storage] atapi cdrom boot_lba=");
                serial_write_hex32(dev.boot_image_lba);
                serial_write_char('\n');
                if (dev.boot_image_lba != 0U) {
                    ata_active = dev;
                    return 0;
                }
                if (!have_atapi) {
                    first_atapi = dev;
                    have_atapi = 1;
                }
            } else {
                serial_write_line("[storage] ata disk candidate");
                if (!have_disk) {
                    first_disk = dev;
                    have_disk = 1;
                }
            }
        }
    }

    if (have_disk) {
        ata_active = first_disk;
        serial_write_line("[storage] ata disk online");
        return 0;
    }
    if (have_atapi) {
        ata_active = first_atapi;
        serial_write_line("[storage] atapi cdrom online without el-torito boot image");
        return 0;
    }
    return -1;
}

int ata_read_sector(uint32_t lba, uint8_t* buffer) {
    ata_device_t* dev = &ata_active;

    if (!buffer || dev->type == ATA_DEVICE_NONE) return -1;
    if (dev->type == ATA_DEVICE_ATAPI) {
        uint32_t cd_lba = dev->boot_image_lba + (lba / 4U);
        uint32_t offset = (lba % 4U) * 512U;
        if (dev->boot_image_lba == 0U) return -1;
        if (dev->cache_valid == 0U || dev->cache_lba != cd_lba) {
            if (atapi_read_cd_sector(cd_lba, dev->cache) != 0) return -1;
            dev->cache_lba = cd_lba;
            dev->cache_valid = 1U;
        }
        memcpy(buffer, dev->cache + offset, 512U);
        return 0;
    }

    ata_select(dev, (uint8_t)(0xE0U | ((lba >> 24) & 0x0FU)));
    if (ata_wait_clear_bsy(dev) != 0) return -1;
    outb_port(dev->io + 2U, 1U);
    outb_port(dev->io + 3U, (uint8_t)lba);
    outb_port(dev->io + 4U, (uint8_t)(lba >> 8));
    outb_port(dev->io + 5U, (uint8_t)(lba >> 16));
    outb_port(dev->io + 7U, ATA_CMD_READ_SECTORS);
    if (ata_wait_clear_bsy(dev) != 0 || ata_wait_drq(dev) != 0) return -1;
    ata_read_words(dev, buffer, 256U);
    return 0;
}

int ata_write_sector(uint32_t lba, const uint8_t* buffer) {
    ata_device_t* dev = &ata_active;

    if (!buffer || dev->type != ATA_DEVICE_DISK) return -1;
    ata_select(dev, (uint8_t)(0xE0U | ((lba >> 24) & 0x0FU)));
    if (ata_wait_clear_bsy(dev) != 0) return -1;
    outb_port(dev->io + 2U, 1U);
    outb_port(dev->io + 3U, (uint8_t)lba);
    outb_port(dev->io + 4U, (uint8_t)(lba >> 8));
    outb_port(dev->io + 5U, (uint8_t)(lba >> 16));
    outb_port(dev->io + 7U, ATA_CMD_WRITE_SECTORS);
    if (ata_wait_clear_bsy(dev) != 0 || ata_wait_drq(dev) != 0) return -1;
    ata_write_words(dev, buffer, 256U);
    outb_port(dev->io + 7U, ATA_CMD_CACHE_FLUSH);
    return ata_wait_clear_bsy(dev);
}
