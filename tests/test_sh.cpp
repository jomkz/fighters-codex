#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <fx/sh.h>
#include <cstring>
#include <vector>

using namespace fx;

// Build a minimal valid SH binary containing one VertexBuffer (3 verts) and
// one triangular Face, then EndShape.  Structure:
//   MZ stub (64 B) â†’ Phar-Lap PE header (24 B) â†’ section table (40 B)
//   â†’ CODE section (48 B)
static std::vector<uint8_t> make_minimal_sh() {
    // PE/LE header offset in the file
    const uint32_t PE_OFF   = 64;
    const uint32_t SEC_OFF  = PE_OFF + 24;        // 88
    const uint32_t CODE_OFF = SEC_OFF + 40;       // 128
    const uint32_t CODE_SZ  = 48;

    std::vector<uint8_t> buf(CODE_OFF + CODE_SZ, 0);

    // MZ stub
    buf[0] = 'M'; buf[1] = 'Z';
    buf[0x3C] = (uint8_t)(PE_OFF & 0xFF);
    buf[0x3D] = (uint8_t)((PE_OFF >> 8) & 0xFF);

    // Phar-Lap PE/LE header (24 bytes at PE_OFF)
    buf[PE_OFF + 0] = 'P'; buf[PE_OFF + 1] = 'L';   // PL signature
    buf[PE_OFF + 4] = 0x4C; buf[PE_OFF + 5] = 0x01; // machine i386
    buf[PE_OFF + 6] = 1;                              // NumSections = 1
    // SizeOfOptionalHeader at PE_OFF+20 = 0 (already zero)

    // Section table entry (40 bytes at SEC_OFF)
    // Name[8] = "CODE\0\0\0\0"
    const char* name = "CODE";
    memcpy(buf.data() + SEC_OFF, name, 4);
    // VirtualSize at SEC_OFF+8
    buf[SEC_OFF + 8]  = CODE_SZ; buf[SEC_OFF + 9]  = 0;
    buf[SEC_OFF + 10] = 0;       buf[SEC_OFF + 11] = 0;
    // VirtualAddress at SEC_OFF+12 (arbitrary)
    buf[SEC_OFF + 12] = 0x80;
    // SizeOfRawData at SEC_OFF+16
    buf[SEC_OFF + 16] = CODE_SZ;
    // PointerToRawData at SEC_OFF+20
    buf[SEC_OFF + 20] = (uint8_t)(CODE_OFF & 0xFF);
    buf[SEC_OFF + 21] = (uint8_t)((CODE_OFF >> 8) & 0xFF);

    // CODE section at CODE_OFF (48 bytes)
    uint8_t* c = buf.data() + CODE_OFF;
    // SH header opcode 0xFF (14 bytes): scale=8 (1x), extents 10/10/10
    c[0]=0xFF; c[1]=0xFF;                     // opcode
    c[2]=0;    c[3]=0;                         // unk0
    c[4]=0;    c[5]=0;                         // unk1
    c[6]=8;    c[7]=0;                         // scale = 8
    c[8]=10;   c[9]=0;                         // ext X = 10
    c[10]=10;  c[11]=0;                        // ext Y = 10
    c[12]=10;  c[13]=0;                        // ext Z = 10

    // VertexBuffer 0x82 0x00 (24 bytes): 3 verts at push_at=0
    c[14]=0x82; c[15]=0x00;                    // opcode
    c[16]=3;    c[17]=0;                       // nverts = 3
    c[18]=0;    c[19]=0;                       // push_at = 0
    // Vertex 0: (10, 0, 0)
    c[20]=10;   c[21]=0;  c[22]=0; c[23]=0;  c[24]=0; c[25]=0;
    // Vertex 1: (-10, 0, 0)  -- 0xFFF6 = -10 in i16
    c[26]=0xF6; c[27]=0xFF; c[28]=0; c[29]=0; c[30]=0; c[31]=0;
    // Vertex 2: (0, 10, 0)
    c[32]=0;    c[33]=0;  c[34]=10; c[35]=0;  c[36]=0; c[37]=0;

    // Face 0xFC (9 bytes): color=7, 3 indices {0,1,2}
    c[38]=0xFC; // opcode
    c[39]=0x00; // content_flags (no normal, no tex)
    c[40]=0x00; // layout_flags  (byte indices)
    c[41]=0x07; // color
    c[42]=0x00; // not shadow
    c[43]=3;    // nindices
    c[44]=0;    c[45]=1;    c[46]=2; // indices

    // EndShape
    c[47]=0x01;

    return buf;
}

