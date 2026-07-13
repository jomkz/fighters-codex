#include "fx/lay.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

// stb_image_write declarations only — implementation compiled in cmd_pic.cpp
#include "stb_image_write.h"

static std::string json_str(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c == '"') out += "\\\"";
        else out += c;
    }
    out += '"';
    return out;
}

static void write_png_cb(void* ctx, void* data, int size) {
    auto* buf = static_cast<std::vector<uint8_t>*>(ctx);
    const uint8_t* b = static_cast<const uint8_t*>(data);
    buf->insert(buf->end(), b, b + size);
}

static void usage_lay() {
    puts("Usage:");
    puts("  fx lay dump     <file.LAY>");
    puts("  fx lay gradient <file.LAY> [-o output.png]");
    puts("  fx lay set      <file.LAY> <key=value ...> [-o out.LAY]");
    puts("    keys: sky_angle_scale, below_angle_scale, or layerN.<field>");
    puts("    layer fields: the scalars fx lay dump prints, plus cloud_pic /");
    puts("    sky_pic (gradient ramps are lib/GUI-only)");
}

static int cmd_lay_dump(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Cannot open: %s\n", path); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    std::vector<uint8_t> buf(sz);
    fread(buf.data(), 1, sz, f);
    fclose(f);

    fx::LayFile lay = fx::lay_parse(buf.data(), buf.size());
    if (!lay.valid) { fprintf(stderr, "Failed to parse LAY: %s\n", path); return 1; }

    puts("{");
    printf("  \"sky_angle_scale\": %u,\n", lay.sky_angle_scale);
    printf("  \"below_angle_scale\": %u,\n", lay.below_angle_scale);
    printf("  \"layer_array_va\": \"0x%08X\",\n", lay.layer_array_va);
    printf("  \"colour_entry_table_va\": \"0x%08X\",\n", lay.colour_entry_table_va);
    printf("  \"palette_buffer_va\": \"0x%08X\",\n", lay.palette_buffer_va);

    printf("  \"sky_layer_vas\": [");
    for (int i = 0; i < 10; ++i)
        printf("%s\"0x%08X\"", i ? ", " : "", lay.sky_layer_va[i]);
    puts("],");

    printf("  \"below_layer_vas\": [");
    for (int i = 0; i < 10; ++i)
        printf("%s\"0x%08X\"", i ? ", " : "", lay.below_layer_va[i]);
    puts("],");

    printf("  \"layers\": [\n");
    for (size_t i = 0; i < lay.layers.size(); ++i) {
        const fx::LayLayer& L = lay.layers[i];
        bool last = (i + 1 == lay.layers.size());
        puts("    {");
        printf("      \"flags\": %u,\n", L.flags);
        printf("      \"sel_alt_min\": %d, \"sel_alt_max\": %d,\n", L.sel_alt_min, L.sel_alt_max);
        printf("      \"alt_min\": %d, \"alt_max\": %d,\n", L.alt_min, L.alt_max);
        printf("      \"fog_alt_low\": %d, \"vis_lo\": %d,\n", L.fog_alt_low, L.vis_lo);
        printf("      \"fog_alt_high\": %d, \"vis_hi\": %d,\n", L.fog_alt_high, L.vis_hi);
        printf("      \"extinction_param\": %d,\n", L.extinction_param);
        printf("      \"gradient_alt_start\": %d, \"gradient_val_start\": %d,\n",
               L.gradient_alt_start, L.gradient_val_start);
        printf("      \"gradient_alt_end\": %d, \"gradient_val_end\": %d,\n",
               L.gradient_alt_end, L.gradient_val_end);
        printf("      \"base_rgb\": [%u, %u, %u],\n", L.base_rgb[0], L.base_rgb[1], L.base_rgb[2]);
        printf("      \"horizon_base_rgb\": [%u, %u, %u],\n",
               L.horizon_base_rgb[0], L.horizon_base_rgb[1], L.horizon_base_rgb[2]);
        printf("      \"fog_density\": %u,\n", L.fog_density);
        printf("      \"visibility\": %u,\n", L.visibility);
        printf("      \"cloud_pic\": %s,\n", json_str(L.cloud_pic).c_str());
        printf("      \"sky_pic\": %s,\n", json_str(L.sky_pic).c_str());

        // zenith gradient
        printf("      \"zenith_grad\": [");
        for (int j = 0; j < 31; ++j)
            printf("%s[%u,%u,%u]", j ? "," : "", L.zenith_grad[j].r, L.zenith_grad[j].g, L.zenith_grad[j].b);
        puts("],");

        // horizon gradient
        printf("      \"horizon_grad\": [");
        for (int j = 0; j < 32; ++j)
            printf("%s[%u,%u,%u]", j ? "," : "", L.horizon_grad[j].r, L.horizon_grad[j].g, L.horizon_grad[j].b);
        puts("]");

        printf("    }%s\n", last ? "" : ",");
    }
    puts("  ]");
    puts("}");
    return 0;
}

