#include "fx/cb8.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

// stb declarations only — implementations live in pic.cpp / cmd_pic.cpp.
#include "stb_image.h"
#include "stb_image_write.h"

namespace fs = std::filesystem;

static std::vector<uint8_t> read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return {};
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) { fclose(f); return {}; }
    std::vector<uint8_t> buf((size_t)len);
    if (fread(buf.data(), 1, (size_t)len, f) != (size_t)len) { fclose(f); return {}; }
    fclose(f);
    return buf;
}

// PGM (P5) — palette index bytes written as 8-bit greyscale.
static bool write_pgm(const char* path, const uint8_t* pixels, uint32_t w, uint32_t h) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    fprintf(f, "P5\n%u %u\n255\n", w, h);
    bool ok = fwrite(pixels, 1, (size_t)w * h, f) == (size_t)w * h;
    fclose(f);
    return ok;
}

// cb8 unpack <file.CB8> [-o output_dir] — decode every frame to colour PNG
// through its embedded palette (#95).
static int cmd_unpack(int argc, char** argv) {
    const char* outdir = ".";
    const char* input  = nullptr;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) outdir = argv[++i];
        else input = argv[i];
    }
    if (!input) {
        fprintf(stderr, "Usage: fx cb8 unpack <file.CB8> [-o output_dir]\n");
        return 1;
    }
    auto data = read_file(input);
    if (data.empty()) { fprintf(stderr, "Cannot read %s\n", input); return 1; }
    fs::create_directories(outdir);

    fx::Cb8Info info;
    if (!fx::cb8_info(data.data(), data.size(), &info)) {
        fprintf(stderr, "Not a valid CB8 file\n");
        return 1;
    }
    fx::Cb8Decoder* dec = fx::cb8_open(data.data(), data.size());
    if (!dec) { fprintf(stderr, "Failed to open CB8 decoder\n"); return 1; }

    uint32_t written = 0;
    for (uint32_t f = 0; f < info.frame_count; f++) {
        auto rgba = fx::cb8_decode_frame_rgba(dec, f);
        if (rgba.empty()) { fprintf(stderr, "Failed to decode frame %u\n", f); continue; }
        char name[32];
        snprintf(name, sizeof(name), "frame%04u.png", f);
        auto path = (fs::path(outdir) / name).string();
        if (stbi_write_png(path.c_str(), (int)info.width, (int)info.height, 4,
                           rgba.data(), (int)info.width * 4))
            written++;
        else
            fprintf(stderr, "Failed to write %s\n", path.c_str());
    }
    fx::cb8_close(dec);
    printf("Wrote %u/%u frames to %s\n", written, info.frame_count, outdir);
    return (written == info.frame_count) ? 0 : 1;
}

// cb8 repack <orig.CB8> <png_dir> [-o out.CB8] — rebuild the movie around
// edited frames: audio and timing carry over from the original; each PNG
// must use at most 256 distinct colours (the palette is rebuilt per frame).
static int cmd_repack(int argc, char** argv) {
    const char* out_path = nullptr;
    std::vector<const char*> pos;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) out_path = argv[++i];
        else pos.push_back(argv[i]);
    }
    if (pos.size() != 2) {
        fprintf(stderr, "Usage: fx cb8 repack <orig.CB8> <png_dir> [-o out.CB8]\n");
        return 1;
    }
    auto orig = read_file(pos[0]);
    if (orig.empty()) { fprintf(stderr, "Cannot read %s\n", pos[0]); return 1; }
    fx::Cb8Info info;
    if (!fx::cb8_info(orig.data(), orig.size(), &info)) {
        fprintf(stderr, "Not a valid CB8 file\n");
        return 1;
    }

    std::vector<std::string> pngs;
    for (const auto& de : fs::directory_iterator(pos[1]))
        if (de.is_regular_file() && de.path().extension() == ".png")
            pngs.push_back(de.path().string());
    std::sort(pngs.begin(), pngs.end());
    if (pngs.size() != info.frame_count) {
        fprintf(stderr, "Frame count mismatch: %zu PNGs vs %u frames in %s\n",
                pngs.size(), info.frame_count, pos[0]);
        return 1;
    }

    std::vector<fx::Cb8Frame> frames(pngs.size());
    for (size_t f = 0; f < pngs.size(); f++) {
        int w = 0, h = 0, ch = 0;
        uint8_t* rgba = stbi_load(pngs[f].c_str(), &w, &h, &ch, 4);
        if (!rgba || (uint32_t)w != info.width || (uint32_t)h != info.height) {
            fprintf(stderr, "Bad frame %s (need %ux%u)\n", pngs[f].c_str(),
                    info.width, info.height);
            if (rgba) stbi_image_free(rgba);
            return 1;
        }
        // Exact-colour palette: every distinct RGB becomes one entry.
        std::map<uint32_t, uint8_t> colors;
        auto& fr = frames[f];
        fr.indices.resize((size_t)w * h);
        for (int i = 0; i < w * h; i++) {
            const uint32_t key = (uint32_t)rgba[i*4] << 16 |
                                 (uint32_t)rgba[i*4+1] << 8 | rgba[i*4+2];
            auto it = colors.find(key);
            if (it == colors.end()) {
                if (colors.size() >= 256) {
                    fprintf(stderr, "%s: more than 256 distinct colours\n", pngs[f].c_str());
                    stbi_image_free(rgba);
                    return 1;
                }
                const uint8_t id = (uint8_t)colors.size();
                it = colors.emplace(key, id).first;
                fr.palette6[id*3+0] = (uint8_t)(rgba[i*4]   >> 2);
                fr.palette6[id*3+1] = (uint8_t)(rgba[i*4+1] >> 2);
                fr.palette6[id*3+2] = (uint8_t)(rgba[i*4+2] >> 2);
            }
            fr.indices[(size_t)i] = it->second;
        }
        stbi_image_free(rgba);
    }

    auto out = fx::cb8_repack(orig.data(), orig.size(), frames);
    if (out.empty()) {
        fprintf(stderr, "Repack failed (frame exceeds codebook capacity)\n");
        return 1;
    }
    std::string dst = out_path ? out_path : (fs::path(pos[0]).stem().string() + ".repack.CB8");
    std::ofstream fo(dst, std::ios::binary);
    if (!fo || !fo.write((const char*)out.data(), (std::streamsize)out.size())) {
        fprintf(stderr, "Cannot write %s\n", dst.c_str());
        return 1;
    }
    fo.close();
    printf("%s + %zu frames -> %s (%zu bytes)\n", pos[0], pngs.size(), dst.c_str(), out.size());
    return 0;
}

