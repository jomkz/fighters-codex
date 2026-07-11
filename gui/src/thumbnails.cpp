#include "thumbnails.h"
#include "editors/sh_scene.h"
#include "util.h"
#include "fx/ealib.h"
#include "fx/pic.h"
#include "fx/sh.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>

namespace fs = std::filesystem;

namespace fxg {

static std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    std::streamoff sz = f.tellg();
    if (sz <= 0) return {};
    std::vector<uint8_t> data((size_t)sz);
    f.seekg(0);
    f.read((char*)data.data(), sz);
    return data;
}

static bool ci_ends_with(const std::string& s, const char* suffix) {
    size_t n = std::strlen(suffix);
    if (s.size() < n) return false;
    for (size_t i = 0; i < n; ++i)
        if (ascii_lower(s[s.size() - n + i]) != ascii_lower(suffix[i]))
            return false;
    return true;
}

// FNV-1a 64 — the cache key; two records only share a file when their bytes
// (and the palette + size baked into the key by the caller) match.
static uint64_t fnv1a(const uint8_t* p, size_t n, uint64_t h = 0xcbf29ce484222325ull) {
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 0x100000001b3ull;
    }
    return h;
}

// Disk-cache entry: "FXTH" magic, u32 width/height, raw RGBA8.
static bool load_cached(const std::string& path, int px, fx_render::Image& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    char magic[4] = {};
    uint32_t w = 0, h = 0;
    f.read(magic, 4);
    f.read((char*)&w, 4);
    f.read((char*)&h, 4);
    if (!f || std::memcmp(magic, "FXTH", 4) != 0) return false;
    if ((int)w != px || (int)h != px || w == 0 || w > 4096 || h > 4096) return false;
    out.resize((int)w, (int)h);
    f.read((char*)out.pixels.data(), (std::streamsize)out.pixels.size());
    return (bool)f;
}

static void store_cached(const std::string& path, const fx_render::Image& img) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return;
    uint32_t w = (uint32_t)img.width, h = (uint32_t)img.height;
    f.write("FXTH", 4);
    f.write((const char*)&w, 4);
    f.write((const char*)&h, 4);
    f.write((const char*)img.pixels.data(), (std::streamsize)img.pixels.size());
}

ThumbnailService::~ThumbnailService() { Stop(); }

void ThumbnailService::Start(const Workspace& ws, const AssetIndex& idx,
                             std::string cacheDir, int px) {
    Stop();
    m_ws       = &ws;
    m_idx      = &idx;
    m_cacheDir = std::move(cacheDir);
    m_px       = px > 0 ? px : 128;
    m_queue.clear();
    m_requested.clear();
    m_results.clear();
    m_delivered = 0;
    m_renders.store(0);
    m_diskHits.store(0);
    m_bufSource = -1;
    m_buf.clear();
    if (!m_cacheDir.empty()) {
        std::error_code ec;
        fs::create_directories(m_cacheDir, ec);  // best-effort; misses just skip the disk cache
    }
    m_stop.store(false);
    m_running.store(true);
    m_thread = std::thread([this] { Worker(); });
}

void ThumbnailService::Stop() {
    if (m_thread.joinable()) {
        m_stop.store(true);
        m_cv.notify_all();
        m_thread.join();
    }
    m_running.store(false);
}

void ThumbnailService::Request(int node) {
    if (!m_running.load() || !m_ws) return;
    if (node < 0 || node >= (int)m_ws->names.size()) return;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (!m_requested.insert(node).second) return;  // already queued or done
        m_queue.push_back(node);
    }
    m_cv.notify_one();
}

std::vector<ThumbnailService::Result> ThumbnailService::Drain() {
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<Result> out = std::move(m_results);
    m_results.clear();
    m_delivered += (int)out.size();
    return out;
}

int ThumbnailService::pending() {
    std::lock_guard<std::mutex> lk(m_mutex);
    return (int)m_requested.size() - m_delivered - (int)m_results.size();
}

