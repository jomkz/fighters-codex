#include <catch2/catch_test_macros.hpp>
#include <fx/vdo.h>
#include <algorithm>
#include <fx/ealib.h>
#include <fx/fbc.h>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <cstdint>
#include <vector>

using namespace fx;

namespace {

void put16(std::vector<uint8_t>& v, size_t off, uint16_t x) {
    v[off] = (uint8_t)(x & 0xff);
    v[off + 1] = (uint8_t)(x >> 8);
}

// An 816-byte VDO header for a w x h movie, palette index i -> grey (i,i,i)6.
std::vector<uint8_t> make_header(uint16_t w, uint16_t h) {
    std::vector<uint8_t> v(816, 0);
    v[0] = 'R'; v[1] = 'A'; v[2] = 'T'; v[3] = 'V'; v[4] = 'I'; v[5] = 'D';
    v[6] = 1; v[7] = 2;
    put16(v, 0x08, 15);           // fps (low half)
    put16(v, 0x12, w);
    put16(v, 0x14, h);
    put16(v, 0x16, 256);
    put16(v, 0x18, 1);
    put16(v, 0x1a, 8000);
    for (int i = 0; i < 256; i++) {
        v[0x30 + i * 3 + 0] = (uint8_t)(i >> 2);  // 6-bit grey
        v[0x30 + i * 3 + 1] = (uint8_t)(i >> 2);
        v[0x30 + i * 3 + 2] = (uint8_t)(i >> 2);
    }
    return v;
}

// FBC = flat u32le array of frame sizes.
std::vector<uint8_t> make_fbc(const std::vector<uint32_t>& sizes) {
    std::vector<uint8_t> v;
    for (uint32_t s : sizes)
        for (int b = 0; b < 4; b++) v.push_back((uint8_t)(s >> (8 * b)));
    return v;
}

// A tag-1 whole-canvas RLE keyframe: fill all `count` pixels with `val`.
//
// The layout matches the retail frames, which is NOT what this fixture used to build: it
// put the RLE output count at +2, exactly where the codec (wrongly) read it, so the
// fixture encoded the bug and then stood guard over it. The real frame is
//
//     +0  u16  tag = 1
//     +2  u16  bytes remaining in the frame (FBC[n] - 4) — stepped over by GetVDOFrame
//     +4  u16  RLE output count (64,000 = 320x200 in every retail keyframe)
//     +6  u16  discarded
//     +8       RLE control stream
std::vector<uint8_t> frame_fill(uint16_t count, uint8_t val) {
    std::vector<uint8_t> f(8, 0);
    put16(f, 0, 1);          // tag = 1
    put16(f, 4, count);      // RLE output count, where UnRLE actually reads it
    // f[2..3] is patched below once the frame length is known; f[6..7] = discarded u16
    f.push_back((uint8_t)(0x80 | ((count - 1) & 0x7f)));  // run of count (<=128)
    f.push_back(val);
    put16(f, 2, (uint16_t)(f.size() - 4));   // the frame's own remaining-byte count
    return f;
}

// An RLE expander written from the spec (VDO.md § UnRLE), independent of the codec: the
// count is a u16 at `off`, a discarded u16 follows, then control bytes — high bit set = a
// run of (low7 + 1), or a 16-bit length when low7 == 0x7F; otherwise a literal copy.
// A test that only re-reads the FILE's bytes proves nothing about the DECODER; this
// expands them so the decoded pixels can be compared against something.
std::vector<uint8_t> rle_expand(const uint8_t* f, size_t sz, size_t off) {
    std::vector<uint8_t> out;
    if (off + 4 > sz) return out;
    uint32_t n = (uint32_t)(f[off] | (f[off + 1] << 8));
    off += 4;
    while (out.size() < n && off < sz) {
        uint8_t ctrl = f[off++];
        if (ctrl & 0x80) {
            uint32_t cnt = ctrl & 0x7f;
            if (cnt == 0x7f) {
                if (off + 2 > sz) break;
                cnt = (uint32_t)(f[off] | (f[off + 1] << 8));
                off += 2;
            }
            cnt += 1;
            if (off >= sz) break;
            uint8_t val = f[off++];
            if (out.size() + cnt > n) cnt = (uint32_t)(n - out.size());
            out.insert(out.end(), cnt, val);
        } else {
            uint32_t cnt = ctrl;
            if (off + cnt > sz) break;
            if (out.size() + cnt > n) cnt = (uint32_t)(n - out.size());
            out.insert(out.end(), f + off, f + off + cnt);
            off += cnt;
        }
    }
    return out;
}

} // namespace

