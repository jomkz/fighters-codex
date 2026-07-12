#include <catch2/catch_test_macros.hpp>
#include <fx/rtpatch.h>

#include <cstring>
#include <string>
#include <vector>

using namespace fx;

// ---------------------------------------------------------------------------
// All fixtures synthetic (policy: tests/fixtures/README.md). The 0xB59C codec's
// byte-exact proof is the fa_disc_install integration test against real patch
// data — an adaptive-Huffman stream cannot be hand-authored here — so these
// units cover the parts that are synthesizable: the §10 rolling checksum, the
// §9 opcode interpreter (which owns the VLI decoder), and the container parser.
// ---------------------------------------------------------------------------

// ---- §10 rolling checksum -------------------------------------------------
TEST_CASE("rtp_checksum matches known vectors", "[rtpatch]") {
    auto w1 = [](const std::string& s) {
        return rtp_checksum((const uint8_t*)s.data(), s.size(), 31);
    };
    auto w2 = [](const std::string& s) {
        return rtp_checksum((const uint8_t*)s.data(), s.size(), 30);
    };
    CHECK(rtp_checksum(nullptr, 0, 31) == 0u);
    CHECK(w1("ABC") == 0x41424300u);
    CHECK(w2("ABC") == 0x01424301u);
    CHECK(w1("hello") == 0x6c6cbfcau);
    CHECK(w2("hello") == 0x2c6dce95u);

    const uint8_t seq[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    CHECK(rtp_checksum(seq, 8, 31) == 0x07020108u);
    CHECK(rtp_checksum(seq, 8, 30) == 0x010e0b10u);
}

// ---- §9 opcode applier ----------------------------------------------------
// Opcode operands are §5 VLIs; values < 64 are a single byte, so these programs
// stay hand-writable while still exercising COPY, gaps + FLUSH, ZFILL, FILL,
// POKE, and STORE/TCOPY.
namespace {
void vli(std::vector<uint8_t>& v, uint32_t n) {
    if (n < 64) { v.push_back((uint8_t)n); return; }
    if (n < (1u << 14)) {                 // count=1 lead: 0x40 | high6, then 1 LE byte
        v.push_back((uint8_t)(0x40 | (n >> 8)));
        v.push_back((uint8_t)(n & 0xFF));
        return;
    }
    v.push_back((uint8_t)(0x60 | (n >> 16)));   // count=2 lead
    v.push_back((uint8_t)(n & 0xFF));
    v.push_back((uint8_t)((n >> 8) & 0xFF));
}
std::vector<uint8_t> apply(const std::vector<uint8_t>& src, std::vector<uint8_t> ops,
                           size_t dst) {
    return rtp_apply(src.data(), src.size(), ops.data(), ops.size(), dst, 1);
}
} // namespace

TEST_CASE("rtp_apply COPY quotes the source", "[rtpatch]") {
    std::vector<uint8_t> src = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'};
    std::vector<uint8_t> ops;
    ops.push_back(0x02); vli(ops, 0);      // SET_SOURCE 0
    ops.push_back(0x03); vli(ops, 2); vli(ops, 4);  // COPY off=2 cnt=4 -> CDEF
    ops.push_back(0x01);                   // END
    CHECK(apply(src, ops, 4) == std::vector<uint8_t>{'C', 'D', 'E', 'F'});
}

TEST_CASE("rtp_apply COPY+gap records a hole that FLUSH fills", "[rtpatch]") {
    std::vector<uint8_t> src = {'x', 'y', 'z', 'w'};
    std::vector<uint8_t> ops;
    ops.push_back(0x02); vli(ops, 0);
    ops.push_back(0x04); vli(ops, 3); vli(ops, 0); vli(ops, 2);  // gap 3, COPY off=0 cnt=2 (xy)
    ops.push_back(0x05);                    // FLUSH: fill the 3-byte gap, then EOF gap (0)
    ops.push_back('n'); ops.push_back('e'); ops.push_back('w');
    ops.push_back(0x01);
    // layout: [gap:3][copy xy] -> total 5
    CHECK(apply(src, ops, 5) == std::vector<uint8_t>{'n', 'e', 'w', 'x', 'y'});
}

TEST_CASE("rtp_apply ZFILL and FILL patterns", "[rtpatch]") {
    std::vector<uint8_t> ops;
    ops.push_back(0x02); vli(ops, 0);
    ops.push_back(0x0B); vli(ops, 3);              // ZFILL 3 bytes
    // FILL2: pattern {AB,CD}, count is a BYTE count (4 -> two pattern periods).
    ops.push_back(0x12); ops.push_back(0xAB); ops.push_back(0xCD); vli(ops, 4);
    ops.push_back(0x01);
    CHECK(apply({}, ops, 7) ==
          std::vector<uint8_t>{0, 0, 0, 0xAB, 0xCD, 0xAB, 0xCD});
}

TEST_CASE("rtp_apply POKE delta-adds into written bytes", "[rtpatch]") {
    std::vector<uint8_t> src = {0x10, 0x20, 0x30, 0x40};
    std::vector<uint8_t> ops;
    ops.push_back(0x02); vli(ops, 0);
    ops.push_back(0x03); vli(ops, 0); vli(ops, 4);   // COPY all 4
    ops.push_back(0x02); vli(ops, 0);                // SET_SOURCE resets cursors
    ops.push_back(0x06); vli(ops, 1); ops.push_back(0x05);  // POKE1 seek=1 delta=+5 -> [1]=0x25
    ops.push_back(0x06); vli(ops, 2); ops.push_back(0xFF);  // POKE1 seek=+2 (pos 3) delta=-1 -> 0x3F
    ops.push_back(0x01);
    CHECK(apply(src, ops, 4) == std::vector<uint8_t>{0x10, 0x25, 0x30, 0x3F});
}

TEST_CASE("rtp_apply STORE/TCOPY reuse a template", "[rtpatch]") {
    std::vector<uint8_t> src = {'H', 'E', 'L', 'L', 'O'};
    std::vector<uint8_t> ops;
    ops.push_back(0x02); vli(ops, 0);
    ops.push_back(0x08); vli(ops, 1); vli(ops, 3);   // STORE {off=1,cnt=3} = "ELL"
    ops.push_back(0x09); vli(ops, 0);                // TCOPY 0
    ops.push_back(0x09); vli(ops, 0);                // TCOPY 0 again
    ops.push_back(0x01);
    CHECK(apply(src, ops, 6) == std::vector<uint8_t>{'E', 'L', 'L', 'E', 'L', 'L'});
}

TEST_CASE("rtp_apply rejects a bad template index and truncation", "[rtpatch]") {
    std::vector<uint8_t> ops;
    ops.push_back(0x09); vli(ops, 5);   // TCOPY of a non-existent template
    ops.push_back(0x01);
    CHECK(apply({}, ops, 4).empty());

    std::vector<uint8_t> trunc = {0x03, 0x00};   // COPY missing its cnt operand
    CHECK(apply({'a'}, trunc, 4).empty());
}

// ---- container parser -----------------------------------------------------
namespace {
void put16(std::vector<uint8_t>& v, uint16_t x) { v.push_back(x & 0xFF); v.push_back(x >> 8); }
void put32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(x & 0xFF); v.push_back((x >> 8) & 0xFF);
    v.push_back((x >> 16) & 0xFF); v.push_back((x >> 24) & 0xFF);
}
// A minimal non-extra-mode container with one MODIFY record and one source and
// destination entry, enough to exercise rtp_read's field walk.
void entry(std::vector<uint8_t>& v, const std::string& name83, uint32_t size,
           uint32_t w1, uint32_t w2) {
    std::vector<uint8_t> desc(24, 0);
    std::memcpy(desc.data(), name83.data(), std::min<size_t>(8, name83.size()));
    desc[16] = size & 0xFF; desc[17] = (size >> 8) & 0xFF;
    desc[18] = (size >> 16) & 0xFF; desc[19] = (size >> 24) & 0xFF;
    v.insert(v.end(), desc.begin(), desc.end());
    v.push_back(0); v.push_back(0);      // b0, b1
    put32(v, w1); put32(v, w2);          // checksum words
}
} // namespace

