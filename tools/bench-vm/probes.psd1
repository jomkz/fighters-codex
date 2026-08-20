# Probe table for the bench watcher — the regions #56 needs observed, addressed by their absolute
# FA.EXE virtual address. FA.EXE is a 1998 non-relocatable PE (ImageBase 0x400000, no reloc
# table), so these VAs are valid directly in the running process; plt-watch.ps1 still resolves the
# FA.EXE module base and rebases, in case a future OS ever loads it elsewhere.
#
# `_campaignPilot` is at VA 0x004F8BB8 (db/inventory/FA.EXE/globals.csv); the .P file image starts
# at that VA, so file offset N in docs/fa/formats/P.md is VA 0x004F8BB8 + N. The four gap regions
# and the (now statically mapped) medal band are seeded below. HUD probes from #142 need the HUD
# struct base — fill their VAs from db/symbols when that campaign is run.
@{
    Watch = @(
        # --- #29 PLT gap regions (file offset -> VA = 0x4F8BB8 + offset) ---
        @{ Name = 'gap1-rank-score';      VA = 0x004F8C68; Len = 0x12;  Note = 'PLT 0xB0-0xC1: rank index / score tier?' }
        @{ Name = 'gap2-mission-log-a';   VA = 0x004F8C87; Len = 0x4A3; Note = 'PLT 0xCF-0x571: mission log, pre-medal-band' }
        @{ Name = 'medal-band';           VA = 0x004F912A; Len = 0x0B;  Note = 'PLT 0x572-0x57C: VALIDATION — an award must flip exactly one byte here (P.md medal map)' }
        @{ Name = 'gap2-mission-log-b';   VA = 0x004F9135; Len = 0x32;  Note = 'PLT 0x57D-0x5AE: mission log remainder' }
        @{ Name = 'service-record';       VA = 0x004F9167; Len = 0x1D0; Note = 'PLT 0x5AF-: _AwardMedal citation strings (read-only in codec)' }
        @{ Name = 'gap3-kill-subcats';    VA = 0x004FABD0; Len = 0xA0;  Note = 'PLT 0x2018-0x20B7: between kill tallies and accuracy' }
        @{ Name = 'gap4-fort-stats';      VA = 0x004FADB0; Len = 0x3E8; Note = 'PLT 0x21F8-0x25DF: fort/campaign-phase stats' }

        # --- #142 runtime probes — fill VAs from db/symbols before flying that campaign ---
        # @{ Name = 'hud-byte-238';  VA = 0x00000000; Len = 0x1; Note = 'HUD struct +0x238 (HUD.md front-matter gap)' }
        # @{ Name = 'hud-flags-word';VA = 0x00000000; Len = 0x4; Note = 'HUD flags-word bit 14, single-player trigger' }
    )
}
