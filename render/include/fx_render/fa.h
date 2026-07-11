// fx_render::fa — the FA-faithful software path (fx_render #290).
//
// Reproduces the game's GG_/G_ pixel model from this project's own
// documentation (docs/fa/renderer.md, docs/fa/render-core.md): an 8-bit
// indexed surface with the documented bitmap record's semantics, the
// 192-entry 6-bit VGA palette, and the G_* raster-state block. Draw entry
// points grow with the #290 sub-issues (spans, shading, clipping, painter's
// order, textures).
//
// Dimensions are runtime parameters throughout: FA.EXE's 1024x768 ceiling is
// a GG_/DirectDraw *device* limit, not a property of the G_ algorithms, and
// the fa path does not inherit it (guarded by the resolution-independence
// tests).
//
// MIT, clean-room from this project's own docs; see NOTICE.
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

namespace fx_render {

struct Image;  // fx_render/render.h — RGBA8 presentation target

namespace fa {

// Fixed-point 16.16 screen coordinates — the integer rasteriser's number
// format (renderer.md §3.1: five-word vertices [x, y, u, v, c], all 16.16).
using Fx = std::int32_t;
inline constexpr Fx kFxOne = 1 << 16;
constexpr Fx ToFx(int pixels) { return static_cast<Fx>(pixels) * kFxOne; }
constexpr int FxFloor(Fx v) { return static_cast<int>(v >> 16); }

// 8-bit indexed render surface mirroring the documented bitmap record
// (renderer.md §7: width, height, row stride, row-pointer table). The row
// pointers are the access path — as in the original, where the rasteriser
// walks `_cb + 0x22` rather than recomputing offsets.
class Surface {
public:
    Surface(int width, int height);              // stride == width
    Surface(int width, int height, int stride);  // stride >= width

    int width() const { return width_; }
    int height() const { return height_; }
    int stride() const { return stride_; }

    std::uint8_t* row(int y) { return rows_[static_cast<std::size_t>(y)]; }
    const std::uint8_t* row(int y) const { return rows_[static_cast<std::size_t>(y)]; }

    void Clear(std::uint8_t index);

private:
    int width_ = 0, height_ = 0, stride_ = 0;
    std::vector<std::uint8_t> pixels_;
    std::vector<std::uint8_t*> rows_;
};

// The 192-entry palette of 6-bit VGA components (renderer.md key globals:
// `_realPalette` / `_curPalette`, 0xC0 entries — base + sky range).
struct Palette {
    struct Rgb6 {
        std::uint8_t r = 0, g = 0, b = 0;  // 0..63
    };
    static constexpr int kEntries = 192;
    std::array<Rgb6, kEntries> entries{};

    // Expand the indexed surface to RGBA8 (top-left origin) for display —
    // the fa equivalent of the GG_ present. 6-bit components widen with the
    // conventional VGA expansion (c << 2 | c >> 4); indices past the palette
    // range present as black (inferred: the engine never emits them).
    void Present(const Surface& src, Image& out) const;

