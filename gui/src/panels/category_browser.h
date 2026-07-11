#pragma once
class App;

// The fxs left panel (#364): an icon navigation bar over the workspace category
// browsers plus the raw per-LIB Archives view. Replaces the direct
// DrawLibBrowser call in App::Draw.
void DrawLeftPanel(App& app);
