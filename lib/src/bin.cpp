#include "fx/bin.h"
#include <cctype>

namespace fx {

BinKind bin_classify(const std::string& entry_name) {
    std::string stem;
    for (char c : entry_name) stem += (char)toupper((unsigned char)c);
    size_t dot = stem.rfind('.');
    if (dot != std::string::npos && stem.substr(dot) == ".BIN")
        stem.resize(dot);

    if (stem == "INSIGMAP") return BinKind::Insigmap;
    if (stem == "MIX2")     return BinKind::Mix2;
    if (stem == "MIX2L")    return BinKind::Mix2L;
    if (stem == "MIX4")     return BinKind::Mix4;
    if (stem == "MIX4L")    return BinKind::Mix4L;
    if (stem == "VFONTPAL") return BinKind::VFontPal;
    return BinKind::Unknown;
}

const char* bin_kind_desc(BinKind kind) {
    switch (kind) {
        case BinKind::Insigmap:
            return "Insignia slot map (256 entries; 0x3B = no insignia)";
        case BinKind::Mix2:
            return "Half-intensity blend table, gamma-corrected (512 bytes)";
        case BinKind::Mix2L:
            return "Half-intensity blend table, linear i/2 (512 bytes)";
        case BinKind::Mix4:
            return "Quarter-intensity blend table, gamma-corrected (1024 bytes)";
        case BinKind::Mix4L:
            return "Quarter-intensity blend table, linear i/4 (1024 bytes)";
        case BinKind::VFontPal:
            return "16-color VGA font palette for video briefing text (48 bytes)";
        case BinKind::Unknown:
            break;
    }
    return "Unknown table";
}

size_t bin_expected_size(BinKind kind) {
    switch (kind) {
        case BinKind::Insigmap: return 256;
        case BinKind::Mix2:     return 512;
        case BinKind::Mix2L:    return 512;
        case BinKind::Mix4:     return 1024;
        case BinKind::Mix4L:    return 1024;
        case BinKind::VFontPal: return 48;
        case BinKind::Unknown:  break;
    }
    return 0;
}

} // namespace fx
