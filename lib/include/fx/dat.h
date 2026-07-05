#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

// DAT — multiplayer network configuration (see DAT.md). NET.DAT, MODEM.DAT,
// and SERIAL.DAT share one 3,552-byte layout: a 4-byte checksum followed by
// the 3,548-byte CN_INFO struct (v3), read/written by CN_ReadConfig /
// CN_WriteConfig (0x47f7a0 / 0x47f930). Documented fields are typed below;
// the stored checksum, the undocumented gap after the transport dword, and
// the ~1,203-byte unmapped region (gap #54) pass through verbatim, so
// dat_write is the byte-identical inverse of dat_read.

namespace fx {

constexpr size_t DAT_FILE_SIZE = 3552;  // 0xDE0: 4-byte checksum + 0xDDC CN_INFO

struct CnInfo {
    uint32_t checksum;              // file +0x000 — stored CfigChecksum, pass-through
    // CN_INFO offsets below (file offset − 4):
    uint32_t version;               // [0x000] — 3 = current
    uint8_t  callsign[80];          // [0x004] — Janes.net online name
    uint32_t transport;             // [0x054] — 2 modem · 3 serial · 4 TCP/IP · other IPX/NetBEUI
    uint8_t  unk_058[8];            // [0x058] — undocumented, pass-through
    uint32_t baud_index;            // [0x060] — 7=9600 … 13=115200 (SER_Initialize4)
    uint32_t serial_com;            // [0x064] — 0–3 = COM1–COM4
    uint8_t  phone_or_mode[84];     // [0x068] — dial number, or "LISTEN"
    uint32_t modem_com;             // [0x0BC] — 0–7 = COM1–COM8, 8 = autodetect
    uint8_t  phone_names[8][80];    // [0x0C0] — phone book names (MODEM.DAT)
    uint8_t  phone_numbers[8][80];  // [0x340] — phone book numbers (MODEM.DAT)
    uint8_t  pad_5c0[0x324];        // [0x5C0] — confirmed-unreachable padding
    uint8_t  ip_hex[8];             // [0x8E4] — IP as 8 ASCII hex chars
    uint8_t  mac_hex[13];           // [0x8EC] — MAC/IPX node as 12 hex chars + NUL
    uint8_t  unk_8f9[0x4B3];        // [0x8F9] — unmapped region (#54), pass-through
    uint32_t appio_ptr;             // [0xDAC] — runtime callback pointer as saved
    uint8_t  tcp_block[0x16];       // [0xDB0] — CN_INFO_TCP sub-block
    uint8_t  ipx_local[10];         // [0xDC6] — local IPX net(4) + node(6)
    uint32_t remote_a;              // [0xDD0] — IP binary / IPX direct net
    uint8_t  remote_b[6];           // [0xDD4] — MAC / IPX direct node
    uint8_t  remote_valid;          // [0xDDA]
    uint8_t  pad_ddb;               // [0xDDB] — end of v3 struct
};

// Parse a v3 config file. false unless size == 3552.
bool dat_read(const uint8_t* data, size_t size, CnInfo& out);

// Serialize — exactly 3552 bytes; byte-identical inverse of dat_read.
std::vector<uint8_t> dat_write(const CnInfo& info);

// Human-readable names ("?" / nullptr-free for any input).
const char* dat_transport_name(uint32_t transport);
unsigned    dat_baud_rate(uint32_t baud_index);  // 0 if unknown index

} // namespace fx
