#define NOMINMAX
#include "preview.h"
#include "../app.h"
#include "imgui.h"
#include "fx/pic.h"
#include "fx/pal.h"
#include "fx/ealib.h"
#include "fx/raw.h"
#include "fx/sh.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <vector>

using namespace DirectX;

// ---------------------------------------------------------------------------
// Image preview (PIC / RAW)
// ---------------------------------------------------------------------------

static GpuTexture s_preview;
static int        s_previewLib   = -2;
static int        s_previewEntry = -2;

// Locate PALETTE.PAL across all open sessions. Returns a loaded Palette,
// or a greyscale fallback if not found.
static fx::Palette FindSysPalette(const App& app) {
    for (const auto& sess : app.sessions) {
        if (const fx::Entry* entry = fx::ealib_find(sess.entries, "PALETTE.PAL")) {
            auto raw = fx::ealib_extract(sess.data.data(), sess.data.size(),
                                         *entry);
            if (!raw.empty())
                return fx::pal_load(raw.data(), raw.size());
        }
    }
    return fx::pal_load(nullptr, 0); // greyscale fallback
}

// ---------------------------------------------------------------------------
// SH 3D preview
// ---------------------------------------------------------------------------

struct Vtx3D { float x, y, z, r, g, b; };

static const char* kVS = R"hlsl(
cbuffer CB : register(b0) { float4x4 mvp; };
struct VI { float3 pos : POSITION; float3 col : TEXCOORD0; };
struct VO { float4 pos : SV_POSITION; float3 col : TEXCOORD0; };
VO main(VI v) {
    VO o;
    o.pos = mul(mvp, float4(v.pos, 1.0f));
    o.col = v.col;
    return o;
}
)hlsl";

static const char* kPS = R"hlsl(
struct PI { float4 pos : SV_POSITION; float3 col : TEXCOORD0; };
float4 main(PI p) : SV_TARGET { return float4(p.col, 1.0f); }
)hlsl";

static const char* kPS_wire = R"hlsl(
struct PI { float4 pos : SV_POSITION; float3 col : TEXCOORD0; };
float4 main(PI p) : SV_TARGET { return float4(0.7f, 0.7f, 0.7f, 1.0f); }
)hlsl";

struct ShPreview {
    ID3D11Texture2D*           colorTex  = nullptr;
    ID3D11RenderTargetView*    rtv       = nullptr;
    ID3D11ShaderResourceView*  srv       = nullptr;
    ID3D11Texture2D*           depthTex  = nullptr;
    ID3D11DepthStencilView*    dsv       = nullptr;
    ID3D11VertexShader*        vs        = nullptr;
    ID3D11PixelShader*         ps        = nullptr;
    ID3D11InputLayout*         il        = nullptr;
    ID3D11Buffer*              vb        = nullptr;
    ID3D11Buffer*              grid_vb   = nullptr;
    ID3D11Buffer*              cb        = nullptr;
    ID3D11RasterizerState*     rs        = nullptr;
    ID3D11RasterizerState*     rs_wire   = nullptr;
    ID3D11PixelShader*         ps_wire   = nullptr;
    ID3D11DepthStencilState*   dss       = nullptr;
    ID3D11DepthStencilState*   dss_wire  = nullptr;
    int vtx_count   = 0;
    int grid_vtx_count = 0;
    int rt_w = 0, rt_h = 0;
    int cached_lib   = -2;
    int cached_entry = -2;
    float azimuth    = 170.0f;
    float elevation  = 20.0f;
    float distance   = 100.0f;
    float model_span = 1.0f;
    float target[3]  = {};
    bool  shaders_ok = false;

    void ReleaseRT() {
        if (colorTex)  { colorTex->Release();  colorTex  = nullptr; }
        if (rtv)       { rtv->Release();       rtv       = nullptr; }
        if (srv)       { srv->Release();       srv       = nullptr; }
        if (depthTex)  { depthTex->Release();  depthTex  = nullptr; }
        if (dsv)       { dsv->Release();       dsv       = nullptr; }
        rt_w = rt_h = 0;
    }
    void ReleaseMesh() {
        if (vb)      { vb->Release();      vb      = nullptr; }
        if (grid_vb) { grid_vb->Release(); grid_vb = nullptr; }
        vtx_count = grid_vtx_count = 0;
    }
    void ReleaseAll() {
        ReleaseRT();
        ReleaseMesh();
        if (vs)  { vs->Release();  vs  = nullptr; }
        if (ps)  { ps->Release();  ps  = nullptr; }
        if (il)  { il->Release();  il  = nullptr; }
        if (cb)  { cb->Release();  cb  = nullptr; }
        if (rs)       { rs->Release();       rs       = nullptr; }
        if (rs_wire)  { rs_wire->Release();  rs_wire  = nullptr; }
        if (ps_wire)  { ps_wire->Release();  ps_wire  = nullptr; }
        if (dss)      { dss->Release();      dss      = nullptr; }
        if (dss_wire) { dss_wire->Release(); dss_wire = nullptr; }
        shaders_ok = false;
    }
} s_sh;

