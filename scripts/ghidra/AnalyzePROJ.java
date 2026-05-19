// Consolidates: DumpPROJDispatch, DumpPROJPhysics, DumpPROJPhysics2,
//               DumpPROJPhysics3, DumpPROJPhysicsInit

public class AnalyzePROJ extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzePROJ");
        analyzePROJ();
        closeOutput();
    }
}
