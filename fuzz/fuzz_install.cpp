// Fuzz target: the install engine's decision layer — script parsing, glob
// matching, archive resolution, the clobber guard, media fingerprinting.
//
// install_plan is a pure function of scanned metadata, which is exactly why it
// can be fuzzed: no disc, no destination, no I/O. The input is split into a
// hostile .SSF script and a hostile .ESA directory, and the two are handed to
// the planner as if a disc had been scanned. install_scan/execute/verify touch
// a filesystem and are covered by tests/test_install.cpp instead.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <fx/install.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 4) return 0;

    // A 16-bit header splits the buffer: script text, then archive bytes. Wide
    // enough to reach past a realistic script (the retail FINSTALL.SSF is 5,786
    // bytes) into the archive on the other side.
    const size_t hdr   = 2;
    const size_t span  = size - hdr;
    const size_t split = hdr + (size_t)((data[0] | (data[1] << 8)) % span);
    const uint8_t* ssf = data + hdr;
    const size_t   ssf_size = split - hdr;
    const uint8_t* esa = data + split;
    const size_t   esa_size = size - split;

    fx::DiscSource disc;
    disc.root     = "/disc";
    disc.esa_name = "SETUP.ESA";
    disc.esa      = fx::esa_read_dir(esa, esa_size);
    disc.scripts  = {{"SETUP.SSF", fx::ssf_read(ssf, ssf_size)},
                     {"FINSTALL.SSF", fx::ssf_read(ssf, ssf_size)},
                     {"MINSTALL.SSF", fx::ssf_read(esa, esa_size)}};
    disc.loose    = {{"SETUP.ESA", esa_size}, {"SETUP.SSF", ssf_size},
                     {"FINSTALL.SSF", ssf_size}, {"MINSTALL.SSF", esa_size},
                     {"FA_4C.LIB", 1}};
    disc.disc     = fx::install_probe_disc(disc);

    // A destination that already holds some of what the plan wants: the
    // clobber-guard path.
    const std::vector<std::string> existing = {"FA.EXE", "EXAMPLE.MT", "EA.CFG"};

    for (int i = 0; i < 4; i++) {
        fx::InstallOptions opt;
        opt.full                = (i & 1) != 0;
        opt.overwrite           = (i & 2) != 0;
        opt.allow_unknown_media = true;  // otherwise every input stops at the print
        const fx::InstallPlan plan = fx::install_plan({disc}, existing, opt);
        for (const auto& item : plan.items)
            fx::install_match(item.dest, item.source);
    }
    return 0;
}