static bool EnsureShaders(ID3D11Device* device) {
    if (s_sh.shaders_ok) return true;

    ID3DBlob* vsBlob = nullptr, *psBlob = nullptr, *errBlob = nullptr;

    if (FAILED(D3DCompile(kVS, strlen(kVS), "vs", nullptr, nullptr,
                          "main", "vs_4_0", 0, 0, &vsBlob, &errBlob))) {
        if (errBlob) errBlob->Release();
        return false;
    }
    if (FAILED(D3DCompile(kPS, strlen(kPS), "ps", nullptr, nullptr,
                          "main", "ps_4_0", 0, 0, &psBlob, &errBlob))) {
        vsBlob->Release();
        if (errBlob) errBlob->Release();
        return false;
    }

    device->CreateVertexShader(vsBlob->GetBufferPointer(),
                               vsBlob->GetBufferSize(), nullptr, &s_sh.vs);
    device->CreatePixelShader (psBlob->GetBufferPointer(),
                               psBlob->GetBufferSize(), nullptr, &s_sh.ps);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    device->CreateInputLayout(layout, 2,
                              vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &s_sh.il);
    vsBlob->Release(); psBlob->Release();

    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth      = sizeof(XMFLOAT4X4);
    cbd.Usage          = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device->CreateBuffer(&cbd, nullptr, &s_sh.cb);

    ID3DBlob* wireBlob = nullptr;
    if (SUCCEEDED(D3DCompile(kPS_wire, strlen(kPS_wire), "ps_wire", nullptr, nullptr,
                             "main", "ps_4_0", 0, 0, &wireBlob, &errBlob)))
    {
        device->CreatePixelShader(wireBlob->GetBufferPointer(),
                                  wireBlob->GetBufferSize(), nullptr, &s_sh.ps_wire);
        wireBlob->Release();
    }

    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode        = D3D11_FILL_SOLID;
    rd.CullMode        = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    device->CreateRasterizerState(&rd, &s_sh.rs);

    rd.FillMode              = D3D11_FILL_WIREFRAME;
    rd.DepthBias             = -2000;
    rd.SlopeScaledDepthBias  = -1.0f;
    device->CreateRasterizerState(&rd, &s_sh.rs_wire);

    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable    = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsd.DepthFunc      = D3D11_COMPARISON_LESS;
    device->CreateDepthStencilState(&dsd, &s_sh.dss);

    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;
    device->CreateDepthStencilState(&dsd, &s_sh.dss_wire);

    s_sh.shaders_ok = s_sh.vs && s_sh.ps && s_sh.il && s_sh.cb && s_sh.rs && s_sh.dss;
    return s_sh.shaders_ok;
}

static bool EnsureRT(ID3D11Device* device, int w, int h) {
    if (s_sh.rt_w == w && s_sh.rt_h == h && s_sh.srv) return true;
    s_sh.ReleaseRT();
    if (w <= 0 || h <= 0) return false;

    D3D11_TEXTURE2D_DESC td = {};
    td.Width  = (UINT)w; td.Height = (UINT)h;
    td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage     = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(device->CreateTexture2D(&td, nullptr, &s_sh.colorTex))) return false;
    device->CreateRenderTargetView(s_sh.colorTex,    nullptr, &s_sh.rtv);
    device->CreateShaderResourceView(s_sh.colorTex,  nullptr, &s_sh.srv);

    td.Format    = DXGI_FORMAT_D24_UNORM_S8_UINT;
    td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(device->CreateTexture2D(&td, nullptr, &s_sh.depthTex))) {
        s_sh.ReleaseRT(); return false;
    }
    device->CreateDepthStencilView(s_sh.depthTex, nullptr, &s_sh.dsv);

    s_sh.rt_w = w; s_sh.rt_h = h;
    return s_sh.rtv && s_sh.srv && s_sh.dsv;
}