// Like make_minimal_sh but the face carries HAVE_TEXCOORDS (byte mode): three
// (s,t) texel pairs after the indices.
static std::vector<uint8_t> make_textured_sh() {
    const uint32_t PE_OFF   = 64;
    const uint32_t SEC_OFF  = PE_OFF + 24;
    const uint32_t CODE_OFF = SEC_OFF + 40;
    const uint32_t CODE_SZ  = 60;

    std::vector<uint8_t> buf(CODE_OFF + CODE_SZ, 0);
    buf[0] = 'M'; buf[1] = 'Z';
    buf[0x3C] = (uint8_t)(PE_OFF & 0xFF);
    buf[0x3D] = (uint8_t)((PE_OFF >> 8) & 0xFF);
    buf[PE_OFF + 0] = 'P'; buf[PE_OFF + 1] = 'L';
    buf[PE_OFF + 4] = 0x4C; buf[PE_OFF + 5] = 0x01;
    buf[PE_OFF + 6] = 1;
    memcpy(buf.data() + SEC_OFF, "CODE", 4);
    buf[SEC_OFF + 8]  = CODE_SZ;
    buf[SEC_OFF + 12] = 0x80;
    buf[SEC_OFF + 16] = CODE_SZ;
    buf[SEC_OFF + 20] = (uint8_t)(CODE_OFF & 0xFF);
    buf[SEC_OFF + 21] = (uint8_t)((CODE_OFF >> 8) & 0xFF);

    uint8_t* c = buf.data() + CODE_OFF;
    c[0]=0xFF; c[1]=0xFF; c[6]=8; c[8]=10; c[10]=10; c[12]=10;   // header, scale 8
    // VertexBuffer: 3 verts
    c[14]=0x82; c[15]=0x00; c[16]=3;
    c[20]=10;                                   // v0.x = 10
    c[26]=0xF6; c[27]=0xFF;                      // v1.x = -10
    c[32]=0; c[34]=10;                           // v2.y = 10
    // Face 0xFC with HAVE_TEXCOORDS (content 0x04), byte texcoords (layout 0x01)
    c[38]=0xFC; c[39]=0x04; c[40]=0x01; c[41]=0x07; c[42]=0x00;
    c[43]=3;    c[44]=0; c[45]=1; c[46]=2;        // 3 indices
    c[47]=0;  c[48]=0;                            // (s,t) corner 0 = (0,0)
    c[49]=255; c[50]=0;                           // corner 1 = (255,0)
    c[51]=128; c[52]=200;                         // corner 2 = (128,200)
    c[53]=0x01;                                   // EndShape
    return buf;
}

TEST_CASE("sh_parse_mesh extracts per-corner texcoords when HAVE_TEXCOORDS") {
    auto data = make_textured_sh();
    ShMesh m = sh_parse_mesh(data.data(), data.size());
    REQUIRE(m.faces.size() == 1u);
    const auto& f = m.faces[0];
    REQUIRE(f.indices.size() == 3u);
    REQUIRE(f.texcoords.size() == 3u);
    REQUIRE(f.texcoords[0].s == Catch::Approx(0.0f));
    REQUIRE(f.texcoords[0].t == Catch::Approx(0.0f));
    REQUIRE(f.texcoords[1].s == Catch::Approx(255.0f));
    REQUIRE(f.texcoords[2].s == Catch::Approx(128.0f));
    REQUIRE(f.texcoords[2].t == Catch::Approx(200.0f));
}

TEST_CASE("sh_parse_mesh leaves texcoords empty for untextured faces") {
    auto data = make_minimal_sh();
    ShMesh m = sh_parse_mesh(data.data(), data.size());
    REQUIRE(m.faces.size() == 1u);
    REQUIRE(m.faces[0].texcoords.empty());
}

TEST_CASE("sh_to_obj emits vt and v/vt faces for textured meshes") {
    auto data = make_textured_sh();
    ShMesh m  = sh_parse_mesh(data.data(), data.size());
    auto obj  = sh_to_obj(m);
    REQUIRE(obj.find("\nvt ") != std::string::npos);
    REQUIRE(obj.find("f 1/1 2/2 3/3") != std::string::npos);
}

