#pragma once
class App;
void DrawPreview(App& app);

// Release the preview's GPU resources (fx_render renderer/target, image
// texture). Must be called while the GL context is still current, before
// shutdown — otherwise the static objects' destructors would delete GL handles
// on a dead context.
void PreviewShutdown();