    // Nearest palette index to an 8-bit RGB colour by squared distance — the
    // G_RemapBitmapToPalette operation, and the bridge for callers that only
    // have true-colour input.
    std::uint8_t Nearest(std::uint8_t r8, std::uint8_t g8, std::uint8_t b8) const;
};

// Fill types per `_cFillType` (renderer.md key globals: 0 = flat, 1 = shaded,
// 2+ = textured).
enum class FillType : std::uint8_t {
    Flat = 0,
    Shaded = 1,
    Textured = 2,
};

// A polygon vertex — the documented five-word 16.16 record
// (renderer.md §3.1: [x_fp, y_fp, u_fp, v_fp, c_packed]). The span core
// (#329) consumes x/y; c feeds the Gouraud path (#330) and u/v the textured
// fills (#333).
struct PolyVertex {
    Fx x = 0, y = 0;
    Fx u = 0, v = 0;
    Fx c = 0;
};

// UPolygonToYLR output: the polygon reduced to per-scanline Left/Right span
// bounds (16.16 screen x), one entry per scanline from `y_top`, with the
// packed shade term `c` (#330) and the texel coordinates u/v (#333) carried
// along each edge.
struct YlrList {
    int y_top = 0;
    std::vector<Fx> left, right;      // span bounds, 16.16 x
    std::vector<Fx> left_c, right_c;  // c_packed at each bound, 16.16
    std::vector<Fx> left_u, right_u;  // u texels at each bound, 16.16
    std::vector<Fx> left_v, right_v;  // v texels at each bound, 16.16
    int height() const { return static_cast<int>(left.size()); }
};

// Palette index 0xFF is the transparent key in FA skins (PIC.md); the textured
// span filler shows the polygon's flat shade colour through such texels.
inline constexpr std::uint8_t kTransparentTexel = 0xFF;

// An indexed texture — PIC pixels are palette indices (renderer.md §9).
// `u, v` sample in 16.16 texel coordinates, truncating and clamping to the
// edges (inferred).
struct Texture {
    int width = 0, height = 0;
    std::vector<std::uint8_t> texels;  // width * height, row-major
    std::uint8_t at(Fx u, Fx v) const;
};

// FVERTEX after projection (renderer.md §3.2): screen position, texel u/v,
// and rw = 1/w — the reciprocal the perspective kernel interpolates.
struct FVertex {
    float x = 0, y = 0;
    float u = 0, v = 0;
    float rw = 1;
};

// FVERTEX-style pre-projection vertex for the near-plane stage
// (renderer.md §3.2): camera-space position with `w` the depth divisor,
// plus the attributes the fa fills consume after projection.
struct ClipVertex {
    float x = 0, y = 0, z = 0, w = 1;
    float u = 0, v = 0, c = 0;
};

// The near guard bit of the per-vertex clip-flag word (FVERTEX bit 2).
inline constexpr unsigned kClipNear = 0x4;

// code_pnt — the outcode for one vertex against the near plane `w = near_w`.
unsigned CodePnt(const ClipVertex& v, float near_w);

// The NPM near-plane scheme (renderer.md §3.2): the AND of the outcodes
// trivially rejects, an OR of zero trivially accepts, and a straddling
// polygon is clipped at the plane with attributes interpolated — clipped,
// never dropped. Writes up to n + 1 vertices to `out`; returns the count
// (0 = fully behind the plane).
int NearClipPolygon(const ClipVertex* in, int n, float near_w, ClipVertex* out);

// The G_* raster-state block over one target surface: the active clip box
// (G_Init / G_SetClipBox), the current colour (`_cColor`), and the current
// fill type (`_cFillType`). Construction is G_Init: full-surface clip box.
// Clip bounds are inclusive pixel coordinates (inferred from the span
// filler's `_no_overlap` flag making the *right* edge exclusive only as an
// option — the default edge is inclusive).
class Raster {
public:
    explicit Raster(Surface& target);

    // G_SetClipBox — clamped to the surface bounds.
    void SetClipBox(int left, int top, int right, int bottom);
    int clip_left() const { return clip_left_; }
    int clip_top() const { return clip_top_; }
    int clip_right() const { return clip_right_; }
    int clip_bottom() const { return clip_bottom_; }

    void SetColor(std::uint8_t index) { color_ = index; }  // _cColor
    std::uint8_t color() const { return color_; }
    void SetFillType(FillType t) { fill_type_ = t; }  // _cFillType
    FillType fill_type() const { return fill_type_; }

    // G_Point — plot a single clipped pixel in the current colour.
    void Point(int x, int y);

    // G_Rect — filled rectangle (inclusive corners), clipped to the clip box.
    void Rect(int left, int top, int right, int bottom);

    // `_no_overlap` — when set, a span's right edge is exclusive, so
    // horizontally abutting polygons paint every seam pixel exactly once.
    void SetNoOverlap(bool on) { no_overlap_ = on; }
    bool no_overlap() const { return no_overlap_; }

    // UPolygonToYLR — reduce a convex polygon to its Y/Left/Right scanline
    // list. Stepping conventions (inferred; see renderer.md §3.1): edge x is
    // evaluated at integer scanlines from the edge's 16.16 slope, and a
    // polygon's vertical coverage is half-open [ceil(y_min), ceil(y_max)),
    // so vertically abutting polygons never overdraw. Returns false for
    // degenerate input (fewer than 3 vertices, or an empty scanline range).
    static bool PolygonToYlr(const PolyVertex* v, int n, YlrList& out);

    // G_UPolygon — fill a convex polygon with the current flat colour.
    // Span endpoints truncate the 16.16 bounds ([left >> 16, right >> 16],
    // inclusive unless `_no_overlap`), then clamp to the clip box.
    void UPolygon(const PolyVertex* v, int n);

    // G_SUPolygon — the shaded (Gouraud) variant: the packed vertex term `c`
    // interpolates along the edges and across each span with the same 16.16
    // stepping, and each pixel writes its interpolated index (clamped to the
    // palette range — inferred; renderer.md §3.1).
    void SUPolygon(const PolyVertex* v, int n);