TEST_CASE("sh_parse_mesh on empty data returns empty mesh") {
    ShMesh m = sh_parse_mesh(nullptr, 0);
    REQUIRE(m.vertices.empty());
    REQUIRE(m.faces.empty());
}

TEST_CASE("sh_parse_mesh on garbage data returns empty mesh") {
    uint8_t garbage[16] = {0xDE, 0xAD, 0xBE, 0xEF};
    ShMesh m = sh_parse_mesh(garbage, 16);
    REQUIRE(m.vertices.empty());
    REQUIRE(m.faces.empty());
}

// Regression (#118 fuzz): a hostile PE-header offset must be rejected, not run
// through a 32-bit bounds check that wraps — a huge pe_off used to produce a
// wild pointer and an out-of-bounds read (SEGV) in find_code_section.
TEST_CASE("sh_parse rejects an out-of-range PE header offset") {
    std::vector<uint8_t> buf(0x80, 0);
    buf[0] = 'M'; buf[1] = 'Z';
    buf[0x3C] = 0xF0; buf[0x3D] = 0xFF; buf[0x3E] = 0xFF; buf[0x3F] = 0xFF;  // 0xFFFFFFF0
    ShInfo info = sh_parse_info(buf.data(), buf.size());
    REQUIRE(info.vert_count == 0);
    ShMesh mesh = sh_parse_mesh(buf.data(), buf.size());
    REQUIRE(mesh.vertices.empty());
    REQUIRE(mesh.faces.empty());
}

// Regression (#118 fuzz): a section whose SizeOfRawData is near UINT32_MAX must
// not pass raw_ptr+raw_sz <= size by wrapping — that returned a bogus multi-GB
// code section and a malloc(~4 GB) OOM in sh_parse_mesh's visited map.
TEST_CASE("sh_parse rejects a section with an overflowing raw size") {
    auto buf = make_minimal_sh();
    const size_t rawsz = 64 + 24 + 16;  // PE_OFF + section table + SizeOfRawData
    buf[rawsz + 0] = 0xFF; buf[rawsz + 1] = 0xFF;
    buf[rawsz + 2] = 0xFF; buf[rawsz + 3] = 0xFF;
    ShMesh mesh = sh_parse_mesh(buf.data(), buf.size());  // must not OOM
    REQUIRE(mesh.vertices.empty());
    REQUIRE(mesh.faces.empty());
}

TEST_CASE("sh_parse_mesh extracts vertices from VertexBuffer instruction") {
    auto data = make_minimal_sh();
    ShMesh m = sh_parse_mesh(data.data(), data.size());
    REQUIRE(m.vertices.size() == 3u);
    REQUIRE(m.vertices[0].x == Catch::Approx(10.0f));
    REQUIRE(m.vertices[0].y == Catch::Approx(0.0f));
    REQUIRE(m.vertices[1].x == Catch::Approx(-10.0f));
    REQUIRE(m.vertices[2].y == Catch::Approx(10.0f));
}

TEST_CASE("sh_parse_mesh extracts face with correct indices and color") {
    auto data = make_minimal_sh();
    ShMesh m = sh_parse_mesh(data.data(), data.size());
    REQUIRE(m.faces.size() == 1u);
    REQUIRE(m.faces[0].color == 7);
    REQUIRE(m.faces[0].indices.size() == 3u);
    REQUIRE(m.faces[0].indices[0] == 0u);
    REQUIRE(m.faces[0].indices[1] == 1u);
    REQUIRE(m.faces[0].indices[2] == 2u);
}

TEST_CASE("sh_parse_mesh scale field 8 gives 1x multiplier") {
    auto data = make_minimal_sh();
    ShMesh m = sh_parse_mesh(data.data(), data.size());
    REQUIRE(m.scale == Catch::Approx(1.0f));
}

TEST_CASE("sh_parse_info reports correct counts and bbox") {
    auto data  = make_minimal_sh();
    ShInfo inf = sh_parse_info(data.data(), data.size());
    REQUIRE(inf.vert_count == 3);
    REQUIRE(inf.face_count == 1);
    REQUIRE(inf.scale      == Catch::Approx(1.0f));
    REQUIRE(inf.scale_raw  == 8);
    // bbox refined from actual vertices
    REQUIRE(inf.bbox[0] == Catch::Approx(-10.0f)); // min X
    REQUIRE(inf.bbox[3] == Catch::Approx(10.0f));  // max X
}