static void BuildMeshVB(ID3D11Device* device, const fx::ShMesh& mesh) {
    s_sh.ReleaseMesh();
    if (mesh.vertices.empty() || mesh.faces.empty()) return;

    std::vector<Vtx3D> verts;
    verts.reserve(mesh.faces.size() * 6);

    // Fixed directional light â€” upper-left-front in render space (X=right, Y=up, Z=fwd)
    const float Lx = 0.577f, Ly = 0.577f, Lz = -0.577f;
    const float ambient = 0.15f, diffuse = 0.85f;

    for (const auto& face : mesh.faces) {
        if (face.indices.size() < 3) continue;
        for (size_t i = 1; i + 1 < face.indices.size(); i++) {
            uint32_t i0 = face.indices[0];
            uint32_t i1 = face.indices[i];
            uint32_t i2 = face.indices[i + 1];
            // Skip entire triangle rather than pushing partial vertices (which misaligns the buffer)
            if (i0 >= mesh.vertices.size() || i1 >= mesh.vertices.size() || i2 >= mesh.vertices.size())
                continue;
            auto& v0 = mesh.vertices[i0];
            auto& v1 = mesh.vertices[i1];
            auto& v2 = mesh.vertices[i2];

            // Compute face normal in render space (SH Y/Z swapped: render Y=SH Z, render Z=SH Y)
            float ax = v1.x - v0.x, ay = v1.z - v0.z, az = v1.y - v0.y;
            float bx = v2.x - v0.x, by = v2.z - v0.z, bz = v2.y - v0.y;
            float nx = ay * bz - az * by;
            float ny = az * bx - ax * bz;
            float nz = ax * by - ay * bx;
            float len = sqrtf(nx*nx + ny*ny + nz*nz);
            float shade;
            if (len > 0.0f) {
                nx /= len; ny /= len; nz /= len;
                float diff = nx * Lx + ny * Ly + nz * Lz;
                shade = ambient + diffuse * std::max(0.0f, diff);
            } else {
                shade = ambient + diffuse * 0.3f;
            }

            // SH uses Z-up (X=right, Y=forward, Z=up); remap to DX Y-up
            verts.push_back({v0.x, v0.z, v0.y, shade, shade, shade});
            verts.push_back({v1.x, v1.z, v1.y, shade, shade, shade});
            verts.push_back({v2.x, v2.z, v2.y, shade, shade, shade});
        }
    }

    if (verts.empty()) return;

    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = (UINT)(verts.size() * sizeof(Vtx3D));
    bd.Usage     = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem = verts.data();
    device->CreateBuffer(&bd, &sd, &s_sh.vb);
    s_sh.vtx_count = (int)verts.size();
}