static int cmd_info(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: fx cb8 info <file.CB8>\n");
        return 1;
    }
    auto data = read_file(argv[1]);
    if (data.empty()) { fprintf(stderr, "Cannot read %s\n", argv[1]); return 1; }

    fx::Cb8Info info;
    if (!fx::cb8_info(data.data(), data.size(), &info)) {
        fprintf(stderr, "Not a valid CB8 file\n");
        return 1;
    }
    double fps = (info.audio_sync_rate > 0 && info.samples_per_frame > 0)
                     ? (double)info.audio_sync_rate / info.samples_per_frame
                     : 15.0;
    double dur = fps > 0 ? info.frame_count / fps : 0.0;
    printf("video: %u x %u, %u frames, %.1f fps, %.2f s\n",
           info.width, info.height, info.frame_count, fps, dur);
    printf("audio: 11025 Hz PCM, %u sync ticks/frame\n", info.samples_per_frame);
    return 0;
}

static int cmd_frames(int argc, char** argv) {
    const char* outdir = ".";
    const char* input  = nullptr;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            outdir = argv[++i];
        else
            input = argv[i];
    }
    if (!input) {
        fprintf(stderr, "Usage: fx cb8 frames <file.CB8> [-o output_dir]\n");
        return 1;
    }

    auto data = read_file(input);
    if (data.empty()) { fprintf(stderr, "Cannot read %s\n", input); return 1; }

    fs::create_directories(outdir);

    fx::Cb8Info info;
    if (!fx::cb8_info(data.data(), data.size(), &info)) {
        fprintf(stderr, "Not a valid CB8 file\n");
        return 1;
    }

    fx::Cb8Decoder* dec = fx::cb8_open(data.data(), data.size());
    if (!dec) { fprintf(stderr, "Failed to open CB8 decoder\n"); return 1; }

    uint32_t written = 0;
    for (uint32_t f = 0; f < info.frame_count; f++) {
        auto frame = fx::cb8_decode_frame(dec, f);
        if (frame.empty()) {
            fprintf(stderr, "Failed to decode frame %u\n", f);
            continue;
        }
        char name[32];
        snprintf(name, sizeof(name), "frame%04u.pgm", f);
        auto path = (fs::path(outdir) / name).string();
        if (write_pgm(path.c_str(), frame.data(), info.width, info.height))
            written++;
        else
            fprintf(stderr, "Failed to write %s\n", path.c_str());
    }
    fx::cb8_close(dec);
    printf("Wrote %u/%u frames to %s\n", written, info.frame_count, outdir);
    return (written == info.frame_count) ? 0 : 1;
}

int cmd_cb8(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: fx cb8 <subcommand> ...\n");
        fprintf(stderr, "  fx cb8 info   <file.CB8>\n");
        fprintf(stderr, "  fx cb8 frames <file.CB8> [-o output_dir]\n");
        fprintf(stderr, "  fx cb8 unpack <file.CB8> [-o output_dir]\n");
        fprintf(stderr, "  fx cb8 repack <orig.CB8> <png_dir> [-o out.CB8]\n");
        return 1;
    }
    const char* sub = argv[1];
    if (strcmp(sub, "info")   == 0) return cmd_info  (argc - 1, argv + 1);
    if (strcmp(sub, "frames") == 0) return cmd_frames(argc - 1, argv + 1);
    if (strcmp(sub, "unpack") == 0) return cmd_unpack(argc - 1, argv + 1);
    if (strcmp(sub, "repack") == 0) return cmd_repack(argc - 1, argv + 1);
    fprintf(stderr, "Unknown subcommand: %s\n", sub);
    return 1;
}