TEST_CASE("vdo_info validates the RATVID header") {
    auto v = make_header(8, 8);
    VdoInfo info{};
    REQUIRE(vdo_info(v.data(), v.size(), &info));
    REQUIRE(info.width == 8);
    REQUIRE(info.height == 8);
    REQUIRE(info.audio_hz == 8000);

    v[0] = 'X';  // break the magic
    REQUIRE_FALSE(vdo_info(v.data(), v.size(), &info));
}

TEST_CASE("vdo_open requires the FBC to tile the frame region") {
    auto v = make_header(8, 8);
    auto f0 = frame_fill(64, 7);
    v.insert(v.end(), f0.begin(), f0.end());

    auto fbc_good = make_fbc({ (uint32_t)f0.size() });
    auto fbc_bad = make_fbc({ (uint32_t)f0.size() + 1 });

    VdoDecoder* d = vdo_open(v.data(), v.size(), fbc_good.data(), fbc_good.size());
    REQUIRE(d != nullptr);
    vdo_close(d);
    REQUIRE(vdo_open(v.data(), v.size(), fbc_bad.data(), fbc_bad.size()) == nullptr);
}

TEST_CASE("vdo decodes a keyframe and a per-pixel delta frame") {
    auto v = make_header(8, 8);  // 64 px, 8 groups

    // Frame 0: fill the 64-pixel canvas with palette index 7.
    auto f0 = frame_fill(64, 7);

    // Frame 1: mask path. Mask = group 0 fully copied (0xFF), rest kept (0x00);
    // raw source supplies the 8 new pixels = index 42.
    std::vector<uint8_t> f1;
    // mask sub-stream: count=8, dead u16, one literal of 8 bytes.
    std::vector<uint8_t> mask_stream(4, 0);
    put16(mask_stream, 0, 8);                 // output count = 8 groups
    mask_stream.push_back(8);                 // literal, 8 bytes
    mask_stream.push_back(0xFF);              // group 0: copy all 8
    for (int i = 0; i < 7; i++) mask_stream.push_back(0x00);
    uint16_t sz1 = (uint16_t)mask_stream.size();
    f1.resize(2);
    put16(f1, 0, (uint16_t)(0x8000 | sz1));   // tag: RLE mask, low15 = sz1
    f1.insert(f1.end(), mask_stream.begin(), mask_stream.end());
    // marker (non-zero, not 0xFFFF) -> raw source follows
    f1.push_back(0x01); f1.push_back(0x00);   // marker = 1
    for (int i = 0; i < 8; i++) f1.push_back(42);  // 8 raw source pixels

    v.insert(v.end(), f0.begin(), f0.end());
    v.insert(v.end(), f1.begin(), f1.end());
    auto fbc = make_fbc({ (uint32_t)f0.size(), (uint32_t)f1.size() });

    VdoDecoder* d = vdo_open(v.data(), v.size(), fbc.data(), fbc.size());
    REQUIRE(d != nullptr);
    REQUIRE(vdo_frame_count(d) == 2);

    auto frame0 = vdo_decode_frame(d, 0);
    REQUIRE(frame0.size() == 64);
    for (uint8_t px : frame0) REQUIRE(px == 7);

    auto frame1 = vdo_decode_frame(d, 1);
    REQUIRE(frame1.size() == 64);
    for (int i = 0; i < 8; i++) REQUIRE(frame1[i] == 42);   // group 0 changed
    for (int i = 8; i < 64; i++) REQUIRE(frame1[i] == 7);   // rest kept

    // Rewind: requesting frame 0 again replays cleanly.
    auto again = vdo_decode_frame(d, 0);
    for (uint8_t px : again) REQUIRE(px == 7);

    auto rgba = vdo_decode_frame_rgba(d, 1);
    REQUIRE(rgba.size() == 64 * 4);
    REQUIRE(rgba[3] == 255);  // opaque

    vdo_close(d);
}

