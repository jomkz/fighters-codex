#include "dialogs.h"
#include "dialog_queue.h"
#include <SDL3/SDL.h>

namespace platform {
namespace {

DialogQueue g_queue;
SDL_Window* g_parent = nullptr;

// Owns the filter/location storage for one in-flight dialog — SDL requires
// the filter array to stay alive until the callback runs. Freed by the
// callback.
struct PendingOp {
    std::vector<std::string>         names;
    std::vector<std::string>         patterns;
    std::vector<SDL_DialogFileFilter> filters;
    std::string                      defaultLoc;

    explicit PendingOp(std::initializer_list<FileFilter> src,
                       const char* defaultLocation = nullptr) {
        for (const auto& f : src) {
            names.emplace_back(f.name);
            patterns.emplace_back(f.pattern);
        }
        for (size_t i = 0; i < names.size(); ++i)
            filters.push_back({names[i].c_str(), patterns[i].c_str()});
        if (defaultLocation) defaultLoc = defaultLocation;
    }
    const char* Location() const {
        return defaultLoc.empty() ? nullptr : defaultLoc.c_str();
    }
};

// May run on any thread. filelist pointers die when this returns, so copy
// immediately; null filelist (error) is treated as cancel.
void SDLCALL FileCallback(void* userdata, const char* const* filelist, int) {
    PendingOp* op = static_cast<PendingOp*>(userdata);
    std::vector<std::string> paths;
    if (filelist)
        for (const char* const* p = filelist; *p; ++p)
            paths.emplace_back(*p);
    g_queue.Complete(std::move(paths));
    delete op;
}

} // namespace

void DialogsInit(SDL_Window* parent) { g_parent = parent; }

void DialogsShutdown() {
    g_queue.Shutdown();
    g_parent = nullptr;
}

bool DialogBusy() { return g_queue.Busy(); }

void PumpDialogResults() { g_queue.Pump(); }

void OpenFilesDialog(std::initializer_list<FileFilter> filters, bool allowMany,
                     std::function<void(std::vector<std::string>)> done) {
    if (!g_queue.Begin(std::move(done))) return;
    auto* op = new PendingOp(filters);
    SDL_ShowOpenFileDialog(FileCallback, op, g_parent,
                           op->filters.data(), (int)op->filters.size(),
                           nullptr, allowMany);
}

void SaveFileDialog(std::initializer_list<FileFilter> filters,
                    const char* defaultExt, const char* defaultLocation,
                    std::function<void(std::string)> done) {
    std::string ext = defaultExt ? defaultExt : "";
    auto adapter = [done = std::move(done), ext](std::vector<std::string> v) {
        done(v.empty() ? std::string()
                       : EnsureExtension(std::move(v[0]), ext.c_str()));
    };
    if (!g_queue.Begin(std::move(adapter))) return;
    auto* op = new PendingOp(filters, defaultLocation);
    SDL_ShowSaveFileDialog(FileCallback, op, g_parent,
                           op->filters.data(), (int)op->filters.size(),
                           op->Location());
}

void ChooseFolderDialog(std::function<void(std::string)> done) {
    auto adapter = [done = std::move(done)](std::vector<std::string> v) {
        done(v.empty() ? std::string() : std::move(v[0]));
    };
    if (!g_queue.Begin(std::move(adapter))) return;
    auto* op = new PendingOp({});
    SDL_ShowOpenFolderDialog(FileCallback, op, g_parent, nullptr, false);
}

} // namespace platform
