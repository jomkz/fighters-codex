#include "fx/dat.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static void usage_dat() {
    puts("Usage:");
    puts("  fx dat info <NET.DAT|MODEM.DAT|SERIAL.DAT>   # dump the CN_INFO struct");
}

static std::vector<uint8_t> read_all(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return {};
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    std::vector<uint8_t> buf((size_t)sz);
    if (sz > 0 && fread(buf.data(), 1, (size_t)sz, f) != (size_t)sz) buf.clear();
    fclose(f);
    return buf;
}

static std::string field_str(const uint8_t* p, size_t n) {
    size_t len = 0;
    while (len < n && p[len]) len++;
    std::string s;
    for (size_t i = 0; i < len; i++)
        s += (p[i] >= 0x20 && p[i] < 0x7F) ? (char)p[i] : '.';
    return s;
}

static int cmd_dat_info(const char* path) {
    auto data = read_all(path);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", path); return 1; }

    fx::CnInfo c;
    if (!fx::dat_read(data.data(), data.size(), c)) {
        fprintf(stderr, "Not a valid CN_INFO config (need 3552 bytes): %s (%zu bytes)\n",
                path, data.size());
        return 1;
    }

    printf("CN_INFO v%u (checksum 0x%08X, 3552 bytes)\n", c.version, c.checksum);
    printf("Callsign: \"%s\"\n", field_str(c.callsign, 80).c_str());
    printf("Transport: %u (%s)\n", c.transport, fx::dat_transport_name(c.transport));
    unsigned baud = fx::dat_baud_rate(c.baud_index);
    printf("Serial: COM%u, baud index %u", c.serial_com + 1, c.baud_index);
    if (baud) printf(" (%u)", baud);
    printf("\n");
    printf("Modem: COM index %u%s, dial/mode \"%s\"\n", c.modem_com,
           c.modem_com == 8 ? " (autodetect)" : "",
           field_str(c.phone_or_mode, 84).c_str());
    int used = 0;
    for (int i = 0; i < 8; i++)
        if (c.phone_names[i][0] || c.phone_numbers[i][0]) used++;
    printf("Phone book: %d of 8 slots in use\n", used);
    printf("TCP/IP: ip-hex \"%s\"  mac-hex \"%s\"  remote-valid=%u\n",
           field_str(c.ip_hex, 8).c_str(), field_str(c.mac_hex, 13).c_str(),
           c.remote_valid);

    auto out = fx::dat_write(c);
    printf("Round-trip: %s\n",
           (out.size() == data.size() &&
            memcmp(out.data(), data.data(), data.size()) == 0)
               ? "byte-identical" : "MISMATCH (report this)");
    return 0;
}

int cmd_dat(int argc, char** argv) {
    if (argc < 3) { usage_dat(); return 1; }
    if (strcmp(argv[1], "info") == 0) return cmd_dat_info(argv[2]);
    fprintf(stderr, "Unknown subcommand: %s\n", argv[1]);
    usage_dat();
    return 1;
}
