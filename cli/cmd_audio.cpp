#include "fx/audio.h"
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

static bool write_file(const char* path, const std::vector<uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write((const char*)data.data(), (std::streamsize)data.size());
    return f.good();
}

// audio info <file>
static int cmd_info(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "Usage: fx audio info <file.11K|.5K|.8K>\n"); return 1; }

    const char* src = argv[1];
    auto data = read_file(src);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", src); return 1; }

    fs::path p(src);
    std::string ext = p.extension().string();
    uint32_t rate = audio_rate_from_ext(ext);

    // Allow override with -r
    for (int i = 2; i < argc - 1; ++i)
        if (strcmp(argv[i], "-r") == 0) rate = (uint32_t)atoi(argv[i + 1]);

    if (rate == 0) {
        fprintf(stderr, "Unknown sample rate for extension '%s'. Use -r <hz>\n", ext.c_str());
        return 1;
    }

    auto info = audio_info(data.data(), data.size(), rate);
    printf("File:        %s\n", src);
    printf("Format:      8-bit unsigned mono PCM\n");
    printf("Sample rate: %u Hz\n", info.sample_rate);
    printf("Samples:     %u\n", info.num_samples);
    printf("Duration:    %.3f s\n", info.duration_s);
    printf("Size:        %zu bytes\n", data.size());
    return 0;
}

// audio unpack <file.11K> [-o out.wav] [-r rate]
static int cmd_unpack(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: fx audio unpack <file.11K|.5K|.8K> [-o out.wav] [-r hz]\n");
        return 1;
    }

    const char* src = argv[1];
    const char* dst = nullptr;
    uint32_t    rate_override = 0;

    for (int i = 2; i < argc - 1; ++i) {
        if (strcmp(argv[i], "-o") == 0) dst = argv[i + 1];
        if (strcmp(argv[i], "-r") == 0) rate_override = (uint32_t)atoi(argv[i + 1]);
    }

    auto data = read_file(src);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", src); return 1; }

    fs::path p(src);
    uint32_t rate = rate_override ? rate_override : audio_rate_from_ext(p.extension().string());
    if (rate == 0) {
        fprintf(stderr, "Unknown sample rate. Use -r <hz>\n");
        return 1;
    }

    auto wav = audio_to_wav(data.data(), data.size(), rate);
    if (wav.empty()) { fprintf(stderr, "Conversion failed\n"); return 1; }

    std::string out_path = dst ? dst : (p.stem().string() + ".wav");
    if (!write_file(out_path.c_str(), wav)) {
        fprintf(stderr, "Cannot write: %s\n", out_path.c_str());
        return 1;
    }

    auto info = audio_info(data.data(), data.size(), rate);
    printf("%s -> %s  (%u Hz, %.3f s)\n", src, out_path.c_str(), rate, info.duration_s);
    return 0;
}

// audio pack <in.wav> -o <out.11K>
static int cmd_pack(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: fx audio pack <in.wav> -o <out.11K|.5K|.8K> [-r hz]\n");
        return 1;
    }

    const char* src = argv[1];
    const char* dst = nullptr;
    uint32_t    rate_override = 0;

    for (int i = 2; i < argc - 1; ++i) {
        if (strcmp(argv[i], "-o") == 0) dst = argv[i + 1];
        if (strcmp(argv[i], "-r") == 0) rate_override = (uint32_t)atoi(argv[i + 1]);
    }

    if (!dst) { fprintf(stderr, "Missing -o output path\n"); return 1; }

    auto data = read_file(src);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", src); return 1; }

    uint32_t wav_rate = 0;
    auto pcm = wav_to_pcm(data.data(), data.size(), &wav_rate);
    if (pcm.empty()) {
        fprintf(stderr, "Invalid WAV (must be 8-bit unsigned mono PCM): %s\n", src);
        return 1;
    }

    // Warn if rate doesn't match expected for extension
    fs::path dp(dst);
    uint32_t expected = rate_override ? rate_override : audio_rate_from_ext(dp.extension().string());
    if (expected && wav_rate != expected) {
        fprintf(stderr, "Warning: WAV sample rate %u Hz doesn't match expected %u Hz for %s\n",
                wav_rate, expected, dp.extension().string().c_str());
    }

    if (!write_file(dst, pcm)) { fprintf(stderr, "Cannot write: %s\n", dst); return 1; }
    printf("%s -> %s  (%u samples)\n", src, dst, (uint32_t)pcm.size());
    return 0;
}

int cmd_audio(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: fx audio <info|unpack|pack> ...\n");
        return 1;
    }
    const char* sub = argv[1];
    if (strcmp(sub, "info")   == 0) return cmd_info(argc - 1, argv + 1);
    if (strcmp(sub, "unpack") == 0) return cmd_unpack(argc - 1, argv + 1);
    if (strcmp(sub, "pack")   == 0) return cmd_pack(argc - 1, argv + 1);
    fprintf(stderr, "Unknown audio subcommand: %s\n", sub);
    return 1;
}
