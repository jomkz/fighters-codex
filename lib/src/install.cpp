#include "fx/install.h"

#include "fx/blast.h"
#include "fx/txt.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace fx {

namespace fs = std::filesystem;

// The retail media, fingerprinted from the ESA directory (ESA.md § File
// Inventory). FA.EXE and FA.SMS both moved with the patch, so agreeing on the
// pair is a stronger check than either alone.
struct BuildPrint {
    MediaBuild build;
    uint32_t   fa_exe;
    uint32_t   fa_sms;
};
static const BuildPrint BUILD_PRINTS[] = {
    {MediaBuild::V100F, 1299968, 104452},  // disc
    {MediaBuild::V102F, 1319424, 106706},  // after the official 1.02F patch
};

static const uint64_t COPY_CHUNK = 1u << 16;

// --- small ASCII helpers ---------------------------------------------------

static char up(char c) { return (char)std::toupper((unsigned char)c); }

static std::string upper(const std::string& s) {
    std::string r = s;
    for (char& c : r) c = up(c);
    return r;
}

static bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (up(a[i]) != up(b[i])) return false;
    return true;
}

static bool iends_with(const std::string& s, const std::string& suffix) {
    if (s.size() < suffix.size()) return false;
    return iequals(s.substr(s.size() - suffix.size()), suffix);
}

const char* install_build_name(MediaBuild build) {
    switch (build) {
        case MediaBuild::V100F: return "1.00F";
        case MediaBuild::V102F: return "1.02F";
        case MediaBuild::Unknown: break;
    }
    return "unknown";
}

// --- glob ------------------------------------------------------------------

// `*` and `?` against an upper-cased name. Backtracking is bounded by the
// pattern length, which comes from a script, but a fuzzer supplies both sides,
// so this is the iterative form: no recursion, linear memory, no blow-up on
// "*a*a*a*a...".
static bool wildcard(const std::string& pat, const std::string& name) {
    size_t p = 0, n = 0, star = std::string::npos, mark = 0;
    while (n < name.size()) {
        if (p < pat.size() && (pat[p] == '?' || pat[p] == name[n])) {
            p++; n++;
        } else if (p < pat.size() && pat[p] == '*') {
            star = p++;
            mark = n;
        } else if (star != std::string::npos) {
            p = star + 1;
            n = ++mark;
        } else {
            return false;
        }
    }
    while (p < pat.size() && pat[p] == '*') p++;
    return p == pat.size();
}

bool install_match(const std::string& pattern, const std::string& name) {
    // DOS: "*.*" is everything, dot or no dot. The scripts lean on it hard
    // (three of the eleven INSTALL_FILES directives select a whole label).
    if (pattern == "*.*") return true;
    const std::string p = upper(pattern), n = upper(name);
    if (p.find('*') == std::string::npos && p.find('?') == std::string::npos)
        return p == n;
    return wildcard(p, n);
}

// --- scan ------------------------------------------------------------------

static const DiscFile* find_loose(const DiscSource& disc, const std::string& name) {
    for (const auto& f : disc.loose)
        if (iequals(f.name, name)) return &f;
    return nullptr;
}

int install_probe_disc(const DiscSource& disc) {
    // Disc 1 is the one that carries the installer: the archive and the master
    // script. Both, so that a directory holding a stray SETUP.ESA is not
    // mistaken for one.
    const bool has_esa = !disc.esa_name.empty() && find_loose(disc, disc.esa_name);
    const bool has_ssf = find_loose(disc, "SETUP.SSF") != nullptr;
    if (has_esa && has_ssf) return 1;

    // Disc 2 is data only: LIBs, no archive, no scripts, no executables.
    if (!has_esa && disc.scripts.empty()) {
        bool lib = false;
        for (const auto& f : disc.loose) {
            if (iends_with(f.name, ".LIB")) lib = true;
            else if (iends_with(f.name, ".EXE")) return 0;
        }
        if (lib) return 2;
    }
    return 0;
}

static std::vector<uint8_t> read_all(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(f),
                                std::istreambuf_iterator<char>());
}

// The archive directory only: enough of the head to cover it, never the 110 MB
// body. esa_read_dir takes the real archive size separately so its bounds
// checks stay exact.
static std::vector<uint8_t> read_head(const fs::path& path, size_t bytes) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::vector<uint8_t> buf(bytes);
    f.read((char*)buf.data(), (std::streamsize)bytes);
    buf.resize((size_t)f.gcount());
    return buf;
}

