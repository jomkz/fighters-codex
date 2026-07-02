#include "fx/plt.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

using namespace fx;

static std::vector<uint8_t> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto sz = f.tellg(); f.seekg(0);
    std::vector<uint8_t> buf((size_t)sz);
    f.read((char*)buf.data(), sz);
    return buf;
}

// plt info <file.P>
static int cmd_info(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "Usage: fx plt info <file.P>\n"); return 1; }
    auto data = read_file(argv[1]);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", argv[1]); return 1; }

    PltInfo info;
    if (!plt_parse(data.data(), data.size(), &info)) {
        fprintf(stderr, "Not a valid pilot file: %s\n", argv[1]);
        return 1;
    }

    printf("File:       %s  (%zu bytes)\n", argv[1], data.size());
    printf("Name:       %s\n", info.name.c_str());
    printf("Callsign:   %s\n", info.callsign.c_str());
    printf("Rank:       %s\n", info.rank.c_str());
    if (!info.voice_file.empty())  printf("Voice:      %s\n", info.voice_file.c_str());
    if (!info.nose_art.empty())    printf("Nose art:   %s\n", info.nose_art.c_str());
    if (!info.left_decal.empty())  printf("Left decal: %s\n", info.left_decal.c_str());
    if (!info.right_decal.empty()) printf("Right decal:%s\n", info.right_decal.c_str());
    if (!info.portrait.empty())    printf("Portrait:   %s\n", info.portrait.c_str());

    if (!info.cam_file.empty()) {
        printf("\nCampaign:   %s", info.cam_file.c_str());
        if (!info.cam_name.empty()) printf("  (%s)", info.cam_name.c_str());
        printf("\n");
        if (!info.aircraft.empty())
            printf("Aircraft:   %s\n", info.aircraft.c_str());
        if (!info.aircraft_pool.empty()) {
            printf("Pool:       ");
            for (size_t i = 0; i < info.aircraft_pool.size(); i++) {
                if (i) printf(", ");
                printf("%s", info.aircraft_pool[i].c_str());
            }
            printf("\n");
        }
        if (!info.ordnance.empty()) {
            printf("Ordnance:\n");
            for (auto& o : info.ordnance)
                printf("  %-16s x%u\n", o.jt_name.c_str(), o.quantity);
        }
        if (!info.sensors.empty()) {
            printf("Sensors:    ");
            for (size_t i = 0; i < info.sensors.size(); i++) {
                if (i) printf(", ");
                printf("%s", info.sensors[i].c_str());
            }
            printf("\n");
        }
    } else {
        printf("\n(No active campaign)\n");
    }

    return 0;
}

// plt dump <file.P>  -- print confirmed stats block
static int cmd_dump(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "Usage: fx plt dump <file.P>\n"); return 1; }
    auto data = read_file(argv[1]);
    if (data.empty()) { fprintf(stderr, "Cannot read: %s\n", argv[1]); return 1; }

    PltInfo info;
    if (!plt_parse(data.data(), data.size(), &info)) {
        fprintf(stderr, "Not a valid pilot file: %s\n", argv[1]);
        return 1;
    }
    printf("Pilot: %s  Rank: %s\n\n", info.name.c_str(), info.rank.c_str());

    PltStats st;
    if (!plt_parse_stats(data.data(), data.size(), &st)) {
        printf("Stats block not present (file too small: %zu bytes, need 0x21F8).\n",
               data.size());
        return 0;
    }

    printf("=== Mission counters ===\n");
    printf("  Missions flown:        %u  (wingman: %u)\n", st.missions_flown, st.wingman_missions);
    printf("  Missions failed:       %u\n", st.missions_failed);
    printf("  Total shots fired:     %u\n", st.shots_fired_total);
    printf("  Ejections:             %u\n", st.ejections);
    printf("  Wingman KIA:           %u\n", st.wingman_kia);
    printf("  Player damage %%:       %u\n", st.player_damage_pct);
    printf("  Player landings:       %u  (score: %u)\n", st.player_landings, st.player_landing_score);
    printf("  Wingman landings:      %u  (score: %u)\n", st.wingman_landings, st.wingman_landing_score);

    printf("\n=== Kill tallies (player / wingman) ===\n");
    auto kill = [](const char* label, const PltKill& k) {
        printf("  %-28s %4u / %u\n", label, k.player, k.wingman);
    };
    kill("Aircraft / fighters:", st.kills_air_fighter);
    kill("Fighters (type B):", st.kills_air_fighter_b);
    kill("Aircraft (crash/BA weapon):", st.kills_air_crash);
    kill("Naval vessels:", st.kills_naval);
    kill("SAM launchers:", st.kills_sam);
    kill("AAA guns:", st.kills_aaa);
    kill("Armor / tanks:", st.kills_armor);
    kill("APCs:", st.kills_apc);
    kill("Vehicles / trucks:", st.kills_vehicle);
    kill("Infantry:", st.kills_infantry);
    kill("Friendly fire:", st.kills_friendly_fire);
    kill("Air (non-fighter):", st.kills_air_nonfighter);
    kill("Capital ships:", st.kills_capital_ship);

    printf("\n=== Weapon accuracy (player / wingman) ===\n");
    printf("  %-18s  %8s  %8s  %8s  %8s  %8s\n",
           "Group", "damage", "shots", "hits", "type3", "kills");
    auto wpn = [](const char* label, const PltWpnGroup& g) {
        printf("  %-18s  P:%6u  %6u  %6u  %6u  %6u\n", label,
               g.player.damage_total, g.player.shots_fired,
               g.player.hits, g.player.type3, g.player.kills);
        printf("  %-18s  W:%6u  %6u  %6u  %6u  %6u\n", "",
               g.wingman.damage_total, g.wingman.shots_fired,
               g.wingman.hits, g.wingman.type3, g.wingman.kills);
    };
    wpn("AA gun:", st.wpn_aa_gun);
    wpn("AA missile:", st.wpn_aa_missile);
    wpn("Ground attack:", st.wpn_ground);
    wpn("Naval attack:", st.wpn_naval);
    wpn("Kill (aircraft):", st.wpn_kill_aircraft);
    wpn("Kill (type B):", st.wpn_kill_b);
    wpn("Kill (type C):", st.wpn_kill_c);
    wpn("Kill (type D):", st.wpn_kill_d);

    return 0;
}

int cmd_plt(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: fx plt <info|dump> ...\n");
        return 1;
    }
    if (strcmp(argv[1], "info") == 0) return cmd_info(argc - 1, argv + 1);
    if (strcmp(argv[1], "dump") == 0) return cmd_dump(argc - 1, argv + 1);
    fprintf(stderr, "Unknown plt subcommand: %s\n", argv[1]);
    return 1;
}