// Build a 6-wall room around the model.
// Room is a cube of half-size = max_span*2, centred on the model bbox centre.
// Camera orbits at <= max_span*1.6, so it is always INSIDE the room.
// All coordinates are in render space: X=FA_X, Y=FA_Z(up), Z=FA_Y(fwd).
static void BuildBoxGridVB(ID3D11Device* device, const fx::ShInfo& info, float max_span) {
    float cx = (info.bbox[0] + info.bbox[3]) * 0.5f;
    float cy = (info.bbox[2] + info.bbox[5]) * 0.5f; // render_Y = FA_Z centre
    float cz = (info.bbox[1] + info.bbox[4]) * 0.5f; // render_Z = FA_Y centre

    float half = max_span * 2.0f;
    float xlo = cx - half,  xhi = cx + half;
    float ylo = cy - half,  yhi = cy + half;
    float zlo = cz - half,  zhi = cz + half;

    // Step: ~8 divisions across the room so lines aren't too dense or sparse
    float raw  = (half * 2.0f) / 8.0f;
    float mag  = powf(10.0f, floorf(log10f(std::max(raw, 0.1f))));
    float step = ceilf(raw / mag) * mag;
    if (step < 1.0f) step = 1.0f;

    // Snap bounds to grid so corners land on lines
    xlo = floorf(xlo/step)*step;  xhi = ceilf(xhi/step)*step;
    ylo = floorf(ylo/step)*step;  yhi = ceilf(yhi/step)*step;
    zlo = floorf(zlo/step)*step;  zhi = ceilf(zhi/step)*step;

    std::vector<Vtx3D> lines;

    // Grid on one XZ face (constant Y)
    auto faceXZ = [&](float y, float c) {
        for (float x = xlo; x <= xhi+0.001f; x += step) {
            lines.push_back({x,y,zlo, c,c,c}); lines.push_back({x,y,zhi, c,c,c});
        }
        for (float z = zlo; z <= zhi+0.001f; z += step) {
            lines.push_back({xlo,y,z, c,c,c}); lines.push_back({xhi,y,z, c,c,c});
        }
    };
    // Grid on one YZ face (constant X)
    auto faceYZ = [&](float x, float c) {
        for (float y = ylo; y <= yhi+0.001f; y += step) {
            lines.push_back({x,y,zlo, c,c,c}); lines.push_back({x,y,zhi, c,c,c});
        }
        for (float z = zlo; z <= zhi+0.001f; z += step) {
            lines.push_back({x,ylo,z, c,c,c}); lines.push_back({x,yhi,z, c,c,c});
        }
    };
    // Grid on one XY face (constant Z)
    auto faceXY = [&](float z, float c) {
        for (float x = xlo; x <= xhi+0.001f; x += step) {
            lines.push_back({x,ylo,z, c,c,c}); lines.push_back({x,yhi,z, c,c,c});
        }
        for (float y = ylo; y <= yhi+0.001f; y += step) {
            lines.push_back({xlo,y,z, c,c,c}); lines.push_back({xhi,y,z, c,c,c});
        }
    };

    faceXZ(ylo, 0.22f);   // floor  â€” slightly brighter
    faceXZ(yhi, 0.14f);   // ceiling â€” dimmer
    faceYZ(xlo, 0.17f);   // left wall
    faceYZ(xhi, 0.17f);   // right wall
    faceXY(zlo, 0.17f);   // rear wall
    faceXY(zhi, 0.17f);   // front wall

    // 12 bright box edges so the outline is crisp
    const float ce = 0.45f;
    auto edge = [&](float ax,float ay,float az, float bx,float by,float bz){
        lines.push_back({ax,ay,az,ce,ce,ce}); lines.push_back({bx,by,bz,ce,ce,ce});
    };
    edge(xlo,ylo,zlo, xhi,ylo,zlo); edge(xhi,ylo,zlo, xhi,ylo,zhi);
    edge(xhi,ylo,zhi, xlo,ylo,zhi); edge(xlo,ylo,zhi, xlo,ylo,zlo);
    edge(xlo,yhi,zlo, xhi,yhi,zlo); edge(xhi,yhi,zlo, xhi,yhi,zhi);
    edge(xhi,yhi,zhi, xlo,yhi,zhi); edge(xlo,yhi,zhi, xlo,yhi,zlo);
    edge(xlo,ylo,zlo, xlo,yhi,zlo); edge(xhi,ylo,zlo, xhi,yhi,zlo);
    edge(xhi,ylo,zhi, xhi,yhi,zhi); edge(xlo,ylo,zhi, xlo,yhi,zhi);

    if (lines.empty()) return;
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = (UINT)(lines.size() * sizeof(Vtx3D));
    bd.Usage     = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA sd = { lines.data(), 0, 0 };
    if (SUCCEEDED(device->CreateBuffer(&bd, &sd, &s_sh.grid_vb)))
        s_sh.grid_vtx_count = (int)lines.size();
}

