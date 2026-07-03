#pragma once

namespace platform {

// Load the UI font into the ImGui atlas: first hit from a per-OS probe list
// of monospace system fonts (Consolas/Tahoma on Windows, DejaVu/Liberation
// on Linux), falling back to ImGui's embedded default. Call once after
// ImGui::CreateContext. With ImGui 1.92 dynamic fonts the size is a base
// size; DPI scaling is applied via style.FontScaleDpi.
void LoadFonts(float sizePx);

} // namespace platform