DiscSource install_scan(const std::string& root) {
    DiscSource disc;
    disc.root = root;

    std::error_code ec;
    for (const auto& de : fs::directory_iterator(root, ec)) {
        if (!de.is_regular_file(ec)) continue;
        const std::string name = de.path().filename().string();
        const uint64_t size = (uint64_t)fs::file_size(de.path(), ec);
        if (ec) continue;
        disc.loose.push_back({name, size});

        if (iends_with(name, ".ESA") && disc.esa_name.empty()) disc.esa_name = name;
        else if (iends_with(name, ".SSF")) {
            const auto bytes = read_all(de.path());
            disc.scripts.push_back({name, ssf_read(bytes.data(), bytes.size())});
        }
    }
    if (ec && disc.loose.empty()) return disc;  // unreadable directory: disc 0

    std::sort(disc.loose.begin(), disc.loose.end(),
              [](const DiscFile& a, const DiscFile& b) { return a.name < b.name; });
    std::sort(disc.scripts.begin(), disc.scripts.end(),
              [](const DiscScript& a, const DiscScript& b) { return a.name < b.name; });

    if (!disc.esa_name.empty()) {
        const fs::path esa_path = fs::path(root) / disc.esa_name;
        const uint64_t esa_size = (uint64_t)fs::file_size(esa_path, ec);
        // 1 MiB covers the retail directory (1,183 bytes) with room to spare.
        const auto head = read_head(esa_path, (size_t)std::min<uint64_t>(esa_size, 1u << 20));
        disc.esa = esa_read_dir(head.data(), head.size(), esa_size);
    }

    disc.disc = install_probe_disc(disc);
    return disc;
}

std::vector<std::string> install_list_dir(const std::string& dir) {
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& de : fs::directory_iterator(dir, ec)) {
        if (de.is_regular_file(ec)) names.push_back(de.path().filename().string());
    }
    std::sort(names.begin(), names.end());
    return names;
}

// --- plan ------------------------------------------------------------------

// "[INSTALL_PATH]\SUB" -> "SUB": the destination is always expressed relative to
// the install directory the user chose, which is the directory we are writing.
static std::string dest_dir(const std::string& arg) {
    std::string s = arg;
    const std::string token = "[INSTALL_PATH]";
    if (s.size() >= token.size() && iequals(s.substr(0, token.size()), token))
        s = s.substr(token.size());
    for (char& c : s)
        if (c == '\\') c = '/';
    while (!s.empty() && s.front() == '/') s.erase(s.begin());
    while (!s.empty() && s.back() == '/') s.pop_back();
    return s;
}

static std::string join(const std::string& dir, const std::string& name) {
    return dir.empty() ? name : dir + "/" + name;
}

static const std::string* arg_at(const SsfStatement& st, size_t i) {
    return i < st.args.size() ? &st.args[i] : nullptr;
}

// The first argument of the first statement with this keyword.
static std::string script_value(const SsfDoc& doc, const std::string& keyword) {
    for (const auto& st : doc.statements)
        if (st.keyword == keyword && !st.args.empty()) return st.args[0];
    return {};
}

static const DiscScript* find_script(const DiscSource& disc, const std::string& name) {
    for (const auto& s : disc.scripts)
        if (iequals(s.name, name)) return &s;
    return nullptr;
}

// One sub-script (FINSTALL.SSF or MINSTALL.SSF) resolved against the archive.
struct Resolved {
    std::vector<InstallItem>      items;
    std::vector<InstallDirective> directives;
    std::vector<std::string>      guards;  // SKIP_ON_REMOVE globs
};

