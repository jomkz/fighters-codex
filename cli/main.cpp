#include <cstdio>
#include <cstring>
#include <string>
#include "fx/version.h"

// Forward declarations from command modules
int cmd_lib(int argc, char** argv);
int cmd_pic(int argc, char** argv);
int cmd_seq(int argc, char** argv);
int cmd_audio(int argc, char** argv);
int cmd_ot(int argc, char** argv, const char* format);
int cmd_mission(int argc, char** argv);
int cmd_sh(int argc, char** argv);
int cmd_cb8(int argc, char** argv);
int cmd_raw(int argc, char** argv);
int cmd_sms(int argc, char** argv);
int cmd_t2(int argc, char** argv);
int cmd_plt(int argc, char** argv);
int cmd_pal(int argc, char** argv);
int cmd_inf(int argc, char** argv);
int cmd_hud(int argc, char** argv);
int cmd_lay(int argc, char** argv);
int cmd_fnt(int argc, char** argv);
int cmd_mus(int argc, char** argv);
int cmd_bi(int argc, char** argv);
int cmd_ai(int argc, char** argv);
int cmd_fbc(int argc, char** argv);
int cmd_bin(int argc, char** argv);
int cmd_cam(int argc, char** argv);
int cmd_txt(int argc, char** argv);
int cmd_cfg(int argc, char** argv);
int cmd_dat(int argc, char** argv);
int cmd_effect(int argc, char** argv);
int cmd_mnu(int argc, char** argv);
int cmd_mt(int argc, char** argv);
int cmd_pts(int argc, char** argv);
int cmd_rgn(int argc, char** argv);
int cmd_ssf(int argc, char** argv);
int cmd_mc(int argc, char** argv);
int cmd_hgr(int argc, char** argv);
int cmd_dlg(int argc, char** argv);
int cmd_xmi(int argc, char** argv);
int cmd_vdo(int argc, char** argv);

