#pragma once
#include <string>
class App;
void DrawPreview(App& app);

// Select the SH preview's render backend before any frame is drawn: true =
// the FA-faithful software rasteriser (fx_render::fa, #290), false = OpenGL.
// The interactive toggle in the preview panel changes it later either way.
void PreviewForceSoftwareBackend(bool on);
void PreviewSetArticulation(const std::string& input, int value);

// Release the preview's GPU resources (fx_render renderer/target, image
// texture). Must be called while the GL context is still current, before
// shutdown — otherwise the static objects' destructors would delete GL handles
// on a dead context.
void PreviewShutdown();