static Resolved resolve_script(const DiscScript& script, const DiscSource& disc1,
                               size_t disc1_index) {
    Resolved out;
    for (const auto& st : script.doc.statements) {
        InstallDirective d;
        d.script  = script.name;
        d.line    = st.line;
        d.keyword = st.keyword;
        d.args    = st.args;

        if (st.keyword == "INSTALL_FILES") {
            const std::string* glob  = arg_at(st, 0);
            const std::string* label = arg_at(st, 1);
            const std::string* dst   = arg_at(st, 2);
            if (!glob || !label) {
                d.note = "malformed: INSTALL_FILES needs a glob and a label";
                out.directives.push_back(d);
                continue;
            }
            const std::string dir = dst ? dest_dir(*dst) : std::string();
            size_t matched = 0;
            for (const auto& e : disc1.esa) {
                if (!iequals(e.label, *label)) continue;
                if (!install_match(*glob, e.name)) continue;
                InstallItem it;
                it.dest   = join(dir, esa_safe_name(e.name));
                it.origin = InstallOrigin::Archive;
                it.disc   = disc1_index;
                it.source = e.name;
                it.label  = e.label;
                it.bytes  = e.usize;
                out.items.push_back(it);
                matched++;
            }
            d.honored = matched > 0;
            if (!matched)
                d.note = "no archive entry matches " + *glob + " under label " + *label;
            out.directives.push_back(d);

        } else if (st.keyword == "INSTALL_SYSFILES") {
            const std::string* name  = arg_at(st, 0);
            const std::string* label = arg_at(st, 1);
            if (!name || !label) {
                d.note = "malformed: INSTALL_SYSFILES needs a name and a label";
                out.directives.push_back(d);
                continue;
            }
            for (const auto& e : disc1.esa) {
                if (!iequals(e.label, *label)) continue;
                if (!install_match(*name, e.name)) continue;
                InstallItem it;
                it.dest   = esa_safe_name(e.name);
                it.status = InstallStatus::SkipSysfile;
                it.origin = InstallOrigin::Archive;
                it.disc   = disc1_index;
                it.source = e.name;
                it.label  = e.label;
                it.bytes  = e.usize;
                it.note   = "EA installer support tool, bound for the Windows system "
                            "directory; the game does not load it";
                out.items.push_back(it);
            }
            // Deliberately not acted on: there is no Windows system directory to
            // install into, and the two files it names (EAREMOVE.EXE, EAEXEC.EXE)
            // belong to the EA installer, not to FA.
            d.note = "system-directory install: not performed";
            out.directives.push_back(d);

        } else if (st.keyword == "SKIP_ON_REMOVE") {
            if (const std::string* glob = arg_at(st, 0)) {
                out.guards.push_back(*glob);
                d.honored = true;
                d.note    = "clobber guard: a file matching it is never overwritten";
            } else {
                d.note = "malformed: SKIP_ON_REMOVE needs a glob";
            }
            out.directives.push_back(d);

        } else if (st.keyword == "CREATE_FOLDERS") {
            d.honored = true;
            d.note    = "the install directory is created by the engine";
            out.directives.push_back(d);

        } else {
            d.note = "Windows shell/registry directive: not applicable";
            out.directives.push_back(d);
        }
    }
    return out;
}

static MediaBuild fingerprint(const std::vector<EsaEntry>& esa) {
    const EsaEntry* exe = esa_find(esa, "FA.EXE");
    const EsaEntry* sms = esa_find(esa, "FA.SMS");
    if (!exe || !sms) return MediaBuild::Unknown;
    for (const auto& p : BUILD_PRINTS)
        if (p.fa_exe == exe->usize && p.fa_sms == sms->usize) return p.build;
    return MediaBuild::Unknown;
}

