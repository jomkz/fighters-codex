#include "fx/dlg.h"
#include "fx/cam.h"

namespace fx {

// Same PL container family as CAM — delegate rather than duplicate. The
// structural record-table decode is deferred to #54 (see dlg.h).

DlgInfo dlg_info(const uint8_t* data, size_t size) {
    CamInfo c = cam_info(data, size);
    return DlgInfo{ c.valid, c.code };
}

std::vector<std::string> dlg_strings(const uint8_t* data, size_t size,
                                     size_t min_len) {
    return cam_strings(data, size, min_len);
}

// A dialog says what it is MADE OF in its import table: every .DLG imports the engine's own
// drawing functions from main.dll (the game executable), one per control type it uses. So the
// controls are a decoded fact, not a guess -- and every name is one we should have claimed.
std::vector<PeImport> dlg_imports(const uint8_t* data, size_t size) {
    return pe_imports(data, size);
}

} // namespace fx
