// T2 terrain overlay DLL analysis.
// The T2 binary format (BIT2 magic) is loaded by an overlay DLL whose location
// in FA.EXE has not yet been pinned down. This script searches for it via:
//   1. The BIT2 magic bytes string search
//   2. Symbol matches for terrain/surface/tile/atlas/class

public class AnalyzeT2DLL extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeT2DLL");
        analyzeT2DLL();
        closeOutput();
    }
}
