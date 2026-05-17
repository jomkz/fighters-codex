#include <cstdio>
#include <cstring>
#include <string>
#include "ft/version.h"

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

static void print_usage() {
    puts("ft -- Fighters Toolkit\n");
    puts("Usage:  ft <command> <subcommand> [options]\n");
    puts("Library commands:");
    puts("  ft lib ls     <file.LIB>");
    puts("  ft lib unpack <file.LIB> [output_dir]");
    puts("  ft lib pack   <dir>      <output.LIB>");
    puts("  ft lib patch  <src.LIB>  <name> <file> <output.LIB>");
    puts("");
    puts("Picture commands:");
    puts("  ft pic info   <file.PIC>");
    puts("  ft pic unpack <file.PIC> [-p PALETTE.PAL] [-o output.png]");
    puts("  ft pic pack   <file.png> [-p PALETTE.PAL] [-o output.PIC]");
    puts("");
    puts("Sequence commands:");
    puts("  ft seq dump   <file.SEQ>");
    puts("  ft seq unpack <file.SEQ> [-o output.txt]");
    puts("  ft seq pack   <file.txt> -o <output.SEQ>");
    puts("");
    puts("Audio commands:");
    puts("  ft audio info   <file.11K|.5K|.8K> [-r hz]");
    puts("  ft audio unpack <file.11K|.5K|.8K> [-o output.wav] [-r hz]");
    puts("  ft audio pack   <file.wav>          -o <output.11K|.5K> [-r hz]");
    puts("");
    puts("Mission file commands:");
    puts("  ft mission info   <file.M|.MM>");
    puts("  ft mission unpack <file.M|.MM> [-o output.txt]");
    puts("  ft mission pack   <file.txt>   -o <output.M|.MM>");
    puts("  ft mm info/unpack/pack (alias for mission)");
    puts("");
    puts("Shape file commands:");
    puts("  ft sh info   <file.SH>");
    puts("  ft sh unpack <file.SH> [-o output.obj]");
    puts("");
    puts("CB8 video commands:");
    puts("  ft cb8 info   <file.CB8>");
    puts("  ft cb8 frames <file.CB8> [-o output_dir]");
    puts("");
    puts("Screenshot commands:");
    puts("  ft raw info   <file.RAW>");
    puts("  ft raw unpack <file.RAW> [-o output.png]");
    puts("");
    puts("Type file commands (BRF format -- OT/NT/PT/JT/SEE/ECM/GAS):");
    puts("  ft ot  info   <file.OT>  (or pt/nt/jt/see/ecm/gas)");
    puts("  ft ot  unpack <file.OT>  [-o output.txt]");
    puts("  ft ot  pack   <file.txt> -o <output.OT>");
    puts("");
    puts("Pilot save commands:");
    puts("  ft plt info   <file.P>");
    puts("");
    puts("Symbol map commands:");
    puts("  ft sms dump   <FA.SMS> [-o out.csv]");
    puts("");
    puts("Terrain map commands:");
    puts("  ft t2  info   <file.T2>");
}

int main(int argc, char** argv) {
    if (argc < 2) { print_usage(); return 1; }

    const char* cmd = argv[1];
    if (strcmp(cmd, "--version") == 0 || strcmp(cmd, "-v") == 0) {
        puts("ft " FT_VERSION_STRING);
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
    if (strcmp(cmd, "ot")  == 0 || strcmp(cmd, "nt")  == 0 ||
        strcmp(cmd, "pt")  == 0 || strcmp(cmd, "jt")  == 0 ||
        strcmp(cmd, "see") == 0 || strcmp(cmd, "ecm") == 0 ||
        strcmp(cmd, "gas") == 0)
        return cmd_ot(argc - 1, argv + 1, cmd);

    fprintf(stderr, "Unknown command: %s\n", cmd);
    print_usage();
    return 1;
}
