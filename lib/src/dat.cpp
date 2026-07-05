#include "fx/dat.h"
#include <cstring>

namespace fx {

static uint32_t r32(const uint8_t* p) {
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) |
                      ((uint32_t)p[3] << 24));
}
static void w32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v;         p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

bool dat_read(const uint8_t* f, size_t size, CnInfo& c) {
    if (size != DAT_FILE_SIZE) return false;
    c.checksum = r32(f);
    const uint8_t* d = f + 4;  // CN_INFO base
    c.version = r32(d + 0x000);
    memcpy(c.callsign, d + 0x004, 80);
    c.transport = r32(d + 0x054);
    memcpy(c.unk_058, d + 0x058, 8);
    c.baud_index = r32(d + 0x060);
    c.serial_com = r32(d + 0x064);
    memcpy(c.phone_or_mode, d + 0x068, 84);
    c.modem_com = r32(d + 0x0BC);
    for (int i = 0; i < 8; i++) {
        memcpy(c.phone_names[i],   d + 0x0C0 + i * 80, 80);
        memcpy(c.phone_numbers[i], d + 0x340 + i * 80, 80);
    }
    memcpy(c.pad_5c0, d + 0x5C0, 0x324);
    memcpy(c.ip_hex,  d + 0x8E4, 8);
    memcpy(c.mac_hex, d + 0x8EC, 13);
    memcpy(c.unk_8f9, d + 0x8F9, 0x4B3);
    c.appio_ptr = r32(d + 0xDAC);
    memcpy(c.tcp_block, d + 0xDB0, 0x16);
    memcpy(c.ipx_local, d + 0xDC6, 10);
    c.remote_a = r32(d + 0xDD0);
    memcpy(c.remote_b, d + 0xDD4, 6);
    c.remote_valid = d[0xDDA];
    c.pad_ddb      = d[0xDDB];
    return true;
}

std::vector<uint8_t> dat_write(const CnInfo& c) {
    std::vector<uint8_t> out(DAT_FILE_SIZE, 0);
    uint8_t* f = out.data();
    w32(f, c.checksum);
    uint8_t* d = f + 4;
    w32(d + 0x000, c.version);
    memcpy(d + 0x004, c.callsign, 80);
    w32(d + 0x054, c.transport);
    memcpy(d + 0x058, c.unk_058, 8);
    w32(d + 0x060, c.baud_index);
    w32(d + 0x064, c.serial_com);
    memcpy(d + 0x068, c.phone_or_mode, 84);
    w32(d + 0x0BC, c.modem_com);
    for (int i = 0; i < 8; i++) {
        memcpy(d + 0x0C0 + i * 80, c.phone_names[i],   80);
        memcpy(d + 0x340 + i * 80, c.phone_numbers[i], 80);
    }
    memcpy(d + 0x5C0, c.pad_5c0, 0x324);
    memcpy(d + 0x8E4, c.ip_hex,  8);
    memcpy(d + 0x8EC, c.mac_hex, 13);
    memcpy(d + 0x8F9, c.unk_8f9, 0x4B3);
    w32(d + 0xDAC, c.appio_ptr);
    memcpy(d + 0xDB0, c.tcp_block, 0x16);
    memcpy(d + 0xDC6, c.ipx_local, 10);
    w32(d + 0xDD0, c.remote_a);
    memcpy(d + 0xDD4, c.remote_b, 6);
    d[0xDDA] = c.remote_valid;
    d[0xDDB] = c.pad_ddb;
    return out;
}

const char* dat_transport_name(uint32_t t) {
    switch (t) {
        case 2: return "modem";
        case 3: return "serial";
        case 4: return "TCP/IP";
        default: return "IPX/NetBEUI (protocol index)";
    }
}

unsigned dat_baud_rate(uint32_t idx) {
    switch (idx) {
        case 7:  return 9600;
        case 8:  return 19200;
        case 9:  return 38400;
        case 10: return 57600;
        case 11: return 57600;
        case 12: return 28800;
        case 13: return 115200;
        default: return 0;
    }
}

} // namespace fx