TEST_CASE("sh_to_obj on empty mesh produces comment only") {
    ShMesh empty{};
    auto obj = sh_to_obj(empty);
    REQUIRE_FALSE(obj.empty());
    REQUIRE(obj.find("# Generated") != std::string::npos);
}

TEST_CASE("sh_to_obj on minimal mesh contains vertex and face lines") {
    auto data = make_minimal_sh();
    ShMesh m  = sh_parse_mesh(data.data(), data.size());
    auto obj  = sh_to_obj(m);
    REQUIRE(obj.find("v ") != std::string::npos);
    REQUIRE(obj.find("\nf ") != std::string::npos);
}

// ---- state selection: LOD / detail / draw-order selectors ---------------

// Wraps arbitrary code-section bytes in the MZ/Phar-Lap shell the parser needs.
static std::vector<uint8_t> wrap_sh(const std::vector<uint8_t>& code) {
    const uint32_t PE_OFF   = 64;
    const uint32_t SEC_OFF  = PE_OFF + 24;
    const uint32_t CODE_OFF = SEC_OFF + 40;
    std::vector<uint8_t> buf(CODE_OFF + code.size(), 0);
    buf[0] = 'M'; buf[1] = 'Z';
    buf[0x3C] = (uint8_t)PE_OFF;
    buf[PE_OFF + 0] = 'P'; buf[PE_OFF + 1] = 'L';
    buf[PE_OFF + 4] = 0x4C; buf[PE_OFF + 5] = 0x01;
    buf[PE_OFF + 6] = 1;
    memcpy(buf.data() + SEC_OFF, "CODE", 4);
    buf[SEC_OFF + 8]  = (uint8_t)code.size();
    buf[SEC_OFF + 12] = 0x80;
    buf[SEC_OFF + 16] = (uint8_t)code.size();
    buf[SEC_OFF + 20] = (uint8_t)(CODE_OFF & 0xFF);
    buf[SEC_OFF + 21] = (uint8_t)((CODE_OFF >> 8) & 0xFF);
    memcpy(buf.data() + CODE_OFF, code.data(), code.size());
    return buf;
}

static void put16(std::vector<uint8_t>& v, size_t at, uint16_t x) {
    v[at] = (uint8_t)(x & 0xFF); v[at + 1] = (uint8_t)(x >> 8);
}

// header + one conditional guard + fine block (VB + face colour 7 + ShortEOF)
// + coarse block (VB + face colour 9 + EndShape). `guard_op` = 0xC8 or 0xA6.
static std::vector<uint8_t> make_two_level_sh(uint8_t guard_op) {
    std::vector<uint8_t> c(14, 0);
    c[0] = 0xFF; c[1] = 0xFF; c[6] = 8;               // header, scale 8
    c[8] = 10; c[10] = 10; c[12] = 10;
    size_t guard = c.size();
    if (guard_op == 0xC8) {
        c.insert(c.end(), {0xC8, 0x00, 0,0, 0,0, 0,0}); // size/thr/rel patched below
        put16(c, guard + 4, 20);                        // pixel threshold = 20
    } else {
        c.insert(c.end(), {0xA6, 0x00, 0,0, 0,0});      // rel/thr patched below
        put16(c, guard + 4, 1);                         // detail threshold = 1
    }
    // fine block
    std::vector<uint8_t> vb = {0x82, 0x00, 3, 0, 0, 0,
        10,0, 0,0, 0,0,  (uint8_t)0xF6,(uint8_t)0xFF, 0,0, 0,0,  0,0, 10,0, 0,0};
    c.insert(c.end(), vb.begin(), vb.end());
    c.insert(c.end(), {0xFC, 0x00, 0x00, 7, 0x00, 3, 0, 1, 2});  // colour 7
    c.push_back(0x1E);                                            // ShortEOF
    size_t coarse = c.size();
    c.insert(c.end(), vb.begin(), vb.end());
    c.insert(c.end(), {0xFC, 0x00, 0x00, 9, 0x00, 3, 0, 1, 2});  // colour 9
    c.push_back(0x01);                                            // EndShape
    // patch the guard's branch to the coarse block (rel from end of operand)
    if (guard_op == 0xC8) put16(c, guard + 6, (uint16_t)(coarse - (guard + 8)));
    else                  put16(c, guard + 2, (uint16_t)(coarse - (guard + 6)));
    return wrap_sh(c);
}

