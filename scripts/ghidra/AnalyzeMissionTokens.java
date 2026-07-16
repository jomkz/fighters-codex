// The .M/.MT tokenizer (#485): the lexer MISSIONTextProc drives — TextNextToken /
// TextNextNumber / TextTokenToValue — plus the section finder. Recovers the grammar.
public class AnalyzeMissionTokens extends FAScript {
    @Override
    public void run() throws Exception {
        openOutput("AnalyzeMissionTokens");
        header("TextNextToken (0x483c90)"); dumpAt(0x00483c90L);
        header("TextNextNumber (0x483d30)"); dumpAt(0x00483d30L);
        header("TextTokenToValue (0x483d50)"); dumpAt(0x00483d50L);
        header("@FindSection@8 (0x47d2c0)"); dumpAt(0x0047d2c0L);
        header("FindSectionHeader (0x47d200)"); dumpAt(0x0047d200L);
        header("FormatText (0x446d90)"); dumpAt(0x00446d90L);
        closeOutput();
    }
}
