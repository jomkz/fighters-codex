// Consolidates: DumpMCCAMLoader, DumpMMCAMMission, DumpMMLayerSlot,
//               DumpMMLayerSlot2, DumpMMLayerSlot3, DumpMMLayerSlot4

public class AnalyzeMM extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeMM");
        analyzeMM();
        closeOutput();
    }
}