TEST_CASE("sh_parse_mesh selects the LOD level via ShState::lod") {
    auto data = make_two_level_sh(0xC8);
    ShMesh fine = sh_parse_mesh(data.data(), data.size());
    REQUIRE(fine.lod_count == 2);
    REQUIRE(fine.faces.size() == 1);
    REQUIRE((int)fine.faces[0].color == 7);      // fell through to the fine block
    ShState st; st.lod = 1;
    ShMesh coarse = sh_parse_mesh(data.data(), data.size(), st);
    REQUIRE(coarse.lod_count == 2);
    REQUIRE(coarse.faces.size() == 1);
    REQUIRE((int)coarse.faces[0].color == 9);    // took the JumpToLOD branch
}

TEST_CASE("sh_parse_mesh honours the JumpToDetail preference") {
    auto data = make_two_level_sh(0xA6);
    ShMesh full = sh_parse_mesh(data.data(), data.size());
    REQUIRE(full.has_detail);
    REQUIRE(full.faces.size() == 1);
    REQUIRE((int)full.faces[0].color == 7);      // default detail >= threshold
    ShState st; st.detail = 0;
    ShMesh low = sh_parse_mesh(data.data(), data.size(), st);
    REQUIRE(low.faces.size() == 1);
    REQUIRE((int)low.faces[0].color == 9);       // detail < threshold branches
}

TEST_CASE("sh_parse_mesh renders both chains of a 0x6C draw-order selector") {
    // header + VB + [6C: call A, continue B] + B-face(colour 7) + ShortEOF
    // + A-fragment (face colour 5 + ShortEOF)
    std::vector<uint8_t> c(14, 0);
    c[0] = 0xFF; c[1] = 0xFF; c[6] = 8; c[8] = 10; c[10] = 10; c[12] = 10;
    std::vector<uint8_t> vb = {0x82, 0x00, 3, 0, 0, 0,
        10,0, 0,0, 0,0,  (uint8_t)0xF6,(uint8_t)0xFF, 0,0, 0,0,  0,0, 10,0, 0,0};
    c.insert(c.end(), vb.begin(), vb.end());
    size_t sel = c.size();
    c.insert(c.end(), {0x6C, 0x00, 0,0, 0,0, 0,0, 0,0, 0x38, 0,0}); // 13 bytes
    size_t bface = c.size();
    c.insert(c.end(), {0xFC, 0x00, 0x00, 7, 0x00, 3, 0, 1, 2});
    c.push_back(0x1E);
    size_t afrag = c.size();
    c.insert(c.end(), {0xFC, 0x00, 0x00, 5, 0x00, 3, 0, 1, 2});
    c.push_back(0x1E);
    put16(c, sel + 8, (uint16_t)(afrag - (sel + 2) - 8));  // relA: call target
    put16(c, sel + 6, (uint16_t)(bface - (sel + 2) - 6));  // relB: continue
    auto data = wrap_sh(c);
    ShMesh m = sh_parse_mesh(data.data(), data.size());
    REQUIRE(m.faces.size() == 2);                 // both chains rendered
    int c0 = m.faces[0].color, c1 = m.faces[1].color;
    REQUIRE(((c0 == 5 && c1 == 7) || (c0 == 7 && c1 == 5)));
}