static void print_usage() {
    puts("fx -- Fighters Codex\n");
    puts("Usage:  fx <command> <subcommand> [options]\n");
    puts("Library commands:");
    puts("  fx lib ls      <file.LIB>");
    puts("  fx lib unpack  <file.LIB> [output_dir]");
    puts("  fx lib extract <file.LIB> <NAME> [NAME ...] [-o output_dir]");
    puts("  fx lib pack    <dir>      <output.LIB>");
    puts("  fx lib patch   <src.LIB>  <name> <file> <output.LIB>");
    puts("");
    puts("Picture commands:");
    puts("  fx pic info   <file.PIC>");
    puts("  fx pic unpack <file.PIC> [-p PALETTE.PAL] [-o output.png]");
    puts("  fx pic pack   <file.png> [-p PALETTE.PAL] [-o output.PIC]");
    puts("");
    puts("Sequence commands:");
    puts("  fx seq dump   <file.SEQ>");
    puts("  fx seq unpack <file.SEQ> [-o output.txt]");
    puts("  fx seq pack   <file.txt> -o <output.SEQ>");
    puts("");
    puts("Audio commands:");
    puts("  fx audio info   <file.11K|.5K|.8K> [-r hz]");
    puts("  fx audio unpack <file.11K|.5K|.8K> [-o output.wav] [-r hz]");
    puts("  fx audio pack   <file.wav>          -o <output.11K|.5K> [-r hz]");
    puts("");
    puts("Mission file commands:");
    puts("  fx mission info   <file.M|.MM>");
    puts("  fx mission unpack <file.M|.MM> [-o output.txt]");
    puts("  fx mission pack   <file.txt>   -o <output.M|.MM>");
    puts("  fx mm info/unpack/pack (alias for mission)");
    puts("");
    puts("Shape file commands:");
    puts("  fx sh info   <file.SH>");
    puts("  fx sh unpack <file.SH> [-o output.obj]");
    puts("");
    puts("CB8 video commands:");
    puts("  fx cb8 info   <file.CB8>");
    puts("  fx cb8 frames <file.CB8> [-o output_dir]");
    puts("");
    puts("Screenshot commands:");
    puts("  fx raw info   <file.RAW>");
    puts("  fx raw unpack <file.RAW> [-o output.png]");
    puts("");
    puts("Type file commands (BRF format -- OT/NT/PT/JT/SEE/ECM/GAS):");
    puts("  fx ot  info   <file.OT>  (or pt/nt/jt/see/ecm/gas)");
    puts("  fx ot  unpack <file.OT>  [-o output.txt]");
    puts("  fx ot  pack   <file.txt> -o <output.OT>");
    puts("");
    puts("Pilot save commands:");
    puts("  fx plt info   <file.P>");
    puts("  fx plt dump   <file.P>");
    puts("");
    puts("Symbol map commands:");
    puts("  fx sms dump   <FA.SMS> [-o out.csv]");
    puts("");
    puts("Terrain map commands:");
    puts("  fx t2  info      <file.T2>");
    puts("  fx t2  dump      <file.T2> [--leaves]");
    puts("  fx t2  heightmap <file.T2> <out.png>");
    puts("");
    puts("Palette commands:");
    puts("  fx pal info   <file.PAL>");
    puts("  fx pal dump   <file.PAL> [-o out.png]");
    puts("");
    puts("Aircraft tech sheet commands:");
    puts("  fx inf dump   <file.INF>");
    puts("");
    puts("HUD layout commands:");
    puts("  fx hud dump   <file.HUD>");
    puts("  fx hud set    <file.HUD> <gauge.field=value ...> [-o out.HUD]");
    puts("");
    puts("Sky/atmosphere layer commands:");
    puts("  fx lay dump     <file.LAY>");
    puts("  fx lay gradient <file.LAY> [-o output.png]");
    puts("  fx lay set      <file.LAY> <key=value ...> [-o out.LAY]");
    puts("");
    puts("Font commands:");
    puts("  fx fnt info   <file.FNT>");
    puts("  fx fnt unpack <file.FNT> [-o output_dir]");
    puts("");
    puts("Music playlist commands:");
    puts("  fx mus dump   <file.MUS>");
    puts("");
    puts("BI disassembler commands:");
    puts("  fx bi dump      <file.BI>");
    puts("  fx bi decompile <file.BI>");
    puts("");
    puts("AI compiler commands:");
    puts("  fx ai compile <file.AI> -o <file.BI>");
    puts("");
    puts("Video frame index commands:");
    puts("  fx fbc info <file.FBC>");
    puts("  fx fbc ls   <file.FBC>");
    puts("");
    puts("Lookup table commands:");
    puts("  fx bin info <file.BIN>");
    puts("");
    puts("Campaign DLL commands:");
    puts("  fx cam info    <file.CAM>");
    puts("  fx cam strings <file.CAM> [-n MIN]");
    puts("");
    puts("In-game text commands:");
    puts("  fx txt info <file.TXT>");
    puts("");
    puts("Game configuration commands:");
    puts("  fx cfg info <EA.CFG>");
    puts("  fx dat info <NET.DAT|MODEM.DAT|SERIAL.DAT>");
    puts("");
    puts("GRAPHIC effect commands:");
    puts("  fx effect types                # effect type -> class/shape map");
    puts("  fx effect dump  <table.bin>    # decode 0x30-byte param records");
    puts("  fx effect spawn <record.bin>   # decode a MSG 0x8003 spawn record");
    puts("");
    puts("Menu DLL commands:");
    puts("  fx mnu info    <file.MNU>");
    puts("  fx mnu strings <file.MNU> [-n MIN]");
    puts("");
    puts("Mission briefing text commands:");
    puts("  fx mt info <file.MT>");
    puts("");
    puts("Aircraft screen asset commands:");
    puts("  fx pts info <file.PTS>");
    puts("");
    puts("Installer region map commands:");
    puts("  fx rgn info <file.RGN>");
    puts("  fx rgn dump <file.RGN>");
    puts("");
    puts("Installer script commands:");
    puts("  fx ssf info <file.SSF>");
    puts("  fx ssf dump <file.SSF>");
    puts("");
    puts("Mission condition DLL commands:");
    puts("  fx mc  info    <file.MC>");
    puts("  fx mc  strings <file.MC> [-n MIN]");
    puts("");
    puts("Hangar screen DLL commands:");
    puts("  fx hgr info    <file.HGR>");
    puts("  fx hgr strings <file.HGR> [-n MIN]");
    puts("");
    puts("Menu dialog DLL commands:");
    puts("  fx dlg info    <file.DLG>");
    puts("  fx dlg strings <file.DLG> [-n MIN]");
    puts("");
    puts("Extended MIDI commands:");
    puts("  fx xmi info   <file.XMI>");
    puts("  fx xmi export <file.XMI> [-s N] -o <out.mid>");
    puts("");
    puts("Briefing video commands:");
    puts("  fx vdo info   <file.VDO> [file.FBC]");
    puts("  fx vdo export <file.VDO> <file.FBC> [-o dir]");
}