    // Fill-type dispatch (the `_fillTypes` selection): Flat uses `_cColor`
    // via UPolygon, Shaded interpolates via SUPolygon — the flat-vs-Gouraud
    // choice the SH interpreter stages through `sh_op_80`/`SetFlatColor`
    // (render-core.md). Textured falls back to flat until #333.
    void Polygon(const PolyVertex* v, int n);

    // G_AcTexture — bind the active texture (null unbinds; the Textured
    // fill type then falls back to flat).
    void SetTexture(const Texture* tex) { texture_ = tex; }
    const Texture* texture() const { return texture_; }

    // SetTmapRemaps / `_tmapRemapTable` — the 256-entry texel-index →
    // screen-index remap (64 dwords in the original) applying the current
    // shade/tint. Null restores the identity mapping.
    void SetTmapRemap(const std::uint8_t* table256);

    // G__Texture — the affine textured span fill: u/v ride the five-word
    // vertex through the same 16.16 edge/span stepping as c, sampled per
    // pixel and pushed through the tmap remap.
    void TexturedUPolygon(const PolyVertex* v, int n);

    // The NPM float triangle kernels (renderer.md §3.2), on the bound
    // texture. Linear interpolates u/v directly in screen space
    // (NPM_TextureLinearTri); Perspective interpolates u·rw, v·rw, rw and
    // divides per pixel (NPM_TexturePerspectiveTri, carefulDiv-guarded).
    void TextureTriLinear(const FVertex t[3]);
    void TextureTriPerspective(const FVertex t[3]);

    // Sutherland–Hodgman clip of a convex polygon against the clip box, in
    // the render-core edge order (`clip_edge_left/right/top/bottom`), with
    // the vertex attributes (u, v, c) interpolated at each crossing.
    // Returns the number of vertices appended to `out` (0 = fully outside).
    int ClipPolygon(const PolyVertex* in, int n, std::vector<PolyVertex>& out) const;

    // G_Polygon / G_SPolygon — the clipped entries: Sutherland–Hodgman
    // against the clip box, then the current fill-type dispatch.
    void PolygonClipped(const PolyVertex* v, int n);

    // G_ClipLine — Cohen–Sutherland segment clip against the clip box
    // (inclusive integer pixel bounds). False = trivially rejected.
    bool ClipLine(int& x0, int& y0, int& x1, int& y1) const;

    // G_Line — clipped line in the current colour.
    void Line(int x0, int y0, int x1, int y1);

    Surface& target() { return *target_; }

private:
    void FillYlrFlat(const YlrList& ylr);      // the _G_DrawYLR_4 flat loop
    void FillYlrShaded(const YlrList& ylr);    // the Gouraud span loop
    void FillYlrTextured(const YlrList& ylr);  // the affine G__Texture loop
    void TextureTriScan(const FVertex t[3], bool perspective);

    Surface* target_;
    int clip_left_ = 0, clip_top_ = 0, clip_right_ = 0, clip_bottom_ = 0;
    std::uint8_t color_ = 0;
    FillType fill_type_ = FillType::Flat;
    bool no_overlap_ = false;
    const Texture* texture_ = nullptr;
    std::array<std::uint8_t, 256> tmap_remap_{};
    bool remap_identity_ = true;
};

// The painter's-order submission list — the fa path's only occlusion
// mechanism. There is no depth buffer anywhere in this pipeline
// (renderer.md §3.3): objects queue with the documented centroid+size sort
// key (`GRAddBrentObj` into `sort_objs_wrapper`, render-core.md) and flush
// back-to-front; face order *within* an object stays exactly as submitted
// (the SH interpreter's draw-order selectors already encode it).
class PaintersList {
public:
    struct Key {
        Fx depth = 0;  // view-space centroid depth (farther = greater), 16.16
        Fx size = 0;   // object extent/radius, 16.16
    };

    // Back-to-front: greater composite depth+size draws first; the exact
    // combination of the documented centroid+size pair is inferred (larger
    // objects bias deeper). Equal keys keep submission order (stable).
    static bool DrawsBefore(const Key& a, const Key& b);

    // GRAddBrentObj — queue one object's draw under its sort key.
    void Add(Key key, std::function<void(Raster&)> draw);

    // sort_objs_wrapper + draw pass: stable back-to-front sort, run each
    // object's draw in order, then clear the list.
    void Flush(Raster& r);

    int size() const { return static_cast<int>(items_.size()); }

private:
    struct Item {
        Key key;
        std::function<void(Raster&)> draw;
    };
    std::vector<Item> items_;
};

}  // namespace fa
}  // namespace fx_render