InstallPlan install_plan(const std::vector<DiscSource>& discs,
                         const std::vector<std::string>& existing,
                         const InstallOptions& opt) {
    InstallPlan plan;

    size_t d1 = discs.size();
    for (size_t i = 0; i < discs.size(); i++)
        if (discs[i].disc == 1) { d1 = i; break; }
    if (d1 == discs.size()) {
        plan.errors.push_back("no Disc 1 among the sources (need the one with "
                              "SETUP.ESA and SETUP.SSF)");
        return plan;
    }
    const DiscSource& disc1 = discs[d1];

    const DiscScript* setup = find_script(disc1, "SETUP.SSF");
    if (!setup) {
        plan.errors.push_back("Disc 1 has no SETUP.SSF");
        return plan;
    }
    plan.company      = script_value(setup->doc, "COMPANY_NAME");
    plan.app_name     = script_value(setup->doc, "APP_NAME");
    plan.default_path = script_value(setup->doc, "DEFAULT_PATH");
    plan.build        = fingerprint(disc1.esa);

    for (const auto& st : setup->doc.statements) {
        InstallDirective d;
        d.script  = setup->name;
        d.line    = st.line;
        d.keyword = st.keyword;
        d.args    = st.args;
        d.honored = st.keyword == "COMPANY_NAME" || st.keyword == "APP_NAME" ||
                    st.keyword == "DEFAULT_PATH" || st.keyword == "INSTALL_SCRIPT";
        if (!d.honored) d.note = "Windows shell/registry directive: not applicable";
        plan.directives.push_back(d);
    }

    // Both sub-scripts, then pick by size. The INSTALL_SCRIPT labels that say
    // which is which are localised prose (":0409:Full Install - Digital
    // Music:0C:Installation complète…"), so they are not a key.
    std::vector<Resolved> candidates;
    std::vector<std::string> names;
    for (const auto& st : setup->doc.statements) {
        if (st.keyword != "INSTALL_SCRIPT" || st.args.empty()) continue;
        const DiscScript* sub = find_script(disc1, st.args[0]);
        if (!sub) {
            plan.errors.push_back("SETUP.SSF references a script that is not on the "
                                  "disc: " + st.args[0]);
            continue;
        }
        candidates.push_back(resolve_script(*sub, disc1, d1));
        names.push_back(sub->name);
    }
    if (candidates.empty()) {
        plan.errors.push_back("SETUP.SSF names no install script");
        return plan;
    }

    size_t pick = 0;
    for (size_t i = 1; i < candidates.size(); i++) {
        const bool bigger = candidates[i].items.size() > candidates[pick].items.size();
        if (opt.full == bigger) pick = i;
    }
    Resolved& chosen = candidates[pick];
    plan.script = names[pick];
    plan.items  = std::move(chosen.items);
    plan.directives.insert(plan.directives.end(), chosen.directives.begin(),
                           chosen.directives.end());

    // CD-resident LIBs. A rule, not a list: every loose .LIB a disc root holds
    // that the archive does not supply is one the game read off the CD at run
    // time. On the retail media that is FA_4C/FA_7 (disc 1) and all five of
    // disc 2 — and it needs no manifest, so it carries to ATF and USNF.
    if (opt.cd_resident) {
        for (size_t i = 0; i < discs.size(); i++) {
            for (const auto& f : discs[i].loose) {
                if (!iends_with(f.name, ".LIB")) continue;
                if (esa_find(disc1.esa, f.name)) continue;
                // The name on disc is not a fact about the game — ISO9660 mount
                // options decide its case. The install is upper-case, as the
                // archive's own names are.
                InstallItem it;
                it.dest   = upper(f.name);
                it.origin = InstallOrigin::Loose;
                it.disc   = i;
                it.source = f.name;
                it.bytes  = f.size;
                it.note   = "CD-resident: loaded from the disc at run time, copied so "
                            "none is needed";
                plan.items.push_back(it);
            }
        }
    }

    // One destination, one item.
    std::vector<InstallItem> unique;
    for (auto& it : plan.items) {
        bool dup = false;
        for (const auto& prev : unique)
            if (iequals(prev.dest, it.dest)) { dup = true; break; }
        if (!dup) unique.push_back(std::move(it));
    }
    plan.items = std::move(unique);

    // The clobber guard. SKIP_ON_REMOVE marks what the *game* writes — pilots,
    // missions, screen captures, EA.CFG — so those are never overwritten, not
    // even with --overwrite. (EXAMPLE.MT is both shipped and matched by `*.MT`:
    // a fresh install writes it, a re-install keeps the one the user edited.)
    for (auto& it : plan.items) {
        if (it.status != InstallStatus::Copy) continue;
        bool present = false;
        for (const auto& name : existing)
            if (iequals(name, it.dest)) { present = true; break; }
        if (!present) continue;

        std::string guard;
        for (const auto& g : chosen.guards)
            if (install_match(g, it.dest)) { guard = g; break; }

        it.status = InstallStatus::KeepExisting;
        it.note   = !guard.empty()
                        ? "preserved: SKIP_ON_REMOVE \"" + guard + "\" marks it as a file "
                          "the game writes"
                        : (opt.overwrite ? std::string() : "already present (--overwrite "
                                                           "to replace it)");
        if (guard.empty() && opt.overwrite) it.status = InstallStatus::Copy;
    }

    for (const auto& it : plan.items)
        if (it.status == InstallStatus::Copy) plan.bytes += it.bytes;

    if (plan.items.empty())
        plan.errors.push_back("the scripts select no files from the archive");
    if (plan.build == MediaBuild::Unknown && !opt.allow_unknown_media)
        plan.errors.push_back("unrecognised media: the archive's FA.EXE/FA.SMS match "
                              "neither the 1.00F disc nor the 1.02F patched build");
    return plan;
}