int main(int argc, char** argv) {
    if (argc < 2) { print_usage(); return 1; }

    const char* cmd = argv[1];
    if (strcmp(cmd, "--version") == 0 || strcmp(cmd, "-v") == 0) {
        puts("fx " FX_VERSION_STRING);
        return 0;
    }
    if (strcmp(cmd, "lib") == 0) return cmd_lib(argc - 1, argv + 1);
    if (strcmp(cmd, "pic") == 0) return cmd_pic(argc - 1, argv + 1);
    if (strcmp(cmd, "seq")   == 0) return cmd_seq(argc - 1, argv + 1);
    if (strcmp(cmd, "audio") == 0) return cmd_audio(argc - 1, argv + 1);
    // BRF type formats (all route through cmd_ot with the format name)
    if (strcmp(cmd, "mission") == 0 || strcmp(cmd, "mm") == 0)
        return cmd_mission(argc - 1, argv + 1);
    if (strcmp(cmd, "sh") == 0)
        return cmd_sh(argc - 1, argv + 1);
    if (strcmp(cmd, "cb8") == 0)
        return cmd_cb8(argc - 1, argv + 1);
    if (strcmp(cmd, "raw") == 0)
        return cmd_raw(argc - 1, argv + 1);
    if (strcmp(cmd, "sms") == 0)
        return cmd_sms(argc - 1, argv + 1);
    if (strcmp(cmd, "t2")  == 0)
        return cmd_t2(argc - 1, argv + 1);
    if (strcmp(cmd, "plt") == 0)
        return cmd_plt(argc - 1, argv + 1);
    if (strcmp(cmd, "pal") == 0)
        return cmd_pal(argc - 1, argv + 1);
    if (strcmp(cmd, "inf") == 0)
        return cmd_inf(argc - 1, argv + 1);
    if (strcmp(cmd, "hud") == 0)
        return cmd_hud(argc - 1, argv + 1);
    if (strcmp(cmd, "lay") == 0)
        return cmd_lay(argc - 1, argv + 1);
    if (strcmp(cmd, "fnt") == 0)
        return cmd_fnt(argc - 1, argv + 1);
    if (strcmp(cmd, "mus") == 0)
        return cmd_mus(argc - 1, argv + 1);
    if (strcmp(cmd, "bi")  == 0)
        return cmd_bi(argc - 1, argv + 1);
    if (strcmp(cmd, "ai")  == 0)
        return cmd_ai(argc - 1, argv + 1);
    if (strcmp(cmd, "fbc") == 0)
        return cmd_fbc(argc - 1, argv + 1);
    if (strcmp(cmd, "bin") == 0)
        return cmd_bin(argc - 1, argv + 1);
    if (strcmp(cmd, "cam") == 0)
        return cmd_cam(argc - 1, argv + 1);
    if (strcmp(cmd, "txt") == 0)
        return cmd_txt(argc - 1, argv + 1);
    if (strcmp(cmd, "cfg") == 0)
        return cmd_cfg(argc - 1, argv + 1);
    if (strcmp(cmd, "dat") == 0)
        return cmd_dat(argc - 1, argv + 1);
    if (strcmp(cmd, "effect") == 0)
        return cmd_effect(argc - 1, argv + 1);
    if (strcmp(cmd, "mnu") == 0)
        return cmd_mnu(argc - 1, argv + 1);
    if (strcmp(cmd, "mt") == 0)
        return cmd_mt(argc - 1, argv + 1);
    if (strcmp(cmd, "pts") == 0)
        return cmd_pts(argc - 1, argv + 1);
    if (strcmp(cmd, "rgn") == 0)
        return cmd_rgn(argc - 1, argv + 1);
    if (strcmp(cmd, "ssf") == 0)
        return cmd_ssf(argc - 1, argv + 1);
    if (strcmp(cmd, "mc") == 0)
        return cmd_mc(argc - 1, argv + 1);
    if (strcmp(cmd, "hgr") == 0)
        return cmd_hgr(argc - 1, argv + 1);
    if (strcmp(cmd, "dlg") == 0)
        return cmd_dlg(argc - 1, argv + 1);
    if (strcmp(cmd, "xmi") == 0)
        return cmd_xmi(argc - 1, argv + 1);
    if (strcmp(cmd, "vdo") == 0)
        return cmd_vdo(argc - 1, argv + 1);
    if (strcmp(cmd, "ot")  == 0 || strcmp(cmd, "nt")  == 0 ||
        strcmp(cmd, "pt")  == 0 || strcmp(cmd, "jt")  == 0 ||
        strcmp(cmd, "see") == 0 || strcmp(cmd, "ecm") == 0 ||
        strcmp(cmd, "gas") == 0)
        return cmd_ot(argc - 1, argv + 1, cmd);

    fprintf(stderr, "Unknown command: %s\n", cmd);
    print_usage();
    return 1;
}
