#pragma once
#include "workspace.h"
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// fxs asset-graph index (#362): over a mounted workspace, assign every entry to
// one or more categories and materialize the documented cross-reference graph
// (PT/OT/NT `~` shape refs → SH → texture PICs + wreck siblings; CAM → missions;
// missions → terrain/objects). The object-category browsers (#364) and the
// object-scoped workspace (#365) consume this. SDL-free so the pure parts are
// unit-tested in gui_tests. See docs/gui.md § Workspace (category rules).
namespace fxg {

// The eight browsable categories align 1:1 with fxs::icons::Id (Aircraft..ArtUI);
// Unassigned is the explicit "nothing hidden" bucket for entries no rule places.
enum class Category {
    Aircraft, Vehicles, Weapons, Missions, Campaigns, Terrain, Audio, ArtUI,
    Unassigned,
    Count
};
inline constexpr int kCategoryCount = static_cast<int>(Category::Count);

const char* category_name(Category c); // "Aircraft" … "Unassigned"

// Base category for an entry from its extension alone (pure — no graph, no IO).
// Unknown/extension-less names return Category::Unassigned. This is the seed the
// graph propagation extends; exposed for unit testing the invariant directly.
Category category_of(const std::string& name);

// One node per workspace name (index == index into Workspace::names).
struct AssetNode {
    uint16_t         categories = 0;  // bitmask over Category (1u << int(cat))
    std::vector<int> refs;            // resolved outgoing edges (node indices)
};

struct AssetIndex {
    std::vector<AssetNode>        nodes;      // parallel to Workspace::names
    std::vector<std::vector<int>> byCategory; // Category -> node indices (sorted)
    bool                          built = false;

    bool has(int node, Category c) const {
        return node >= 0 && node < (int)nodes.size() &&
               (nodes[node].categories & (1u << (int)c)) != 0;
    }
};

// Progress sink: (done, total, current source path). Called from the build thread.
using IndexProgressFn = std::function<void(int done, int total, const std::string&)>;

// Optional cooperative cancel: the builder checks it between sources and bails
// early (returns a partial, built=false index) when it reads true.
struct IndexCancel { std::atomic<bool> stop{false}; };

// Build the index over a mounted workspace. Reads each LIB once, extracts and
// parses the ref-bearing record types, resolves edges against the namespace,
// then propagates object categories along the graph so an object's shapes,
// textures and wreck siblings share its category. IO + CPU heavy — the GUI runs
// it on a worker thread; gui_tests call it synchronously.
AssetIndex asset_index_build(const Workspace& ws,
                             const IndexProgressFn& progress = {},
                             const IndexCancel* cancel = nullptr);

} // namespace fxg
