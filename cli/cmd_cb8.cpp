#include "fx/cb8.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>

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

// PGM (P5) â€” palette index bytes written as 8-bit greyscale.
static bool write_pgm(const char* path, const uint8_t* pixels, uint32_t w, uint32_t h) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    fprintf(f, "P5\n%u %u\n255\n", w, h);
    bool ok = fwrite(pixels, 1, (size_t)w * h, f) == (size_t)w * h;
    fclose(f);
    return ok;
}

static int cmd_info(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: ft cb8 info <file.CB8>\n");
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
        fprintf(stderr, "Usage: ft cb8 frames <file.CB8> [-o output_dir]\n");
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
        fprintf(stderr, "Usage: ft cb8 <subcommand> ...\n");
        fprintf(stderr, "  ft cb8 info   <file.CB8>\n");
        fprintf(stderr, "  ft cb8 frames <file.CB8> [-o output_dir]\n");
        return 1;
    }
    const char* sub = argv[1];
    if (strcmp(sub, "info")   == 0) return cmd_info  (argc - 1, argv + 1);
    if (strcmp(sub, "frames") == 0) return cmd_frames(argc - 1, argv + 1);
    fprintf(stderr, "Unknown subcommand: %s\n", sub);
    return 1;
}