static int cmd_lay_gradient(int argc, char** argv) {
    const char* path = nullptr;
    const char* out_path = "gradient.png";
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) out_path = argv[++i];
        else if (!path) path = argv[i];
    }
    if (!path) { usage_lay(); return 1; }

    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Cannot open: %s\n", path); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    std::vector<uint8_t> buf(sz);
    fread(buf.data(), 1, sz, f);
    fclose(f);

    fx::LayFile lay = fx::lay_parse(buf.data(), buf.size());
    if (!lay.valid || lay.layers.empty()) {
        fprintf(stderr, "Failed to parse LAY: %s\n", path);
        return 1;
    }

    // One row per layer, 63 columns (31 zenith + 32 horizon), 3 bytes per pixel (RGB)
    const int COLS = 63;
    const int ROW_H = 16;
    int rows = (int)lay.layers.size();
    int W = COLS, H = rows * ROW_H;
    std::vector<uint8_t> img(W * H * 3, 0);

    for (int li = 0; li < rows; ++li) {
        const fx::LayLayer& L = lay.layers[li];
        for (int row = 0; row < ROW_H; ++row) {
            int y = li * ROW_H + row;
            for (int x = 0; x < 31; ++x) {
                uint8_t* p = img.data() + (y * W + x) * 3;
                // VGA 6-bit to 8-bit
                p[0] = (L.zenith_grad[x].r << 2) | (L.zenith_grad[x].r >> 4);
                p[1] = (L.zenith_grad[x].g << 2) | (L.zenith_grad[x].g >> 4);
                p[2] = (L.zenith_grad[x].b << 2) | (L.zenith_grad[x].b >> 4);
            }
            for (int x = 0; x < 32; ++x) {
                uint8_t* p = img.data() + (y * W + 31 + x) * 3;
                p[0] = (L.horizon_grad[x].r << 2) | (L.horizon_grad[x].r >> 4);
                p[1] = (L.horizon_grad[x].g << 2) | (L.horizon_grad[x].g >> 4);
                p[2] = (L.horizon_grad[x].b << 2) | (L.horizon_grad[x].b >> 4);
            }
        }
    }

    std::vector<uint8_t> png_buf;
    stbi_write_png_to_func(write_png_cb, &png_buf, W, H, 3, img.data(), W * 3);

    FILE* fo = fopen(out_path, "wb");
    if (!fo) { fprintf(stderr, "Cannot write: %s\n", out_path); return 1; }
    fwrite(png_buf.data(), 1, png_buf.size(), fo);
    fclose(fo);

    printf("Written %dx%d gradient PNG (%d layers) to %s\n", W, H, rows, out_path);
    return 0;
}

