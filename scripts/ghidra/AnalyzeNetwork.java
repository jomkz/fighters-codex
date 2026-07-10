// Multiplayer networking protocol, CN_INFO struct mapping, and IP.EXE interface.
// Dark zone: 0x482200-0x4AACEF (169 KB, shared with mission eval) -- zero prior coverage.
// Invoke: run_ghidra.sh AnalyzeNetwork.java
// Output: $FA_PROJECT/output/AnalyzeNetwork.txt

public class AnalyzeNetwork extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeNetwork");
        header("CN -- CN_ReadConfig / CN_WriteConfig (CN_INFO struct)");
        dumpSymbolsMatching("cn_readconfig", "_cn_readconfig", "cn_writeconfig", "_cn_writeconfig",
                "cnreadconfig", "cnwriteconfig", "cn_init", "_cn_init", "cninit");
        header("CN -- CN_INFO struct field scan (offsets 0x00-0xddb, full struct)");
        for (long va : findFunctionsReadingOffsets(0x00400000L, 0x00540000L, 0x00, 0xddb)) dumpAt(va);
        header("MP -- ?MPReceive@@YGDXZ (0x46C980) and callers");
        dumpAt(0x0046c980L);
        dumpCallers(0x0046c980L);
        header("MP -- multiplayer state and symbols");
        dumpSymbolsMatching("mpreceive", "mpsend", "mpupdate", "mpinit", "mpshutdown",
                "mpframe", "mpsync", "mpstatus", "mpsession",
                "mpjoin", "mphost", "mpstart", "mpend", "mpkill",
                "?mpset", "?mpget", "?mpstatus", "?mprecv", "?mpsend",
                "_mprecv", "_mpsend", "_mpupdate", "_mpinit");
        header("MP -- range 0x482200-0x4AACEF");
        dumpRange(0x00482200L, 0x004aacefL);
        header("NET -- IPX transport");
        dumpSymbolsMatching("_ipxsend", "ipxsend", "_ipxrecv", "ipxrecv",
                "_ipxinit", "ipxinit", "_ipxopen", "ipxopen",
                "_ipxclose", "ipxclose", "_ipxpoll", "ipxpoll");
        searchStrings(new String[]{"IPX", "ipx", "Novell", "novell"});
        header("NET -- TCP/IP transport");
        dumpSymbolsMatching("_tcpsend", "tcpsend", "_tcprecv", "tcprecv",
                "_tcpinit", "tcpinit", "_tcpopen", "tcpopen",
                "_tcpclose", "tcpclose", "_tcpconnect", "tcpconnect");
        searchStrings(new String[]{"TCP", "tcp", "socket", "Socket", "winsock", "WinSock"});
        header("NET -- serial / modem transport");
        dumpSymbolsMatching("_serialsend", "serialsend", "_serialrecv", "serialrecv",
                "_modemsend", "modemsend", "_modemrecv", "modemrecv",
                "_comminit", "comminit", "_commopen", "commopen",
                "_commsend", "commsend", "_commrecv", "commrecv");
        searchStrings(new String[]{"COM", "modem", "Modem", "serial", "Serial"});
        header("MOD -- MOD_Initialize (0x49aff0) and callers");
        dumpAt(0x0049aff0L);
        dumpCallers(0x0049aff0L);
        header("MOD -- MOD_* symbol search");
        dumpSymbolsMatching("mod_", "_mod_", "modinit", "moddial", "modconnect", "modopen",
                "modclose", "modhangup", "moddetect", "modautodetect", "modanswer",
                "modatinit", "modatcmd", "phonebook", "phonebk", "phoneno", "phonenum",
                "dial", "_dial", "doconnect", "modport");
        searchStrings(new String[]{"ATZ", "ATDT", "ATE", "ATA", "ATH", "phone", "Phone",
                "phonebook", "PhoneBook", "NetBEUI", "netbeui", "NBF", "nbbios",
                "NBIOS", "NetBIOS", "netbios"});
        header("MOD -- CN_INFO unknown range [0xc0]-[0x8e3] offset scan in MOD area");
        for (long va : findFunctionsReadingOffsets(0x00498000L, 0x004ab000L, 0xc0, 0x8e3)) dumpAt(va);
        header("NET -- packet encode / decode");
        dumpSymbolsMatching("_packetinit", "packetinit", "_packencode", "packencode",
                "_packdecode", "packdecode", "_packetsend", "packetsend",
                "_packetrecv", "packetrecv", "_netpacket", "netpacket");
        header("NET -- session management");
        dumpSymbolsMatching("_netsession", "netsession", "_sessioninit", "sessioninit",
                "_sessionjoin", "sessionjoin", "_sessionhost", "sessionhost",
                "_sessionlist", "sessionlist", "_sessionclose", "sessionclose");
        searchStrings(new String[]{"session", "Session", "lobby", "Lobby", "host", "join"});
        header("MP -- player/entity sync over network");
        dumpSymbolsMatching("mpsetfuel", "?mpsetfuel", "mpsetpos", "mpsetstate",
                "mpsetweapon", "mpsetdamage", "mpentityupdate", "mpplayerupdate");
        closeOutput();
    }
}
