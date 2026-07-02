#include "fx/sh.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>

using namespace fx;
namespace fs = std::filesystem;

static std::vector<uint8_t> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto sz = f.tellg(); f.seekg(0);
    std::vector<uint8_t> buf((size_t)sz);
    f.read((char*)buf.data(), sz);
    return buf;
}

static bool write_text(const char* path, const std::string& s) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(s.data(), (std::streamsize)s.size());
    return f.good();
}

// fx sh info <file.SH>
static int cmd_info(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "Usage: fx sh info <file.SH>\n"); return 1; }
    auto data = read_file(argv[1]);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", argv[1]); return 1; }

    auto info = sh_parse_info(data.data(), data.size());
    printf("File:       %s\n", argv[1]);
    printf("Scale:      %d (%.1f ft/unit)\n", info.scale_raw, info.scale);
    printf("Vertices:   %d\n", info.vert_count);
    printf("Faces:      %d\n", info.face_count);
    if (info.vert_count > 0) {
        printf("Bounds X:   %.1f .. %.1f ft\n", info.bbox[0], info.bbox[3]);
        printf("Bounds Y:   %.1f .. %.1f ft\n", info.bbox[1], info.bbox[4]);
        printf("Bounds Z:   %.1f .. %.1f ft\n", info.bbox[2], info.bbox[5]);
    }
    if (!info.textures.empty()) {
        printf("Textures:  ");
        for (auto& t : info.textures) printf(" %s", t.c_str());
        printf("\n");
    }
    return 0;
}

// fx sh unpack <file.SH> [-o out.obj]
static int cmd_unpack(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "Usage: fx sh unpack <file.SH> [-o out.obj]\n"); return 1; }
    const char* src = argv[1];
    const char* dst = nullptr;
    for (int i = 2; i < argc - 1; ++i)
        if (strcmp(argv[i], "-o") == 0) dst = argv[i + 1];

    auto data = read_file(src);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", src); return 1; }

    auto mesh = sh_parse_mesh(data.data(), data.size());
    if (mesh.vertices.empty()) {
        fprintf(stderr, "No geometry found in %s\n", src);
        return 1;
    }

    auto obj = sh_to_obj(mesh);
    std::string out_path = dst ? dst : (fs::path(src).string() + ".obj");
    if (!write_text(out_path.c_str(), obj)) {
        fprintf(stderr, "Cannot write: %s\n", out_path.c_str());
        return 1;
    }
    printf("%s -> %s  (%d verts, %d faces)\n",
           src, out_path.c_str(), (int)mesh.vertices.size(), (int)mesh.faces.size());
    return 0;
}

int cmd_sh(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: fx sh <info|unpack> ...\n");
        return 1;
    }
    const char* sub = argv[1];
    if (strcmp(sub, "info")   == 0) return cmd_info(argc - 1, argv + 1);
    if (strcmp(sub, "unpack") == 0) return cmd_unpack(argc - 1, argv + 1);
    fprintf(stderr, "Unknown sh subcommand: %s\n", sub);
    return 1;
}