// Apply one scalar edit to a layer by dump-field name. Returns false for an
// unknown field.
static bool lay_set_field(fx::LayLayer& L, const std::string& field,
                          const std::string& val) {
    const int32_t iv = (int32_t)atol(val.c_str());
    struct F { const char* name; int32_t* p; };
    const F ints[] = {
        {"sel_alt_min", &L.sel_alt_min}, {"sel_alt_max", &L.sel_alt_max},
        {"alt_min", &L.alt_min},         {"alt_max", &L.alt_max},
        {"fog_alt_low", &L.fog_alt_low}, {"vis_lo", &L.vis_lo},
        {"fog_alt_high", &L.fog_alt_high}, {"vis_hi", &L.vis_hi},
        {"extinction_param", &L.extinction_param},
        {"gradient_alt_start", &L.gradient_alt_start},
        {"gradient_val_start", &L.gradient_val_start},
        {"gradient_alt_end", &L.gradient_alt_end},
        {"gradient_val_end", &L.gradient_val_end},
    };
    for (const auto& f : ints) {
        if (field == f.name) { *f.p = iv; return true; }
    }
    if (field == "flags")       { L.flags = (uint8_t)iv; return true; }
    if (field == "fog_density") { L.fog_density = (uint32_t)iv; return true; }
    if (field == "visibility")  { L.visibility = (uint8_t)iv; return true; }
    if (field == "cloud_pic")   { L.cloud_pic = val; return true; }
    if (field == "sky_pic")     { L.sky_pic = val; return true; }
    return false;
}

static int cmd_lay_set(int argc, char** argv) {
    const char* src = nullptr;
    const char* dst = nullptr;
    std::vector<const char*> edits;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) dst = argv[++i];
        else if (!src) src = argv[i];
        else edits.push_back(argv[i]);
    }
    if (!src || edits.empty()) { usage_lay(); return 1; }

    std::ifstream f(src, std::ios::binary | std::ios::ate);
    if (!f) { fprintf(stderr, "Cannot open: %s\n", src); return 1; }
    std::vector<uint8_t> buf((size_t)f.tellg());
    f.seekg(0);
    f.read((char*)buf.data(), (std::streamsize)buf.size());

    fx::LayFile lay = fx::lay_parse(buf.data(), buf.size());
    if (!lay.valid) { fprintf(stderr, "Failed to parse LAY: %s\n", src); return 1; }

    for (const char* e : edits) {
        const char* eq = strchr(e, '=');
        if (!eq || eq == e) { fprintf(stderr, "Bad edit (want key=value): %s\n", e); return 1; }
        std::string key(e, (size_t)(eq - e));
        std::string val(eq + 1);

        if (key == "sky_angle_scale")   { lay.sky_angle_scale = (uint32_t)atol(val.c_str()); continue; }
        if (key == "below_angle_scale") { lay.below_angle_scale = (uint32_t)atol(val.c_str()); continue; }

        // layerN.<field>
        if (key.rfind("layer", 0) == 0) {
            size_t dot = key.find('.');
            if (dot != std::string::npos && dot > 5) {
                char* end = nullptr;
                long n = strtol(key.c_str() + 5, &end, 10);
                if (end == key.c_str() + dot && n >= 0 &&
                    (size_t)n < lay.layers.size()) {
                    if (lay_set_field(lay.layers[(size_t)n], key.substr(dot + 1), val))
                        continue;
                }
            }
        }
        fprintf(stderr, "Unknown key: %s\n", key.c_str());
        return 1;
    }

    auto out = fx::lay_repack(buf.data(), buf.size(), lay);
    if (out.empty()) {
        fprintf(stderr, "Repack failed (sentinel bit moved or oversized pic name?)\n");
        return 1;
    }
    std::string out_path = dst ? dst : (std::string(src) + ".out");
    std::ofstream ofs(out_path, std::ios::binary);
    if (!ofs || !ofs.write((const char*)out.data(), (std::streamsize)out.size())) {
        fprintf(stderr, "Cannot write: %s\n", out_path.c_str());
        return 1;
    }
    printf("%s -> %s (%zu bytes, %zu edit(s))\n", src, out_path.c_str(),
           out.size(), edits.size());
    return 0;
}

int cmd_lay(int argc, char** argv) {
    if (argc < 2) { usage_lay(); return 1; }
    const char* sub = argv[1];
    if (strcmp(sub, "dump") == 0) {
        if (argc < 3) { usage_lay(); return 1; }
        return cmd_lay_dump(argv[2]);
    }
    if (strcmp(sub, "gradient") == 0) {
        return cmd_lay_gradient(argc - 1, argv + 1);
    }
    if (strcmp(sub, "set") == 0) {
        return cmd_lay_set(argc - 1, argv + 1);
    }
    fprintf(stderr, "Unknown subcommand: %s\n", sub);
    usage_lay();
    return 1;
}
