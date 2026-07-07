// Fuzz target: the SH 3D shape parser (fx sh info/obj over untrusted bytes).
// sh_parse_mesh walks the shape bytecode allocating vertices/faces; the
// header-derived counts from sh_parse_info bound the mesh parse so a hostile
// count cannot force a multi-GB allocation past the fuzzer's malloc limit.

#include <cstddef>
#include <cstdint>
#include <string>

#include <fx/sh.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    fx::ShInfo info = fx::sh_parse_info(data, size);
    if (info.vert_count < 0 || info.face_count < 0 ||
        (long)info.vert_count + (long)info.face_count > (1L << 20))
        return 0;  // skip absurd headers; keep allocations under the limit

    fx::ShMesh mesh = fx::sh_parse_mesh(data, size);
    std::string obj = fx::sh_to_obj(mesh);
    volatile size_t sink = obj.size() ^ mesh.vertices.size() ^ mesh.faces.size();
    (void)sink;
    return 0;
}