// --- execute ---------------------------------------------------------------

// A copy that never holds more than a chunk: FA_7.LIB is 160 MB.
static bool copy_region(std::ifstream& in, uint64_t offset, uint64_t size,
                        std::ofstream& out) {
    in.seekg((std::streamoff)offset);
    if (!in) return false;
    std::vector<char> buf((size_t)std::min<uint64_t>(size, COPY_CHUNK));
    uint64_t left = size;
    while (left) {
        const size_t n = (size_t)std::min<uint64_t>(left, buf.size());
        in.read(buf.data(), (std::streamsize)n);
        if ((size_t)in.gcount() != n) return false;
        out.write(buf.data(), (std::streamsize)n);
        if (!out) return false;
        left -= n;
    }
    return true;
}

// A PKWA payload decodes whole — the compressed side is at most 678 KB on the
// retail archive and the plain side at most 1.3 MB. Only the stored entries are
// big, and those stream.
static std::vector<uint8_t> read_pkwa(std::ifstream& in, const EsaEntry& e) {
    std::vector<uint8_t> comp(e.csize);
    in.seekg((std::streamoff)e.offset);
    in.read((char*)comp.data(), (std::streamsize)e.csize);
    if ((uint64_t)in.gcount() != (uint64_t)e.csize) return {};
    std::vector<uint8_t> out(e.usize);
    const int n = blast_decompress(comp.data(), comp.size(), out.data(), out.size());
    if (n < 0 || (uint32_t)n != e.usize) return {};
    return out;
}

// Where an item's bytes come from, resolved once for both execute and verify.
struct ItemSource {
    fs::path        path;    // the disc file to read (the archive, or a loose file)
    const EsaEntry* entry;   // null for a loose file
};

static bool item_source(const std::vector<DiscSource>& discs, const InstallItem& it,
                        ItemSource* out, std::string* err) {
    if (it.disc >= discs.size()) {
        *err = it.dest + ": no such disc in the sources";
        return false;
    }
    const DiscSource& disc = discs[it.disc];
    if (it.origin == InstallOrigin::Loose) {
        const DiscFile* f = find_loose(disc, it.source);
        if (!f) { *err = it.dest + ": " + it.source + " is not on " + disc.root; return false; }
        *out = {fs::path(disc.root) / f->name, nullptr};
        return true;
    }
    const EsaEntry* e = esa_find(disc.esa, it.source);
    if (!e) { *err = it.dest + ": " + it.source + " is not in the archive"; return false; }
    if (e->method != "PKWA" && e->method != "NULL") {
        *err = it.dest + ": unsupported compression method " + e->method;
        return false;
    }
    *out = {fs::path(disc.root) / disc.esa_name, e};
    return true;
}

