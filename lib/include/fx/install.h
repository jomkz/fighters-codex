#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "fx/esa.h"
#include "fx/ssf.h"

// install — the FA disc install engine (see docs/fa/formats/SSF.md § Install
// Engine).
//
// What SETUP.EXE does, portably: read the .SSF scripts from the Disc 1 root,
// resolve every INSTALL_FILES directive against the SETUP.ESA directory, and
// copy the selected entries to the install directory. Disc 2 and the two
// CD-resident LIBs on Disc 1 are loose files the scripts never mention; a full
// install copies them too, so no disc is needed at run time.
//
// A disc is a directory. That is the whole portability story: an ISO mount, a
// 7z extract of one, or a real drive all scan the same way, and the tests
// synthesise one with `fx esa pack`.
//
// The three stages are deliberately split so that only the ends touch a disk:
//
//   install_scan     I/O    directory -> DiscSource (loose files, ESA dir, SSF)
//   install_plan     PURE   DiscSource[] -> InstallPlan (every byte accounted)
//   install_execute  I/O    InstallPlan -> files on disk
//   install_verify   I/O    re-derives each item and byte-compares
//
// install_plan is a pure function of scanned metadata, so the unit tests and
// the fuzzer drive the whole decision layer — script selection, glob matching,
// the clobber guard, media fingerprinting — entirely in memory.

namespace fx {

// --- scan -----------------------------------------------------------------

struct DiscFile {          // a file in a disc root
    std::string name;      // as it appears on disk (case is not normalised)
    uint64_t    size = 0;
};

struct DiscScript {        // a .SSF from a disc root
    std::string name;      // "SETUP.SSF"
    SsfDoc      doc;
};

struct DiscSource {
    std::string             root;      // the directory that was scanned
    int                     disc = 0;  // 1, 2, or 0 = unrecognised
    std::vector<DiscFile>   loose;     // files in the root (not recursed)
    std::string             esa_name;  // the archive in the root, "" if none
    std::vector<EsaEntry>   esa;       // its directory (empty on disc 2)
    std::vector<DiscScript> scripts;   // every .SSF in the root
};

// Which disc this is, from its contents — never from a volume label or the
// directory name, both of which vary with how the media was mounted. Disc 1
// carries the archive and the scripts; disc 2 carries LIBs and nothing else.
// Order-independent: probing an arbitrary directory yields 0.
int install_probe_disc(const DiscSource& disc);

// Read a disc root: list the loose files, parse the ESA directory if there is
// one, parse every .SSF, then probe. The only read in stage 1. A directory that
// cannot be read comes back with disc == 0.
DiscSource install_scan(const std::string& root);

// --- plan -----------------------------------------------------------------

// The DOS glob the .SSF scripts use, ASCII case-insensitive: `*.*` selects
// every entry (as in DOS, including names with no dot), `*` and `?` wildcard
// within a name, and anything else is an exact name match.
bool install_match(const std::string& pattern, const std::string& name);

enum class InstallOrigin {
    Archive,   // an ESA entry, named by an INSTALL_FILES directive
    Loose,     // a file sitting in a disc root (the CD-resident LIBs)
};

enum class InstallStatus {
    Copy,          // write it
    KeepExisting,  // a file is already there: user data, or no --overwrite
    SkipSysfile,   // INSTALL_SYSFILES: the Windows system directory, which we
                   // have no business writing to. Recorded, never written.
};

struct InstallItem {
    std::string   dest;                              // relative to the install dir
    InstallStatus status = InstallStatus::Copy;
    InstallOrigin origin = InstallOrigin::Archive;
    size_t        disc   = 0;                        // index into the discs vector
    std::string   source;                            // ESA entry, or loose file name
    std::string   label;                             // ESA label; "" when loose
    uint64_t      bytes  = 0;                        // uncompressed size
    std::string   note;                              // why, when it is not a Copy
};

// Every statement in every script that was read, and whether the engine acted on
// it. The ones it cannot act on (REGEXE, ADD_GROUP, DESKTOP_ITEM, DIRECTX, …)
// are Windows shell and registry work, and they are reported rather than
// silently dropped: an install that quietly ignores half its script is not a
// documented install.
struct InstallDirective {
    std::string              script;
    size_t                   line = 0;   // index into the script's TxtDoc
    std::string              keyword;
    std::vector<std::string> args;
    bool                     honored = false;
    std::string              note;
};

// Which build the media carries, fingerprinted from the ESA directory. The disc
// is 1.00F; the official patch lifts an install to 1.02F, which is the build the
// symbol database describes (ESA.md § File Inventory, "Build note").
enum class MediaBuild { Unknown, V100F, V102F };

const char* install_build_name(MediaBuild build);

struct InstallOptions {
    bool full        = true;   // the full script; false selects the minimal one
    bool cd_resident = true;   // also copy the loose LIBs the archive does not carry
    bool overwrite   = false;  // replace files already in the destination
    bool allow_unknown_media = false;  // proceed on media we cannot fingerprint
};

struct InstallPlan {
    MediaBuild                    build = MediaBuild::Unknown;
    std::string                   company;       // COMPANY_NAME
    std::string                   app_name;      // APP_NAME
    std::string                   default_path;  // DEFAULT_PATH (no drive letter)
    std::string                   script;        // the sub-script that was chosen
    std::vector<InstallItem>      items;
    std::vector<InstallDirective> directives;
    std::vector<std::string>      errors;  // non-empty: execute refuses to run
    uint64_t                      bytes = 0;     // sum over the Copy items
};

// Resolve the scripts against the discs. Pure: `existing` is the destination
// directory's file names (install_list_dir), and nothing here touches a disk.
//
// Script choice is data-driven, not label-driven: SETUP.SSF's INSTALL_SCRIPT
// labels are localised strings, so both sub-scripts are resolved and the one
// with more files is the full install (on the retail disc the two differ by
// exactly FA_4B.LIB, the digital-music archive).
InstallPlan install_plan(const std::vector<DiscSource>& discs,
                         const std::vector<std::string>& existing,
                         const InstallOptions& opt);

// --- execute --------------------------------------------------------------

// Per-item progress. A function pointer with a userdata word, not a
// std::function: fa-bridge links fx_lib into a shared plugin.
typedef void (*InstallProgress)(const InstallItem& item, uint64_t done,
                                uint64_t total, void* user);

// Names in `dir`, non-recursive. Empty if it does not exist.
std::vector<std::string> install_list_dir(const std::string& dir);

// Write the plan's Copy items to `dest`. Refuses a plan that carries errors.
// Every write goes to a `.part` file and is renamed once complete, so an
// interrupted install leaves no file that looks finished. Payloads stream:
// the 160 MB CD-resident LIBs never sit in memory. False on any failure, with
// the reasons appended to *errors (if non-null).
bool install_execute(const std::vector<DiscSource>& discs, const InstallPlan& plan,
                     const std::string& dest, InstallProgress progress, void* user,
                     std::vector<std::string>* errors);

// Re-derive every Copy item from the discs and byte-compare it against what is
// in `dest`. The proof, not a checksum of a checksum: fx_lib has no hash, and
// comparing against the source bytes is a stronger statement anyway.
bool install_verify(const std::vector<DiscSource>& discs, const InstallPlan& plan,
                    const std::string& dest, std::vector<std::string>* errors);

} // namespace fx