// ---------------------------------------------------------------------------
// Real-asset decode census (#491).
//
// The paired .FBC is an INDEPENDENT ORACLE the codec cannot fake: it declares each
// frame's byte length, and a correct decode must consume exactly that. The tag-1
// keyframe used to read its RLE output count from frame+2 — which is the frame's own
// remaining-byte count, not a pixel count — so it decoded roughly `frame_size` pixels
// instead of the 64,000 the real count at frame+4 asks for, and every keyframe lost a
// band across the bottom of the picture. No round-trip could see it: VDO is read-only.
//
// 89 tag-1 frames across 28 of the 355 shipped videos; LACA.VDO uses one for 62 of its
// 135 frames.
TEST_CASE("every real VDO decodes, and tag-1 keyframes fill the canvas") {
    const char* root = std::getenv("FX_FA_ROOT");
    if (!root || !*root)
        SKIP("FX_FA_ROOT not set (real-asset mode runs on the benches)");
    namespace fs = std::filesystem;
    auto upper = [](std::string s) {
        for (char& c : s) c = (char)std::toupper((unsigned char)c);
        return s;
    };

    std::map<std::string, std::vector<uint8_t>> assets;
    for (const auto& de : fs::directory_iterator(root)) {
        if (!de.is_regular_file()) continue;
        std::string fn = upper(de.path().filename().string());
        if (fn.size() < 4 || fn.substr(fn.size() - 4) != ".LIB") continue;
        std::ifstream f(de.path(), std::ios::binary | std::ios::ate);
        std::vector<uint8_t> lib((size_t)f.tellg());
        f.seekg(0);
        f.read((char*)lib.data(), (std::streamsize)lib.size());
        for (const auto& e : ealib_read_dir(lib.data(), lib.size())) {
            std::string name = upper(e.name);
            if (name.size() < 4) continue;
            std::string ext = name.substr(name.size() - 4);
            if (ext != ".VDO" && ext != ".FBC") continue;
            auto bytes = ealib_extract(lib.data(), lib.size(), e, true);
            if (!bytes.empty()) assets.emplace(name, std::move(bytes));
        }
    }
    if (assets.empty()) SKIP("no .VDO in this install");

    int videos = 0, keyframes = 0;
    for (const auto& [name, vdo] : assets) {
        if (name.size() < 4 || name.substr(name.size() - 4) != ".VDO") continue;
        auto fbc = assets.find(name.substr(0, name.size() - 4) + ".FBC");
        if (fbc == assets.end()) continue;

        bool ok = false;
        auto sizes = fbc_read(fbc->second.data(), fbc->second.size(), &ok);
        REQUIRE(ok);

        VdoDecoder* d = vdo_open(vdo.data(), vdo.size(),
                                 fbc->second.data(), fbc->second.size());
        INFO(name);
        REQUIRE(d != nullptr);
        const size_t pixels = (size_t)vdo_width(d) * vdo_height(d);

        // Every frame must decode — a tag-1 frame that read its count from the wrong
        // offset still "decoded", just short, so also require the canvas to be filled.
        size_t off = 816;   // frames follow the fixed header
        for (uint32_t i = 0; i < vdo_frame_count(d); ++i) {
            auto px = vdo_decode_frame(d, i);
            INFO(name << " frame " << i);
            REQUIRE(px.size() == pixels);
            const uint16_t tag = (uint16_t)(vdo[off] | (vdo[off + 1] << 8));
            if (tag == 1) {
                // The RLE count at +4 is the pixel count, and for a keyframe it is the
                // whole canvas. At +2 sits the frame's remaining-byte length (size - 4),
                // which is what the codec used to read.
                const uint16_t count  = (uint16_t)(vdo[off + 4] | (vdo[off + 5] << 8));
                const uint16_t remain = (uint16_t)(vdo[off + 2] | (vdo[off + 3] << 8));
                CHECK(count == pixels);
                CHECK(remain == sizes[i] - 4);

                // The assertion with teeth: what the DECODER produced must equal what the
                // frame's RLE actually expands to. Checking the file's own header bytes
                // (above) would pass even with the codec reading the wrong offset — the
                // bug lives in the decode, so the decode is what has to be compared.
                auto want = rle_expand(vdo.data() + off, sizes[i], 4);
                REQUIRE(want.size() == pixels);
                CHECK(std::equal(px.begin(), px.end(), want.begin()));
                ++keyframes;
            }
            off += sizes[i];
        }
        vdo_close(d);
        ++videos;
    }
    REQUIRE(videos > 0);
    REQUIRE(keyframes > 0);
    WARN("VDO census: " << videos << " videos decoded, " << keyframes
                        << " tag-1 keyframes fill the canvas");
}
