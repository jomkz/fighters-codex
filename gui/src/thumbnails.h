#pragma once
// Async SH thumbnails for the category browsers (#366). A single worker
// thread resolves an object's shape through the asset graph, extracts it from
// its source LIB, renders it through the display-free scene core
// (editors/sh_scene.h — the FA-faithful software backend, context-free), and
// hands finished RGBA images back to the UI thread, which uploads GL textures.
// Rendered thumbnails are cached on disk keyed by the record's digest so
// re-opens skip the render entirely. No ImGui, no SDL — gui_tests drive the
// whole service headless.
#include "asset_index.h"
#include "workspace.h"
#include "fx/pal.h"
#include "fx_render/render.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace fxg {

class ThumbnailService {
public:
    ~ThumbnailService();

    // Start over a mounted workspace and built index (both must outlive the
    // service run; Stop before replacing either). `cacheDir` holds the disk
    // cache (created if missing; empty = memory-only). `px` is the square
    // thumbnail size.
    void Start(const Workspace& ws, const AssetIndex& idx,
               std::string cacheDir, int px);
    void Stop();  // cooperative cancel + join; safe to call any time
    bool running() const { return m_running.load(); }
    int  pixels() const { return m_px; }

    // UI thread: queue a thumbnail for a workspace node. Idempotent — a node
    // is only ever rendered once per run.
    void Request(int node);

    // One finished thumbnail. width == 0 records "no renderable shape"
    // (object without a shape, x86-only geometry) so the UI can stop asking.
    struct Result {
        int node = -1;
        fx_render::Image image;
    };

    // UI thread: drain finished results (the caller owns uploads/display).
    std::vector<Result> Drain();

    // Diagnostics for tests and the status line.
    int renders() const { return m_renders.load(); }
    int diskHits() const { return m_diskHits.load(); }

    // Requests not yet delivered (queued or in flight) — the headless flows
    // settle on pending() == 0 before capturing.
    int pending();

private:
    void Worker();
    Result Produce(int node);

    const Workspace*  m_ws  = nullptr;
    const AssetIndex* m_idx = nullptr;
    std::string       m_cacheDir;
    int               m_px = 128;
    fx::Palette       m_palette{};

    std::thread             m_thread;
    std::mutex              m_mutex;   // guards queue, requested, results
    std::condition_variable m_cv;
    std::deque<int>         m_queue;
    std::unordered_set<int> m_requested;
    std::vector<Result>     m_results;
    int                     m_delivered = 0;  // guarded by m_mutex
    std::atomic<bool>       m_stop{false};
    std::atomic<bool>       m_running{false};
    std::atomic<int>        m_renders{0};
    std::atomic<int>        m_diskHits{0};

    // Worker-only: the last source file read, kept so a burst of requests
    // against one LIB reads it once.
    int                  m_bufSource = -1;
    std::vector<uint8_t> m_buf;
};

}  // namespace fxg