TEST_CASE("frame selection emits exactly one frame's block") {
    // header + VB + JumpToFrame(2) + frame0(face 7, Jump join) + frame1(face 9)
    // + join: ShortEOF
    std::vector<uint8_t> c(14, 0);
    c[0] = 0xFF; c[1] = 0xFF; c[6] = 8; c[8] = 10; c[10] = 10; c[12] = 10;
    std::vector<uint8_t> vb = {0x82, 0x00, 3, 0, 0, 0,
        10,0, 0,0, 0,0,  (uint8_t)0xF6,(uint8_t)0xFF, 0,0, 0,0,  0,0, 10,0, 0,0};
    c.insert(c.end(), vb.begin(), vb.end());
    size_t ftab = c.size();
    c.insert(c.end(), {0x40, 0x00, 2, 0, 0,0, 0,0});       // 2 frames, slots patched
    size_t f0 = c.size();
    c.insert(c.end(), {0xFC, 0x00, 0x00, 7, 0x00, 3, 0, 1, 2});
    size_t jmp = c.size();
    c.insert(c.end(), {0x48, 0x00, 0, 0});                  // Jump -> join
    size_t f1 = c.size();
    c.insert(c.end(), {0xFC, 0x00, 0x00, 9, 0x00, 3, 0, 1, 2});
    size_t join = c.size();
    c.push_back(0x1E);
    put16(c, ftab + 4, (uint16_t)(f0 - (ftab + 4)));        // slot 0 rel
    put16(c, ftab + 6, (uint16_t)(f1 - (ftab + 6)));        // slot 1 rel
    put16(c, jmp + 2, (uint16_t)(join - (jmp + 4)));        // frame0 skips frame1
    auto data = wrap_sh(c);
    ShMesh m0 = sh_parse_mesh(data.data(), data.size());
    REQUIRE(m0.frame_count == 2);
    REQUIRE(m0.faces.size() == 1);
    REQUIRE((int)m0.faces[0].color == 7);         // frame 0 only, not the union
    ShState st; st.frame = 1;
    ShMesh m1 = sh_parse_mesh(data.data(), data.size(), st);
    REQUIRE(m1.faces.size() == 1);
    REQUIRE((int)m1.faces[0].color == 9);
}

TEST_CASE("sh_variant_name derives the engine's sibling names") {
    REQUIRE(sh_variant_name("A10.SH", 'a') == "A10_A.SH");
    REQUIRE(sh_variant_name("A10.SH", 'd') == "A10_D.SH");
    REQUIRE(sh_variant_name("A10.SH", 's') == "A10_S.SH");
    REQUIRE(sh_variant_name("a10.sh", 'A') == "a10_a.SH");   // case follows the stem
    REQUIRE(sh_variant_name("F15E.SH", 'c') == "F15E_C.SH");
    REQUIRE(sh_variant_name("NOEXT", 'a') == "NOEXT_A.SH");  // extension optional
    REQUIRE(sh_variant_name("A10.SH", 'x').empty());          // invalid letter
    REQUIRE(sh_variant_name(".SH", 'a').empty());             // empty stem
}

TEST_CASE("has_damage reports an inline JumpToDamage branch") {
    // header + [AC -> damaged] + intact face(colour 7) + ShortEOF
    //        + damaged face(colour 9) + EndShape
    std::vector<uint8_t> c(14, 0);
    c[0] = 0xFF; c[1] = 0xFF; c[6] = 8; c[8] = 10; c[10] = 10; c[12] = 10;
    std::vector<uint8_t> vb = {0x82, 0x00, 3, 0, 0, 0,
        10,0, 0,0, 0,0,  (uint8_t)0xF6,(uint8_t)0xFF, 0,0, 0,0,  0,0, 10,0, 0,0};
    c.insert(c.end(), vb.begin(), vb.end());
    size_t guard = c.size();
    c.insert(c.end(), {0xAC, 0x00, 0, 0});
    c.insert(c.end(), {0xFC, 0x00, 0x00, 7, 0x00, 3, 0, 1, 2});
    c.push_back(0x1E);
    size_t dmg = c.size();
    c.insert(c.end(), {0xFC, 0x00, 0x00, 9, 0x00, 3, 0, 1, 2});
    c.push_back(0x01);
    put16(c, guard + 2, (uint16_t)(dmg - (guard + 4)));
    auto data = wrap_sh(c);

    ShMesh intact = sh_parse_mesh(data.data(), data.size());
    REQUIRE(intact.has_damage);
    REQUIRE(intact.faces.size() == 1);
    REQUIRE((int)intact.faces[0].color == 7);
    ShState st; st.destroyed = true;
    ShMesh wreck = sh_parse_mesh(data.data(), data.size(), st);
    REQUIRE(wreck.faces.size() == 1);
    REQUIRE((int)wreck.faces[0].color == 9);

    // and a shape without 0xAC reports no inline damage
    auto plain = make_two_level_sh(0xC8);
    REQUIRE_FALSE(sh_parse_mesh(plain.data(), plain.size()).has_damage);
}