TEST_CASE("rtp_read parses the header and a MODIFY record", "[rtpatch]") {
    std::vector<uint8_t> c;
    c.push_back('K'); c.push_back('*');
    put16(c, 0x0100);                    // version
    put16(c, 0x0000);                    // flags (no ext word, no dir table)
    put16(c, 0x0000);                    // option_flags
    put32(c, 0);                         // patch_total_size
    put32(c, 0);                         // reserved_a
    put16(c, 0);                         // default_attrs
    put16(c, 0);                         // reserved_b
    put16(c, 0);                         // cmd_flags (no combine)
    put32(c, 0);                         // reserved_c
    put16(c, 0);                         // n_special = 0
    put16(c, 0);                         // n_dirs = 0

    // MODIFY record
    put16(c, 0x4000);                    // rec_hdr: type 4, no optional fields
    for (int i = 0; i < 10; ++i) c.push_back(0);   // metadata
    put16(c, 0);                         // file_mod_flags
    c.push_back(1);                      // src_count VLI
    c.push_back(1);                      // dst_count VLI
    put32(c, 0);                         // reserved
    put32(c, 3);                         // payload_len
    entry(c, "OLD.BIN", 100, 0xAABBCCDD, 0x11223344);
    entry(c, "NEW.BIN", 128, 0, 0);
    size_t block = c.size();
    c.push_back(0xB5); c.push_back(0x9C); c.push_back(0x00);  // 3-byte "block"
    put16(c, 0x1000);                    // EOF record (type 1)

    RtpPatch p = rtp_read(c.data(), c.size());
    REQUIRE(p.records.size() == 1);
    const RtpRecord& r = p.records[0];
    CHECK(p.version == 0x0100);
    CHECK(r.mode == RtpMode::Modify);
    CHECK(r.name == "NEW.BIN");
    CHECK(r.src_size == 100u);
    CHECK(r.dst_size == 128u);
    CHECK(r.src_w1 == (0xAABBCCDDu & 0x7FFFFFFF));
    CHECK(r.src_w2 == (0x11223344u & 0x3FFFFFFF));
    CHECK(r.block_off == block);
    CHECK(r.payload_len == 3u);
}

TEST_CASE("rtp_read rejects non-RTPatch input", "[rtpatch]") {
    std::vector<uint8_t> junk = {'M', 'Z', 0, 0, 1, 2, 3, 4};
    CHECK(rtp_read(junk.data(), junk.size()).records.empty());
    CHECK(rtp_read(nullptr, 0).records.empty());
}
