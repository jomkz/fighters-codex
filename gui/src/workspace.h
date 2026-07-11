#pragma once
#include "fx/ealib.h"
#include <functional>
#include <string>
#include <vector>

// fxs workspace: mount an entire FA install root as ONE name-keyed namespace,
// mirroring the engine's startup scan (LibStartUp @0x478BC0 builds a single
// sorted hint index over every LIB entry plus every loose file — see
// docs/fa/memory-resource.md § LIB name resolution). This is the data layer
// consumed by the category browsers; it is deliberately SDL-free so the pure
// build step is unit-tested in gui_tests against synthetic archives.
namespace fxg {

// One mounted source: a LIB archive (its parsed directory) or a single loose
// file (one synthetic entry). No file payload is retained — only the directory.
struct MountSource {
    std::string            path;    // full path on disk
    bool                   isLib = false;
    std::vector<fx::Entry> entries; // parsed LIB dir, or 1 synthetic loose entry
};

// One resolved name in the unified namespace — points at the winning source.
struct WorkspaceEntry {
    std::string name;      // 8.3 entry name, as stored
    int         sourceIdx = -1; // index into Workspace::sources
    int         entryIdx  = -1; // index into that source's entries
};

// A name carried by more than one source. `winner` resolves; `shadowed` lists
// the source indices it overrides. Nothing is hidden — every clash is recorded.
struct Collision {
    std::string      name;
    int              winner = -1;
    std::vector<int> shadowed;
};

// The mounted namespace. `names` is sorted case-insensitively for binary lookup.
struct Workspace {
    std::string                 root;
    std::vector<MountSource>    sources;    // LIBs + loose files, in mount order
    std::vector<WorkspaceEntry> names;      // unified namespace (sorted)
    std::vector<Collision>      collisions; // duplicate names across sources
    int                         libCount   = 0;
    int                         looseCount = 0;

    bool mounted() const { return !root.empty(); }

    // Resolve a name (case-insensitive), or nullptr if absent.
    const WorkspaceEntry* find(const std::string& name) const;
};

// Pure: merge already-parsed sources into one namespace applying the engine's
// precedence — LIB entries resolve before loose files, and within one kind the
// later-mounted source wins (docs/fa/memory-resource.md) — recording every
// collision. `sources` is consumed. Deterministic and filesystem-free.
Workspace workspace_build(std::vector<MountSource> sources, std::string root = {});

// IO: scan an FA install root. Every *.LIB (case-insensitive) is opened and its
// directory parsed; every other regular file is mounted as a loose entry. LIBs
// are mounted in case-insensitive filename order so precedence is deterministic
// (the engine's FindFirstFile order is filesystem-defined). Then workspace_build.
// `progress(done, total, path)` is called as each source is processed, if set.
using ProgressFn = std::function<void(int done, int total, const std::string& path)>;
Workspace workspace_scan(const std::string& root, const ProgressFn& progress = {});

} // namespace fxg