static void RenderSh(ID3D11Device* device, ID3D11DeviceContext* ctx, int w, int h) {
    if (!EnsureRT(device, w, h))      return;
    if (!EnsureShaders(device))       return;

    static const float kClear[4] = {0.08f, 0.08f, 0.12f, 1.0f};
    ctx->ClearRenderTargetView(s_sh.rtv, kClear);
    ctx->ClearDepthStencilView(s_sh.dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);

    if (!s_sh.vb || s_sh.vtx_count == 0) return; // empty model â€” just show dark bg

    // Build MVP
    float azRad = s_sh.azimuth   * XM_PI / 180.0f;
    float elRad = s_sh.elevation * XM_PI / 180.0f;
    float eyeX  = s_sh.target[0] + s_sh.distance * cosf(elRad) * sinf(azRad);
    float eyeY  = s_sh.target[1] + s_sh.distance * sinf(elRad);
    float eyeZ  = s_sh.target[2] + s_sh.distance * cosf(elRad) * cosf(azRad);

    XMVECTOR eye = XMVectorSet(eyeX, eyeY, eyeZ, 0);
    XMVECTOR tgt = XMVectorSet(s_sh.target[0], s_sh.target[1], s_sh.target[2], 0);
    XMVECTOR up  = XMVectorSet(0, 1, 0, 0);

    XMMATRIX view = XMMatrixLookAtLH(eye, tgt, up);
    float near_p = std::max(1.0f, s_sh.distance * 0.01f);
    float far_p  = s_sh.distance + s_sh.model_span * 10.0f;
    XMMATRIX proj = XMMatrixPerspectiveFovLH(
        60.0f * XM_PI / 180.0f,
        (float)w / (float)h,
        near_p, far_p);
    XMMATRIX mvp = XMMatrixTranspose(view * proj);

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    ctx->Map(s_sh.cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    XMStoreFloat4x4((XMFLOAT4X4*)mapped.pData, mvp);
    ctx->Unmap(s_sh.cb, 0);

    ctx->OMSetRenderTargets(1, &s_sh.rtv, s_sh.dsv);
    D3D11_VIEWPORT vp = {0, 0, (float)w, (float)h, 0.0f, 1.0f};
    ctx->RSSetViewports(1, &vp);
    ctx->VSSetShader(s_sh.vs, nullptr, 0);
    ctx->PSSetShader(s_sh.ps, nullptr, 0);
    ctx->IASetInputLayout(s_sh.il);
    ctx->VSSetConstantBuffers(0, 1, &s_sh.cb);
    ctx->RSSetState(s_sh.rs);
    ctx->OMSetDepthStencilState(s_sh.dss, 0);

    UINT stride = sizeof(Vtx3D), offset = 0;

    // Floor grid â€” draw first so model depth writes occlude it naturally
    if (s_sh.grid_vb && s_sh.grid_vtx_count > 0) {
        ctx->IASetVertexBuffers(0, 1, &s_sh.grid_vb, &stride, &offset);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        ctx->RSSetState(s_sh.rs);
        ctx->OMSetDepthStencilState(s_sh.dss, 0);
        ctx->Draw((UINT)s_sh.grid_vtx_count, 0);
    }

    ctx->IASetVertexBuffers(0, 1, &s_sh.vb, &stride, &offset);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->Draw((UINT)s_sh.vtx_count, 0);

    // Wireframe overlay â€” dark lines on top of the solid mesh
    if (s_sh.rs_wire && s_sh.ps_wire && s_sh.dss_wire) {
        ctx->RSSetState(s_sh.rs_wire);
        ctx->PSSetShader(s_sh.ps_wire, nullptr, 0);
        ctx->OMSetDepthStencilState(s_sh.dss_wire, 0);
        ctx->Draw((UINT)s_sh.vtx_count, 0);
    }

    // Unbind our RT so ImGui can reclaim the output RT
    ID3D11RenderTargetView* nullRTV = nullptr;
    ctx->OMSetRenderTargets(1, &nullRTV, nullptr);
}

// ---------------------------------------------------------------------------
// DrawPreview
// ---------------------------------------------------------------------------

void DrawPreview(App& app) {

    const auto& ed = app.editor;

    // ---- Image preview (PIC / RAW) -----------------------------------------
    if (ed.libIdx != s_previewLib || ed.entryIdx != s_previewEntry) {
        s_preview.Release();
        s_previewLib   = ed.libIdx;
        s_previewEntry = ed.entryIdx;

        if (!ed.data.empty() && ed.kind != EditorKind::Sh) {
            if (ed.kind == EditorKind::Pic) {
                fx::PicInfo info;
                if (fx::pic_info(ed.data.data(), ed.data.size(), &info)) {
                    fx::Palette sysPal = FindSysPalette(app);
                    auto rgba = fx::pic_decode(ed.data.data(), ed.data.size(), &sysPal);
                    if (!rgba.empty())
                        s_preview = app.UploadTexture(rgba.data(),
                                                      (int)info.width, (int)info.height);
                }
            } else if (ed.kind == EditorKind::Raw) {
                fx::RawInfo info;
                if (fx::raw_info(ed.data.data(), ed.data.size(), &info)) {
                    auto rgba = fx::raw_decode(ed.data.data(), ed.data.size());
                    if (!rgba.empty())
                        s_preview = app.UploadTexture(rgba.data(),
                                                      (int)info.width, (int)info.height);
                }
            }
        }
    }

    // ---- SH 3D preview -------------------------------------------------------
    if (ed.kind == EditorKind::Sh) {
        ID3D11Device*        device = app.GetDevice();
        ID3D11DeviceContext* ctx    = app.GetCtx();

        // Rebuild mesh VB when selection changes
        if (ed.libIdx != s_sh.cached_lib || ed.entryIdx != s_sh.cached_entry) {
            s_sh.cached_lib   = ed.libIdx;
            s_sh.cached_entry = ed.entryIdx;

            fx::ShInfo info = fx::sh_parse_info(ed.data.data(), ed.data.size());
            fx::ShMesh mesh = fx::sh_parse_mesh(ed.data.data(), ed.data.size());

            // Reset camera to fit the model.
            // SH bbox is (X, Y, Z) = (right, forward, up); remap target to DX Y-up.
            s_sh.target[0] = (info.bbox[0] + info.bbox[3]) * 0.5f; // X unchanged
            s_sh.target[1] = (info.bbox[2] + info.bbox[5]) * 0.5f; // render Y = model Z (up)
            s_sh.target[2] = (info.bbox[1] + info.bbox[4]) * 0.5f; // render Z = model Y (fwd)
            float spanX = info.bbox[3] - info.bbox[0];
            float spanY = info.bbox[4] - info.bbox[1];
            float spanZ = info.bbox[5] - info.bbox[2];
            float maxSpan = std::max(spanX, std::max(spanY, spanZ));
            s_sh.model_span = std::max(maxSpan, 1.0f);
            // Camera INSIDE the room (room half-size = 2*max_span, orbit = 1*max_span)
            s_sh.distance   = std::max(maxSpan * 1.0f, 20.0f);
            s_sh.azimuth    = 45.0f;
            s_sh.elevation  = 25.0f;

            BuildMeshVB(device, mesh);
            BuildBoxGridVB(device, info, s_sh.model_span);
        }

        float  txt_h   = ImGui::GetTextLineHeightWithSpacing() * 2.0f + 4.0f;
        ImVec2 canvas  = ImGui::GetContentRegionAvail();
        if (canvas.x < 4) canvas.x = 4;
        canvas.y = std::max(4.0f, canvas.y - txt_h);

        int iw = (int)canvas.x, ih = (int)canvas.y;
        RenderSh(device, ctx, iw, ih);

        if (s_sh.srv) {
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("##sh3d", canvas);
            bool held    = ImGui::IsItemActive();
            bool hovered = ImGui::IsItemHovered();
            ImGui::GetWindowDrawList()->AddImage(
                (ImTextureID)(void*)s_sh.srv,
                pos, {pos.x + canvas.x, pos.y + canvas.y});

            if (held) {
                ImVec2 delta = ImGui::GetIO().MouseDelta;
                s_sh.azimuth   += delta.x * 0.4f;
                s_sh.elevation -= delta.y * 0.4f;
                s_sh.elevation  = std::max(-89.0f, std::min(89.0f, s_sh.elevation));
                s_sh.azimuth    = fmodf(s_sh.azimuth + 360.0f, 360.0f);
            }
            if (hovered) {
                float wheel = ImGui::GetIO().MouseWheel;
                if (wheel != 0.0f) {
                    s_sh.distance *= powf(0.85f, wheel);
                    float min_d = std::max(s_sh.model_span * 0.8f, 10.0f);
                    float max_d = s_sh.model_span * 1.6f;  // keep camera inside the room
                    s_sh.distance  = std::max(min_d, std::min(max_d, s_sh.distance));
                    ImGui::SetScrollY(0.0f);
                }
            }
            ImGui::TextDisabled("az=%.0f el=%.0f dist=%.0f  tgt=(%.1f,%.1f,%.1f)  span=%.0f",
                s_sh.azimuth, s_sh.elevation, s_sh.distance,
                s_sh.target[0], s_sh.target[1], s_sh.target[2], s_sh.model_span);
            ImGui::TextDisabled("Drag to orbit  |  Scroll to zoom");
        } else {
            ImGui::TextDisabled("x86-only geometry \xe2\x80\x94 no 3D preview.");
        }
        return;
    }

    // ---- Static image preview -----------------------------------------------
    if (s_preview.srv) {
        float avail = ImGui::GetContentRegionAvail().x;
        float scale = avail / (float)s_preview.width;
        float dispH = s_preview.height * scale;
        ImGui::Image((ImTextureID)(void*)s_preview.srv, ImVec2(avail, dispH));
        ImGui::TextDisabled("%dx%d", s_preview.width, s_preview.height);
    } else if (ed.kind != EditorKind::None) {
        ImGui::TextDisabled("No preview for .%s", ed.ext.c_str());
    } else {
        ImGui::TextDisabled("No record selected.");
    }
}
