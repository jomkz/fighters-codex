#include <catch2/catch_test_macros.hpp>
#include <fx/xmi.h>
#include <fx/ealib.h>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <map>
#include <string>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

using namespace fx;

// ---------------------------------------------------------------------------
// Build a minimal one-sequence XMI around a given EVNT payload.
// ---------------------------------------------------------------------------
static void put_be32(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back((uint8_t)(v >> 24)); b.push_back((uint8_t)(v >> 16));
    b.push_back((uint8_t)(v >> 8));  b.push_back((uint8_t)v);
}
static void chunk(std::vector<uint8_t>& b, const char* tag,
                  const std::vector<uint8_t>& body) {
    b.insert(b.end(), tag, tag + 4);
    put_be32(b, (uint32_t)body.size());
    b.insert(b.end(), body.begin(), body.end());
    if (body.size() & 1) b.push_back(0);  // IFF even padding
}

static std::vector<uint8_t> make_xmi(const std::vector<uint8_t>& evnt) {
    std::vector<uint8_t> timb = { 0x01, 0x00, 0x00, 0x00 };  // 1 timbre entry
    std::vector<uint8_t> seq;
    seq.insert(seq.end(), { 'X', 'M', 'I', 'D' });
    chunk(seq, "TIMB", timb);
    chunk(seq, "EVNT", evnt);

    std::vector<uint8_t> cat;
    cat.insert(cat.end(), { 'X', 'M', 'I', 'D' });
    chunk(cat, "FORM", seq);

    std::vector<uint8_t> body;
    body.insert(body.end(), { 'X', 'D', 'I', 'R' });
    chunk(body, "INFO", { 0x01, 0x00 });  // seq_count = 1 (LE)
    chunk(body, "CAT ", cat);

    std::vector<uint8_t> out;
    chunk(out, "FORM", body);
    return out;
}

// Minimal SMF track reader: count note-ons / note-offs, confirm end-of-track.
struct SmfStats { int non = 0, noff = 0; bool eot = false; bool ok = false; };
static SmfStats read_smf(const std::vector<uint8_t>& b) {
    SmfStats s;
    if (b.size() < 22 || memcmp(b.data(), "MThd", 4) != 0) return s;
    if (memcmp(b.data() + 14, "MTrk", 4) != 0) return s;
    size_t p = 22;
    uint8_t running = 0;
    auto vlq = [&]() { uint32_t v = 0; while (p < b.size()) {
        uint8_t c = b[p++]; v = (v << 7) | (c & 0x7F); if (!(c & 0x80)) break; } return v; };
    while (p < b.size()) {
        vlq();  // delta
        uint8_t st = b[p];
        if (st < 0x80) st = running;
        else { p++; if (st < 0xF0) running = st; }
        if (st == 0xFF) {
            uint8_t t = b[p++]; uint32_t ln = vlq(); p += ln;
            if (t == 0x2F) { s.eot = true; break; }
        } else if (st == 0xF0 || st == 0xF7) {
            uint32_t ln = vlq(); p += ln;
        } else {
            uint8_t hi = st & 0xF0;
            if (hi == 0xC0 || hi == 0xD0) p += 1;
            else {
                uint8_t vel = b[p + 1]; p += 2;
                if (hi == 0x90 && vel > 0) s.non++;
                else if (hi == 0x80 || (hi == 0x90 && vel == 0)) s.noff++;
            }
        }
    }
    s.ok = true;
    return s;
}

// ---------------------------------------------------------------------------

// The census leans on XmiDecode: an event loop that gives up halfway still produces a
// perfectly well-formed SMF, just a shorter one. These are the two cases it must tell apart.

TEST_CASE("XmiDecode reports a stream that ran to its end-of-track") {
    auto xmi = make_xmi({ 0x00, 0x90, 0x3C, 0x64, 0x10, 0xFF, 0x2F, 0x00 });
    XmiDecode st;
    auto smf = xmi_to_smf(xmi.data(), xmi.size(), 0, 60, &st);
    REQUIRE_FALSE(smf.empty());
    REQUIRE(st.end_of_track);
    REQUIRE(st.events == 2);              // the note-on and its scheduled note-off
    REQUIRE(st.consumed == st.evnt_size);
}

