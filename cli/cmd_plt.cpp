#include "ft/plt.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

using namespace ft;

static std::vector<uint8_t> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto sz = f.tellg(); f.seekg(0);
    std::vector<uint8_t> buf((size_t)sz);
    f.read((char*)buf.data(), sz);
    return buf;
}

// plt info <file.P>
static int cmd_info(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "Usage: ft plt info <file.P>\n"); return 1; }
    auto data = read_file(argv[1]);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", argv[1]); return 1; }

    PltInfo info;
    if (!plt_parse(data.data(), data.size(), &info)) {
        fprintf(stderr, "Not a valid pilot file: %s\n", argv[1]);
        return 1;
    }

    printf("File:       %s  (%zu bytes)\n", argv[1], data.size());
    printf("Name:       %s\n", info.name.c_str());
    printf("Callsign:   %s\n", info.callsign.c_str());
    printf("Rank:       %s\n", info.rank.c_str());
    if (!info.voice_file.empty())  printf("Voice:      %s\n", info.voice_file.c_str());
    if (!info.nose_art.empty())    printf("Nose art:   %s\n", info.nose_art.c_str());
    if (!info.left_decal.empty())  printf("Left decal: %s\n", info.left_decal.c_str());
    if (!info.right_decal.empty()) printf("Right decal:%s\n", info.right_decal.c_str());
    if (!info.portrait.empty())    printf("Portrait:   %s\n", info.portrait.c_str());

    if (!info.cam_file.empty()) {
        printf("\nCampaign:   %s", info.cam_file.c_str());
        if (!info.cam_name.empty()) printf("  (%s)", info.cam_name.c_str());
        printf("\n");
        if (!info.aircraft.empty())
            printf("Aircraft:   %s\n", info.aircraft.c_str());
        if (!info.aircraft_pool.empty()) {
            printf("Pool:       ");
            for (size_t i = 0; i < info.aircraft_pool.size(); i++) {
                if (i) printf(", ");
                printf("%s", info.aircraft_pool[i].c_str());
            }
            printf("\n");
        }
        if (!info.ordnance.empty()) {
            printf("Ordnance:\n");
            for (auto& o : info.ordnance)
                printf("  %-16s x%u\n", o.jt_name.c_str(), o.quantity);
        }
        if (!info.sensors.empty()) {
            printf("Sensors:    ");
            for (size_t i = 0; i < info.sensors.size(); i++) {
                if (i) printf(", ");
                printf("%s", info.sensors[i].c_str());
            }
            printf("\n");
        }
    } else {
        printf("\n(No active campaign)\n");
    }

    return 0;
}

int cmd_plt(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: ft plt <info> ...\n");
        return 1;
    }
    if (strcmp(argv[1], "info") == 0) return cmd_info(argc - 1, argv + 1);
    fprintf(stderr, "Unknown plt subcommand: %s\n", argv[1]);
    return 1;
}
