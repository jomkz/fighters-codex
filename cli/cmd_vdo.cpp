#include "fx/vdo.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// stb declarations only — implementation lives in pic.cpp / cmd_pic.cpp.
#include "stb_image_write.h"

static void usage_vdo() {
    puts("Usage:");
    puts("  fx vdo info   <file.VDO> [file.FBC]        # header (+ frame count with FBC)");
    puts("  fx vdo export <file.VDO> <file.FBC> [-o dir]  # decode every frame to PNG");
}

static std::vector<uint8_t> read_all(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return {};
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz <= 0) { fclose(f); return {}; }
    std::vector<uint8_t> buf((size_t)sz);
    if (fread(buf.data(), 1, (size_t)sz, f) != (size_t)sz) buf.clear();
    fclose(f);
    return buf;
}

static int cmd_vdo_info(int argc, char** argv) {
    auto vdo = read_all(argv[0]);
    if (vdo.empty()) { fprintf(stderr, "Cannot read: %s\n", argv[0]); return 1; }
    fx::VdoInfo info;
    if (!fx::vdo_info(vdo.data(), vdo.size(), &info)) {
        fprintf(stderr, "Not a RATVID .VDO: %s\n", argv[0]);
        return 1;
    }
    printf("File: %s (%zu bytes)\n", argv[0], vdo.size());
    printf("Resolution: %ux%u  fps: %u  audio: %u Hz (paired .11K)\n",
           info.width, info.height, info.fps, info.audio_hz);
    if (argc >= 2) {
        auto fbc = read_all(argv[1]);
        fx::VdoDecoder* d =
            fx::vdo_open(vdo.data(), vdo.size(), fbc.data(), fbc.size());
        if (!d) {
            fprintf(stderr, "FBC does not match this VDO: %s\n", argv[1]);
            return 1;
        }
        printf("Frames: %u (from %s)\n", fx::vdo_frame_count(d), argv[1]);
        fx::vdo_close(d);
    } else {
        printf("Frames: (supply the paired .FBC to count)\n");
    }
    return 0;
}

static int cmd_vdo_export(int argc, char** argv) {
    if (argc < 2) { usage_vdo(); return 1; }
    const char* out_dir = ".";
    for (int i = 2; i + 1 < argc; i++)
        if (strcmp(argv[i], "-o") == 0) out_dir = argv[i + 1];

    auto vdo = read_all(argv[0]);
    auto fbc = read_all(argv[1]);
    if (vdo.empty()) { fprintf(stderr, "Cannot read: %s\n", argv[0]); return 1; }
    fx::VdoDecoder* d =
        fx::vdo_open(vdo.data(), vdo.size(), fbc.data(), fbc.size());
    if (!d) { fprintf(stderr, "Invalid VDO or mismatched FBC\n"); return 1; }

    uint32_t n = fx::vdo_frame_count(d);
    int w = (int)fx::vdo_width(d), h = (int)fx::vdo_height(d);
    uint32_t written = 0;
    for (uint32_t f = 0; f < n; f++) {
        auto rgba = fx::vdo_decode_frame_rgba(d, f);
        if (rgba.empty()) { fprintf(stderr, "Failed to decode frame %u\n", f); break; }
        char path[1024];
        snprintf(path, sizeof(path), "%s/frame%04u.png", out_dir, f);
        if (stbi_write_png(path, w, h, 4, rgba.data(), w * 4)) written++;
        else fprintf(stderr, "Failed to write %s\n", path);
    }
    printf("Wrote %u/%u frames to %s\n", written, n, out_dir);
    fx::vdo_close(d);
    return (written == n) ? 0 : 1;
}

int cmd_vdo(int argc, char** argv) {
    if (argc < 3) { usage_vdo(); return 1; }
    if (strcmp(argv[1], "info") == 0)   return cmd_vdo_info(argc - 2, argv + 2);
    if (strcmp(argv[1], "export") == 0) return cmd_vdo_export(argc - 2, argv + 2);
    fprintf(stderr, "Unknown subcommand: %s\n", argv[1]);
    usage_vdo();
    return 1;
}