void ThumbnailService::Worker() {
    // The palette every thumbnail decodes and quantizes against: the install's
    // PALETTE.PAL (the same base the preview's Auto mode hunts). Loaded here so
    // Start never blocks the UI thread on LIB reads.
    if (const WorkspaceEntry* pe = m_ws->find("PALETTE.PAL")) {
        const MountSource& src = m_ws->sources[pe->sourceIdx];
        std::vector<uint8_t> buf = read_file(src.path);
        std::vector<uint8_t> rec;
        if (!buf.empty())
            rec = src.isLib ? fx::ealib_extract(buf.data(), buf.size(),
                                                src.entries[pe->entryIdx], true)
                            : std::move(buf);
        m_palette = fx::pal_load(rec.data(), rec.size());
    }

    for (;;) {
        int node = -1;
        {
            std::unique_lock<std::mutex> lk(m_mutex);
            m_cv.wait(lk, [this] { return m_stop.load() || !m_queue.empty(); });
            if (m_stop.load()) return;
            node = m_queue.front();
            m_queue.pop_front();
        }

        Result r = Produce(node);

        std::lock_guard<std::mutex> lk(m_mutex);
        m_results.push_back(std::move(r));
    }
}

ThumbnailService::Result ThumbnailService::Produce(int node) {
    Result r;
    r.node = node;

    // Resolve the node's shape: the node itself when it is a .SH, else the
    // first .SH its graph references reach (a PT/OT/NT names its shape
    // directly — memory-resource.md's `~` shape refs become these edges).
    int shNode = -1;
    const std::string& name = m_ws->names[node].name;
    if (ci_ends_with(name, ".sh")) {
        shNode = node;
    } else {
        for (int j : m_idx->nodes[node].refs)
            if (ci_ends_with(m_ws->names[j].name, ".sh")) { shNode = j; break; }
    }
    if (shNode < 0) return r;  // no shape — a negative result the UI remembers

    // Extract the record, reusing the last source read across a burst.
    auto extract = [this](int k) -> std::vector<uint8_t> {
        const WorkspaceEntry& we = m_ws->names[k];
        const MountSource& src = m_ws->sources[we.sourceIdx];
        if (m_bufSource != we.sourceIdx) {
            m_buf = read_file(src.path);
            m_bufSource = we.sourceIdx;
        }
        if (m_buf.empty()) return {};
        if (!src.isLib) return m_buf;
        return fx::ealib_extract(m_buf.data(), m_buf.size(),
                                 src.entries[we.entryIdx], true);
    };

    std::vector<uint8_t> rec = extract(shNode);
    if (rec.empty()) return r;

    // Cache key: record bytes + palette + size. The palette rarely changes,
    // but a different install (or a future palette switch) must not serve
    // stale colours.
    uint64_t key = fnv1a(rec.data(), rec.size());
    key = fnv1a((const uint8_t*)m_palette.r, sizeof(m_palette.r), key);
    key = fnv1a((const uint8_t*)m_palette.g, sizeof(m_palette.g), key);
    key = fnv1a((const uint8_t*)m_palette.b, sizeof(m_palette.b), key);
    key = fnv1a((const uint8_t*)&m_px, sizeof(m_px), key);

    std::string cachePath;
    if (!m_cacheDir.empty()) {
        char hex[17];
        std::snprintf(hex, sizeof(hex), "%016llx", (unsigned long long)key);
        cachePath = (fs::path(m_cacheDir) / (std::string(hex) + ".fxt")).string();
        if (load_cached(cachePath, m_px, r.image)) {
            m_diskHits.fetch_add(1);
            return r;
        }
    }

    // The shape's first texture, decoded against the install palette (SH
    // texture names may omit ".PIC").
    std::shared_ptr<const fx_render::Image> tex;
    fx::ShInfo info = fx::sh_parse_info(rec.data(), rec.size());
    for (const std::string& t : info.textures) {
        if (t.empty()) continue;
        const WorkspaceEntry* te = m_ws->find(t);
        if (!te && t.find('.') == std::string::npos) te = m_ws->find(t + ".PIC");
        if (!te) break;
        std::vector<uint8_t> pic = extract((int)(te - m_ws->names.data()));
        fx::PicInfo pi;
        if (!pic.empty() && fx::pic_info(pic.data(), pic.size(), &pi)) {
            auto img = std::make_shared<fx_render::Image>();
            auto rgba = fx::pic_decode(pic.data(), pic.size(), &m_palette);
            if (!rgba.empty()) {
                img->resize((int)pi.width, (int)pi.height);
                std::memcpy(img->pixels.data(), rgba.data(), rgba.size());
                tex = std::move(img);
            }
        }
        break;
    }

    r.image = RenderShThumbnail(rec.data(), rec.size(), m_palette, std::move(tex), m_px);
    if (r.image.width > 0) {
        m_renders.fetch_add(1);
        if (!cachePath.empty()) store_cached(cachePath, r.image);
    }
    return r;
}

}  // namespace fxg