TEST_CASE("XmiDecode reports a stream it gave up on") {
    // An unknown status byte (0xF5). The loop stops rather than desync -- and the SMF it
    // returns still looks entirely valid, which is the whole problem.
    auto xmi = make_xmi({ 0x00, 0x90, 0x3C, 0x64, 0x10, 0xF5, 0x01, 0xFF, 0x2F, 0x00 });
    XmiDecode st;
    auto smf = xmi_to_smf(xmi.data(), xmi.size(), 0, 60, &st);
    REQUIRE_FALSE(smf.empty());           // a well-formed SMF, all the same
    REQUIRE_FALSE(st.end_of_track);       // but it never reached the end
    REQUIRE(st.consumed < st.evnt_size);
}

TEST_CASE("XmiDecode reports a truncated event") {
    auto xmi = make_xmi({ 0x00, 0x90, 0x3C });   // note-on cut short
    XmiDecode st;
    xmi_to_smf(xmi.data(), xmi.size(), 0, 60, &st);
    REQUIRE_FALSE(st.end_of_track);
}

TEST_CASE("xmi_parse walks the IFF structure") {
    auto xmi = make_xmi({ 0xFF, 0x2F, 0x00 });
    auto f = xmi_parse(xmi.data(), xmi.size());
    REQUIRE(f.valid);
    REQUIRE(f.seq_count == 1);
    REQUIRE(f.sequences.size() == 1);
    REQUIRE(f.sequences[0].timbres == 1);
    bool has_evnt = false;
    for (auto& c : f.sequences[0].chunks) if (c.tag == "EVNT") has_evnt = true;
    REQUIRE(has_evnt);
}

TEST_CASE("xmi_parse rejects a non-XMI file") {
    std::vector<uint8_t> junk = { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'X', 'D', 'I', 'R' };
    REQUIRE_FALSE(xmi_parse(junk.data(), junk.size()).valid);
}

TEST_CASE("xmi_to_smf emits a valid format-0 header") {
    auto xmi = make_xmi({ 0xFF, 0x2F, 0x00 });
    auto smf = xmi_to_smf(xmi.data(), xmi.size(), 0);
    REQUIRE(smf.size() > 22);
    REQUIRE(memcmp(smf.data(), "MThd", 4) == 0);
    REQUIRE(smf[7] == 6);      // MThd length = 6 (BE u32 at +4)
    REQUIRE(smf[9] == 0);      // format 0 (BE u16 at +8)
    REQUIRE(smf[11] == 1);     // one track (BE u16 at +10)
    REQUIRE(smf[13] == 60);    // division = default ppqn 60 (BE u16 at +12)
    REQUIRE(memcmp(smf.data() + 14, "MTrk", 4) == 0);
}

TEST_CASE("xmi_to_smf turns each note-on duration into a scheduled note-off") {
    // Two note-ons with explicit XMI durations; no note-offs in the stream.
    std::vector<uint8_t> evnt = {
        0x90, 0x3C, 0x64, 0x0A,              // note on 60, vel 100, duration 10
        0x14, 0x90, 0x3E, 0x64, 0x81, 0x00,  // delay 20, note on 62, dur 128 (VLQ)
        0xFF, 0x2F, 0x00,                    // end of track
    };
    auto xmi = make_xmi(evnt);
    auto smf = xmi_to_smf(xmi.data(), xmi.size(), 0);
    auto s = read_smf(smf);
    REQUIRE(s.ok);
    REQUIRE(s.non == 2);
    REQUIRE(s.noff == 2);   // both note-offs synthesised from durations
    REQUIRE(s.eot);
}

TEST_CASE("xmi_to_smf accumulates AIL delay bytes and passes meta through") {
    std::vector<uint8_t> evnt = {
        0x7F, 0x7F, 0x02,                    // delay 127+127+2 = 256 ticks
        0xFF, 0x51, 0x03, 0x06, 0x1A, 0x80,  // a tempo meta in the stream
        0xB0, 0x07, 0x64,                    // control change (volume)
        0xFF, 0x2F, 0x00,
    };
    auto xmi = make_xmi(evnt);
    auto smf = xmi_to_smf(xmi.data(), xmi.size(), 0);
    auto s = read_smf(smf);
    REQUIRE(s.ok);
    REQUIRE(s.eot);
    // The passed-through tempo meta appears somewhere after our default one.
    std::vector<uint8_t> needle = { 0xFF, 0x51, 0x03, 0x06, 0x1A, 0x80 };
    auto it = std::search(smf.begin(), smf.end(), needle.begin(), needle.end());
    REQUIRE(it != smf.end());
}

