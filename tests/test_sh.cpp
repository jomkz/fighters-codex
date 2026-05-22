#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <ft/sh.h>
#include <cstring>
#include <vector>

using namespace ft;

// Build a minimal valid SH binary containing one VertexBuffer (3 verts) and
// one triangular Face, then EndShape.  Structure:
//   MZ stub (64 B) → Phar-Lap PE header (24 B) → section table (40 B)
//   → CODE section (48 B)
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
