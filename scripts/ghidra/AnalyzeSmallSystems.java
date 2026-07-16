// Five small unread systems (#493): radio/speech (MSG*/SAY*), effects (GRAPHICAdd*),
// weather (WR*), airports (AP*), multiplayer chat (CHAT*). Each cluster is dumped whole
// (named anchors + the FUN_ statics between them). WRCanSee gets a caller sweep — the
// issue asks whether the visibility model gates AI target acquisition.
public class AnalyzeSmallSystems extends FAScript {
    @Override
    public void run() throws Exception {
        openOutput("AnalyzeSmallSystems");

        header("RADIO -- MSG core (0x418070-0x418a48)");
        dumpRange(0x00418070L, 0x00418a48L);
        header("RADIO -- SAY core + message builders (0x48d2b0-0x48ec50)");
        dumpRange(0x0048d2b0L, 0x0048ec50L);
        header("RADIO -- SAYTranslate / fort reports (0x490f30-0x491240)");
        dumpRange(0x00490f30L, 0x00491240L);

        header("CHAT -- the whole cluster (0x413120-0x413c68)");
        dumpRange(0x00413120L, 0x00413c68L);

        header("EFFECTS -- the GRAPHIC spawn block (0x442c00-0x4448ff)");
        dumpRange(0x00442c00L, 0x004448ffL);

        header("WEATHER -- WR core (0x4b3170-0x4b4ba4)");
        dumpRange(0x004b3170L, 0x004b4ba4L);
        header("WEATHER -- palette tint helpers (0x4c8e20-0x4c8f79)");
        dumpRange(0x004c8e20L, 0x004c8f79L);
        header("WEATHER -- ZONE leftovers: ZONEInit / ZONEUpdate");
        dumpAt(0x00421c70L);
        dumpAt(0x00421dd0L);
        header("WEATHER -- who calls WRCanSee (does it gate AI acquisition?)");
        dumpCallers(0x004b4b30L);

        header("AIRPORTS -- AP core (0x4ba7e0-0x4bad40)");
        dumpRange(0x004ba7e0L, 0x004bad40L);
        header("AIRPORTS -- parking (0x4bd2d0-0x4bd510)");
        dumpRange(0x004bd2d0L, 0x004bd510L);
        header("AIRPORTS -- approach/carrier (0x4be6a0-0x4bee60)");
        dumpRange(0x004be6a0L, 0x004bee60L);
        header("AIRPORTS -- _APCommentProc (0x48f6a0)");
        dumpAt(0x0048f6a0L);

        closeOutput();
    }
}