TEST_CASE("xmi_to_smf rejects an out-of-range sequence index") {
    auto xmi = make_xmi({ 0xFF, 0x2F, 0x00 });
    REQUIRE(xmi_to_smf(xmi.data(), xmi.size(), 5).empty());
}

// ---------------------------------------------------------------------------
// The real-asset census (#491 A).
//
// XMI.md claimed the 78 shipped files parse and export, and no test opened one. There is no
// round-trip here to lean on -- XMI->MID is a one-way translation -- so the ONLY check that
// means anything is whether the decode consumed what it was given. The event loop bails out
// on an unknown status byte rather than desync, which is right, but a stream it gave up on
// halfway yields an SMF that looks perfectly fine: just shorter, and silently so.
// ---------------------------------------------------------------------------

TEST_CASE("XMI decode census: every shipped sequence, decoded to its end") {
    const char* root = std::getenv("FX_FA_ROOT");
    if (!root || !*root)
        SKIP("FX_FA_ROOT not set (real-asset mode runs on the benches)");
    namespace fs = std::filesystem;

    auto lower = [](std::string s) {
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };

    int files = 0, sequences = 0;
    size_t total_events = 0;
    std::map<std::string, int> tags;

    for (const auto& de : fs::directory_iterator(root)) {
        if (!de.is_regular_file()) continue;
        if (lower(de.path().extension().string()) != ".lib") continue;
        std::ifstream f(de.path(), std::ios::binary | std::ios::ate);
        std::vector<uint8_t> lib((size_t)f.tellg());
        f.seekg(0);
        f.read((char*)lib.data(), (std::streamsize)lib.size());

        for (const auto& e : ealib_read_dir(lib.data(), lib.size())) {
            if (lower(fs::path(e.name).extension().string()) != ".xmi") continue;
            auto rec = ealib_extract(lib.data(), lib.size(), e, true);
            INFO(de.path().filename().string() << " : " << e.name);
            REQUIRE_FALSE(rec.empty());

            XmiFile x = xmi_parse(rec.data(), rec.size());
            REQUIRE(x.valid);

            // 1. The INFO chunk declares how many sequences the file holds. If the walk
            //    found fewer FORM XMIDs than that, it silently lost one.
            REQUIRE(x.seq_count == x.sequences.size());
            REQUIRE(x.seq_count >= 1);

            for (size_t i = 0; i < x.sequences.size(); i++) {
                INFO("sequence " << i);
                for (const XmiChunk& c : x.sequences[i].chunks) tags[c.tag]++;

                // Every sequence carries the events it is for.
                bool has_evnt = false;
                for (const XmiChunk& c : x.sequences[i].chunks)
                    if (c.tag == "EVNT") has_evnt = true;
                REQUIRE(has_evnt);

                // 2. The decode ran to the end of the stream. Not "produced some output" --
                //    an SMF is produced either way.
                XmiDecode st;
                auto smf = xmi_to_smf(rec.data(), rec.size(), i, 60, &st);
                REQUIRE_FALSE(smf.empty());
                REQUIRE(st.end_of_track);                 // hit FF 2F, did not run out
                REQUIRE(st.consumed <= st.evnt_size);
                REQUIRE(st.evnt_size - st.consumed <= 1); // at most the IFF pad byte
                REQUIRE(st.events > 0);
                total_events += st.events;

                // 3. The SMF we emitted is well-formed: header, one track, and the track
                //    length field agrees with the bytes that follow it.
                REQUIRE(smf.size() > 22);
                REQUIRE(std::string((const char*)smf.data(), 4) == "MThd");
                REQUIRE(std::string((const char*)smf.data() + 14, 4) == "MTrk");
                const uint32_t tlen = ((uint32_t)smf[18] << 24) | ((uint32_t)smf[19] << 16) |
                                      ((uint32_t)smf[20] << 8) | smf[21];
                REQUIRE(tlen == smf.size() - 22);
                // ...and it ends on end-of-track, as an SMF must.
                REQUIRE(smf[smf.size() - 3] == 0xFF);
                REQUIRE(smf[smf.size() - 2] == 0x2F);
                REQUIRE(smf[smf.size() - 1] == 0x00);
                sequences++;
            }
            files++;
        }
    }
    REQUIRE(files > 0);

    std::string chunks;
    for (auto& [t, c] : tags) chunks += " " + t + "=" + std::to_string(c);
    WARN("XMI census: " << files << " files, " << sequences << " sequences, "
         << total_events << " events decoded; chunks:" << chunks);
}
