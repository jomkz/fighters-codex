// Consolidates: DumpMUSCallbacks, DumpMUSInterpreter, DumpSEQFunctions

public class AnalyzeMUS extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeMUS");
        header("MUS -- _SEQmusic (0x446b70)");
        dumpAtForced(0x00446b70L);
        header("MUS -- _SEQfadein (0x446890)");
        dumpAtForced(0x00446890L);
        header("MUS -- _SEQfadeout (0x446910)");
        dumpAtForced(0x00446910L);
        header("MUS -- ?SeqFadeOut (0x446990)");
        dumpAtForced(0x00446990L);
        header("MUS -- ?SeqFadeIn (0x4469f0)");
        dumpAtForced(0x004469f0L);
        header("MUS -- ?MusicOn (0x4329e0)");
        dumpAt(0x004329e0L);
        header("MUS -- ?MusicOff (0x432c00)");
        dumpAt(0x00432c00L);
        header("MUS -- ?DMusicOff (0x432bd0)");
        dumpAt(0x00432bd0L);
        header("MUS -- ?DMusicVolume (0x432a80)");
        dumpAt(0x00432a80L);
        header("MUS -- ?MusicVolume (0x432b40)");
        dumpAt(0x00432b40L);
        header("MUS -- ?ShellMusicUpdate (0x432f80)");
        dumpAt(0x00432f80L);
        header("MUS -- ?ShellMusic (0x433170)");
        dumpAt(0x00433170L);
        header("MUS -- ?InitMusic (0x4328b0)");
        dumpAtForced(0x004328b0L);
        header("MUS -- ?ShutDownMidi (0x432920)");
        dumpAtForced(0x00432920L);
        header("MUS -- loader 1 (0x4a6ae0)");
        dumpAt(0x004a6ae0L);
        header("MUS -- loader 2 (0x4a6b50)");
        dumpAt(0x004a6b50L);
        header("MUS -- loader 3 (0x4a7180)");
        dumpAt(0x004a7180L);
        header("MUS -- AIL callback registration");
        dumpSymbolsMatching("ail_init", "ail_register", "ailcallback", "ail_sequence");
        searchStrings(new String[]{"AIL_init_sequence", "AIL_register"});
        header("MUS -- symbols matching mus/music/seq/midi/sound/audio/ail");
        dumpSymbolsMatching("mus", "music", "seq", "midi", "sound", "audio", "ail", "fade");
        closeOutput();
    }
}