bool install_execute(const std::vector<DiscSource>& discs, const InstallPlan& plan,
                     const std::string& dest, InstallProgress progress, void* user,
                     std::vector<std::string>* errors) {
    auto fail = [&](const std::string& msg) {
        if (errors) errors->push_back(msg);
        return false;
    };
    if (!plan.errors.empty()) return fail("the plan carries errors; refusing to install");

    std::error_code ec;
    fs::create_directories(dest, ec);
    if (!fs::is_directory(dest, ec)) return fail("cannot create the install directory: " + dest);

    const auto space = fs::space(dest, ec);
    if (!ec && space.available < plan.bytes)
        return fail("not enough space in " + dest + ": the install needs " +
                    std::to_string(plan.bytes) + " bytes");

    uint64_t done = 0;
    bool ok = true;
    for (const auto& it : plan.items) {
        if (it.status != InstallStatus::Copy) continue;

        std::string err;
        ItemSource src;
        if (!item_source(discs, it, &src, &err)) { ok = fail(err); continue; }

        const fs::path out_path = fs::path(dest) / it.dest;
        fs::create_directories(out_path.parent_path(), ec);
        fs::path part = out_path;
        part += ".part";

        std::ifstream in(src.path, std::ios::binary);
        if (!in) { ok = fail(it.dest + ": cannot read " + src.path.string()); continue; }
        std::ofstream out(part, std::ios::binary | std::ios::trunc);
        if (!out) { ok = fail(it.dest + ": cannot write " + part.string()); continue; }

        bool wrote;
        if (!src.entry) {
            wrote = copy_region(in, 0, it.bytes, out);
        } else if (src.entry->method == "NULL") {
            wrote = copy_region(in, src.entry->offset, src.entry->csize, out);
        } else {
            const auto bytes = read_pkwa(in, *src.entry);
            wrote = !bytes.empty() || src.entry->usize == 0;
            if (wrote) out.write((const char*)bytes.data(), (std::streamsize)bytes.size());
            wrote = wrote && out.good();
        }
        out.close();
        if (!wrote) {
            fs::remove(part, ec);
            ok = fail(it.dest + ": failed to extract " + it.source);
            continue;
        }

        // Rename last: an interrupted install leaves a .part, never a file that
        // looks complete.
        fs::rename(part, out_path, ec);
        if (ec) {
            fs::remove(part, ec);
            ok = fail(it.dest + ": cannot replace " + out_path.string());
            continue;
        }

        done += it.bytes;
        if (progress) progress(it, done, plan.bytes, user);
    }
    return ok;
}

// --- verify ----------------------------------------------------------------

// Compare a file against a region of another file, a chunk at a time.
static bool same_region(std::ifstream& want, uint64_t offset, uint64_t size,
                        std::ifstream& got, uint64_t got_size) {
    if (got_size != size) return false;
    want.seekg((std::streamoff)offset);
    got.seekg(0);
    if (!want || !got) return false;
    std::vector<char> a((size_t)std::min<uint64_t>(size, COPY_CHUNK)), b(a.size());
    uint64_t left = size;
    while (left) {
        const size_t n = (size_t)std::min<uint64_t>(left, a.size());
        want.read(a.data(), (std::streamsize)n);
        got.read(b.data(), (std::streamsize)n);
        if ((size_t)want.gcount() != n || (size_t)got.gcount() != n) return false;
        if (std::memcmp(a.data(), b.data(), n) != 0) return false;
        left -= n;
    }
    return true;
}

// Compare a file against bytes already in memory (a decoded PKWA payload).
static bool same_bytes(std::ifstream& got, uint64_t got_size,
                       const std::vector<uint8_t>& want) {
    if (got_size != want.size()) return false;
    got.seekg(0);
    std::vector<char> buf((size_t)std::min<uint64_t>(want.size(), COPY_CHUNK));
    size_t at = 0;
    while (at < want.size()) {
        const size_t n = std::min(want.size() - at, buf.size());
        got.read(buf.data(), (std::streamsize)n);
        if ((size_t)got.gcount() != n) return false;
        if (std::memcmp(buf.data(), want.data() + at, n) != 0) return false;
        at += n;
    }
    return true;
}

bool install_verify(const std::vector<DiscSource>& discs, const InstallPlan& plan,
                    const std::string& dest, std::vector<std::string>* errors) {
    auto fail = [&](const std::string& msg) {
        if (errors) errors->push_back(msg);
        return false;
    };
    bool ok = true;
    for (const auto& it : plan.items) {
        if (it.status != InstallStatus::Copy) continue;

        std::string err;
        ItemSource src;
        if (!item_source(discs, it, &src, &err)) { ok = fail(err); continue; }

        const fs::path out_path = fs::path(dest) / it.dest;
        std::error_code ec;
        const uint64_t got_size = (uint64_t)fs::file_size(out_path, ec);
        if (ec) { ok = fail(it.dest + ": missing from " + dest); continue; }

        std::ifstream got(out_path, std::ios::binary);
        std::ifstream in(src.path, std::ios::binary);
        if (!got || !in) { ok = fail(it.dest + ": cannot re-read it"); continue; }

        bool same;
        if (!src.entry) {
            same = same_region(in, 0, it.bytes, got, got_size);
        } else if (src.entry->method == "NULL") {
            same = same_region(in, src.entry->offset, src.entry->csize, got, got_size);
        } else {
            same = same_bytes(got, got_size, read_pkwa(in, *src.entry));
        }
        if (!same) ok = fail(it.dest + ": does not match the disc");
    }
    return ok;
}

} // namespace fx
