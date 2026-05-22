#include "fx/ot.h"
#include <cstdio>
#include <cstring>

namespace fx {

// ---------------------------------------------------------------------------
// OT general section (struct_type 1..7 all share this header)
// Derived from OpenFA crates/asset/ot/src/lib.rs
// ---------------------------------------------------------------------------
const OtField OT_GENERAL_FIELDS[] = {
    // --- general info ---
    { "struct_type",        "1=OT 3=NT 5=PT 7=JT" },
    { "type_size",          "bytes" },
    { "instance_size",      "bytes" },
    { "names",              "ptr -> short, long, filename" },
    { "ot_flags",           "bitfield" },
    { "obj_class",          "$8000=Fighter $4000=Bomber $2000=Ship $1000=SAM $800=AAA $400=Tank $200=Vehicle $100=Struct $80=Projectile" },
    { "shape",              "ptr -> .SH filename" },
    { "shadow_shape",       "ptr -> .SH filename (or dword 0)" },
    { "dmg_debris_pos_x",   "i16 feet (V2+)" },
    { "dmg_debris_pos_y",   "i16 feet (V2+)" },
    { "dmg_debris_pos_z",   "i16 feet (V2+)" },
    { "dst_debris_pos_x",   "i16 feet (V2+)" },
    { "dst_debris_pos_y",   "i16 feet (V2+)" },
    { "dst_debris_pos_z",   "i16 feet (V2+)" },
    { "dmg_type",           "u32 (V2+)" },
    { "year_available",     "u32 year (V3+)" },
    { "max_vis_dist",       "i16 feet" },
    { "camera_dist",        "i16 feet" },
    { "sig_vis",            "u16 visual/optical signature" },
    { "sig_laser",          "u16 laser reflectivity" },
    { "sig_ir",             "u16 infrared signature" },
    { "sig_radar",          "u16 radar cross-section" },
    { "sig_unk",            "u16 unknown fifth signature" },
    { "hit_points",         "u16" },
    { "dmg_planes",         "u16 damage vs planes" },
    { "dmg_ships",          "u16 damage vs ships" },
    { "dmg_structures",     "u16 damage vs structures" },
    { "dmg_armor",          "u16 damage vs armor" },
    { "dmg_other",          "u16 damage vs other" },
    { "explosion_type",     "u8 15=SmallGround 18=SmallAir 21=MedGround 26=LargeGround 27=Flak 30=LargeAir" },
    { "crater_size",        "u8 feet" },
    { "empty_weight",       "u32 lbs" },
    { "cmd_buf_size",       "u16" },
    // --- movement info ---
    { "turn_rate",          "u16" },
    { "bank_rate",          "u16" },
    { "max_climb",          "i16" },
    { "max_dive",           "i16" },
    { "max_bank",           "i16" },
    { "min_speed",          "u16 mph" },
    { "corner_speed",       "u16 mph" },
    { "max_speed",          "u16 mph" },
    { "acceleration",       "i32" },
    { "deceleration",       "i32" },
    { "min_altitude",       "i32 feet" },
    { "max_altitude",       "i32 feet" },
    { "util_proc",          "symbol" },
    // --- sound info ---
    { "loop_sound",         "ptr -> .11K filename" },
    { "second_sound",       "ptr -> .11K filename" },
    { "engine_on_sound",    "ptr -> .11K filename (V1+)" },
    { "engine_off_sound",   "ptr -> .11K filename (V1+)" },
    { "do_doppler",         "byte 0/1" },
    { "max_snd_dist",       "u16 feet" },
    { "doppler_pitch_lo",   "i16" },
    { "doppler_pitch_hi",   "i16" },
    { "doppler_speed_lo",   "i16" },
    { "doppler_speed_hi",   "i16" },
    { "view_offset_x",      "i16 feet" },
    { "view_offset_y",      "i16 feet" },
    { "view_offset_z",      "i16 feet" },
    { "hud_name",           "ptr -> .HUD filename (V2+)" },
};
const int OT_GENERAL_COUNT = (int)(sizeof(OT_GENERAL_FIELDS) / sizeof(OT_GENERAL_FIELDS[0]));

// ---------------------------------------------------------------------------
// NT (NpcType) extension -- follows OT_GENERAL when struct_type >= 3
// ---------------------------------------------------------------------------
const OtField NT_FIELDS[] = {
    { "npc_flags",          "u32 bitfield; bits 18-20/25-26 control AI state" },
    { "ct_name",            "ptr -> pilot/crew type name" },
    { "crew_chief_skill",   "byte 0-5" },
    { "wingman_skill",      "byte 0-5" },
    { "leader_skill",       "byte 0-5" },
    { "max_target_dist",    "i16 feet" },
    { "target_unk",         "i16" },
    { "ai_aggressiveness",  "byte 0=passive 5=aggressive" },
    { "hards",              "ptr -> hardpoint table" },
};
const int NT_COUNT = (int)(sizeof(NT_FIELDS) / sizeof(NT_FIELDS[0]));

// ---------------------------------------------------------------------------
// PT (PlaneType) extension -- follows NT when struct_type >= 5
// Derived from OpenFA crates/asset/pt/src/lib.rs
// ---------------------------------------------------------------------------
const OtField PT_FIELDS[] = {
    { "pt_flags",           "$1=Jet $2=Hook $4=TwoSeat $8=Helo $10=Eject $20=VTOL $40=Carrier $80=Bay" },
    { "env",                "ptr -> flight envelope table" },
    { "env_min_g",          "i16 minimum G envelope index" },
    { "env_max_g",          "i16 maximum G envelope index" },
    { "max_speed_sea_lvl",  "u16 mph" },
    { "max_speed_36k",      "u16 mph at 36000 ft" },
    // bv_x (pitch velocity): min, max, acc, dacc
    { "bv_x_min",           "i16" }, { "bv_x_max", "i16" },
    { "bv_x_acc",           "i16" }, { "bv_x_dacc", "i16" },
    // bv_y (roll velocity)
    { "bv_y_min",           "i16" }, { "bv_y_max", "i16" },
    { "bv_y_acc",           "i16" }, { "bv_y_dacc", "i16" },
    // bv_z (yaw velocity)
    { "bv_z_min",           "i16" }, { "bv_z_max", "i16" },
    { "bv_z_acc",           "i16" }, { "bv_z_dacc", "i16" },
    // brv_x (roll rate)
    { "brv_x_min",          "i16" }, { "brv_x_max", "i16" },
    { "brv_x_acc",          "i16" }, { "brv_x_dacc", "i16" },
    // brv_y (pitch rate)
    { "brv_y_min",          "i16" }, { "brv_y_max", "i16" },
    { "brv_y_acc",          "i16" }, { "brv_y_dacc", "i16" },
    // brv_z (rudder/yaw rate)
    { "brv_z_min",          "i16" }, { "brv_z_max", "i16" },
    { "brv_z_acc",          "i16" }, { "brv_z_dacc", "i16" },
    { "gpull_aoa",          "i16 deg" },
    { "low_aoa_speed",      "i16 mph" },
    { "low_aoa_pitch",      "i16" },
    { "turbulence_pct",     "i16 (V1+)" },
    { "stall_warn_delay",   "i16" },
    { "stall_delay",        "i16" },
    { "stall_severity",     "i16" },
    { "stall_pitch_down",   "i16" },
    { "spin_entry",         "i16" }, { "spin_exit",      "i16" },
    { "spin_yaw_lo",        "i16" }, { "spin_yaw_hi",    "i16" },
    { "spin_aoa_lo",        "i16" }, { "spin_aoa_hi",    "i16" },
    { "spin_bank_lo",       "i16" }, { "spin_bank_hi",   "i16" },
    // rudder
    { "rudder_yaw_min",     "i16" }, { "rudder_yaw_max", "i16" },
    { "rudder_yaw_acc",     "i16" }, { "rudder_yaw_dacc","i16" },
    { "rudder_slip",        "i16" }, { "rudder_drag",    "i16" },
    { "rudder_bank",        "i16" },
    { "coef_drag",          "i16" },
    { "air_brakes_drag",    "i16" },
    { "wheel_brakes_drag",  "i16" },
    { "flaps_drag",         "i16" }, { "flaps_lift",     "i16" },
    { "gear_drag",          "i16" },
    { "bay_drag",           "i16" },
    { "loaded_drag",        "i16" }, { "loaded_gpull_drag","i16" },
    { "loaded_elevator",    "i16" }, { "loaded_aileron",  "i16" },
    { "loaded_rudder",      "i16" },
    { "struct_warn_limit",  "i16 G" },
    { "struct_limit",       "i16 G" },
    // system_damage[32]
    { "sys_dmg_0",  "u8" }, { "sys_dmg_1",  "u8" }, { "sys_dmg_2",  "u8" }, { "sys_dmg_3",  "u8" },
    { "sys_dmg_4",  "u8" }, { "sys_dmg_5",  "u8" }, { "sys_dmg_6",  "u8" }, { "sys_dmg_7",  "u8" },
    { "sys_dmg_8",  "u8" }, { "sys_dmg_9",  "u8" }, { "sys_dmg_10", "u8" }, { "sys_dmg_11", "u8" },
    { "sys_dmg_12", "u8" }, { "sys_dmg_13", "u8" }, { "sys_dmg_14", "u8" }, { "sys_dmg_15", "u8" },
    { "sys_dmg_16", "u8" }, { "sys_dmg_17", "u8" }, { "sys_dmg_18", "u8" }, { "sys_dmg_19", "u8" },
    { "sys_dmg_20", "u8" }, { "sys_dmg_21", "u8" }, { "sys_dmg_22", "u8" }, { "sys_dmg_23", "u8" },
    { "sys_dmg_24", "u8" }, { "sys_dmg_25", "u8" }, { "sys_dmg_26", "u8" }, { "sys_dmg_27", "u8" },
    { "sys_dmg_28", "u8" }, { "sys_dmg_29", "u8" }, { "sys_dmg_30", "u8" }, { "sys_dmg_31", "u8" },
    { "num_engines",        "u8" },
    { "neg_g_limit",        "i16" },
    { "thrust",             "u32 lbf" },
    { "aft_thrust",         "u32 lbf afterburner" },
    { "throttle_acc",       "i16" }, { "throttle_dacc",  "i16" },
    { "fuel_consumption",   "i16 lbs/sec" },
    { "aft_fuel_consumption","i16 lbs/sec" },
    { "internal_fuel",      "u32 lbs" },
    { "gear_pitch",         "i16" },
    { "crash_speed_fwd",    "i16 mph" },
    { "crash_speed_side",   "i16 mph" },
    { "crash_speed_vert",   "i16 mph" },
    { "crash_pitch",        "i16" }, { "crash_roll", "i16" },
    { "misc_per_flight",    "i16" },
    { "repair_multiplier",  "i16" },
    { "max_takeoff_weight", "u32 lbs" },
};
const int PT_COUNT = (int)(sizeof(PT_FIELDS) / sizeof(PT_FIELDS[0]));

// ---------------------------------------------------------------------------
// JT (ProjectileType) extension -- follows OT_GENERAL when struct_type == 7
// ---------------------------------------------------------------------------
const OtField JT_FIELDS[] = {
    { "jt_flags",           "u32 warhead/capability flags" },
    { "warhead_count",      "u16 number of warheads" },
    { "seeker_class",       "u8 seeker category (same as SEE struct_type)" },
    { "si_names",           "ptr -> seeker display names" },
    { "seeker_flags",       "u16 capability/identifier flags" },
    { "seeker_subtype",     "u8 seeker sub-type byte" },
    { "seeker_mode",        "u8 0=unguided 1=radar 2=IR 3=laser" },
    { "target_param_1",     "u8" },
    { "target_param_2",     "u8" },
    { "target_param_3",     "u8" },
    { "target_param_4",     "u8" },
    { "target_param_5",     "u8" },
    { "target_param_6",     "u8" },
    // Seeker lobe 1 (primary / search)
    { "lobe1_az",           "u16 azimuth half-angle (182 units/deg); $7FFF=omnidirectional" },
    { "lobe1_el",           "u16 elevation half-angle" },
    { "lobe1_min_range",    "i32 feet (^prefix = negative two's complement)" },
    { "lobe1_max_range",    "i32 feet" },
    { "lobe1_min_heading",  "i32 $80000000=no limit" },
    { "lobe1_max_heading",  "i32 $7fffffff=no limit" },
    // Seeker lobe 2 (secondary / track)
    { "lobe2_az",           "u16 azimuth half-angle" },
    { "lobe2_el",           "u16 elevation half-angle" },
    { "lobe2_min_range",    "i32 feet" },
    { "lobe2_max_range",    "i32 feet" },
    { "lobe2_min_heading",  "i32 $80000000=no limit" },
    { "lobe2_max_heading",  "i32 $7fffffff=no limit" },
    // (remaining warhead / guidance / fire parameters are format-version dependent)
};
const int JT_COUNT = (int)(sizeof(JT_FIELDS) / sizeof(JT_FIELDS[0]));

// ---------------------------------------------------------------------------
// SEE (Seeker/Sensor) -- standalone BRF, struct_type=10
// Derived from SEE.md and F15R.SEE example.
// ---------------------------------------------------------------------------
const OtField SEE_FIELDS[] = {
    { "struct_type",        "u8 always 10 (seeker)" },
    { "names",              "ptr -> short, long, filename strings" },
    { "capability_flags",   "u16 seeker identifier / capability flags" },
    { "seeker_subtype",     "u8 seeker sub-type" },
    { "seeker_type",        "u8 0=visual 1=laser 2=IR 3=radar" },
    { "dual_mode",          "u8 $1=search/track lobe split enabled" },
    { "track_rate",         "u8 acquisition time / track rate" },
    { "reserved_1",         "u8" },
    { "reserved_2",         "u8" },
    { "reserved_3",         "u8" },
    { "reserved_4",         "u8" },
    // Primary lobe (search / initial acquisition)
    { "lobe1_az",           "u16 azimuth half-angle (182 units/deg); $7FFF=omnidirectional" },
    { "lobe1_el",           "u16 elevation half-angle" },
    { "lobe1_min_range",    "i32 feet (^prefix = negative two's complement)" },
    { "lobe1_max_range",    "i32 feet; divide by 6076 for nautical miles" },
    { "lobe1_min_heading",  "i32 $80000000=no limit (any heading passes)" },
    { "lobe1_max_heading",  "i32 $7fffffff=no limit" },
    // Secondary lobe (track / lock-on)
    { "lobe2_az",           "u16 azimuth half-angle; narrower than lobe1 for track mode" },
    { "lobe2_el",           "u16 elevation half-angle" },
    { "lobe2_min_range",    "i32 feet" },
    { "lobe2_max_range",    "i32 feet" },
    { "lobe2_min_heading",  "i32 $80000000=no limit" },
    { "lobe2_max_heading",  "i32 $7fffffff=no limit" },
    { "lobe1_prob_detect",  "u8 probability of detection % (100=always)" },
    { "lobe2_prob_detect",  "u8 probability of detection %" },
};
const int SEE_COUNT = (int)(sizeof(SEE_FIELDS) / sizeof(SEE_FIELDS[0]));

// ---------------------------------------------------------------------------
// ECM (Electronic Countermeasures) -- standalone BRF, struct_type=9
// Derived from ECM.md and F15.ECM example.
// ---------------------------------------------------------------------------
const OtField ECM_FIELDS[] = {
    { "struct_type",        "u8 always 9 (ECM)" },
    { "names",              "ptr -> short, long, filename strings" },
    { "quantity",           "u16 0=built-in suite; N=pod carrying capacity" },
    { "category",           "u8 0=built-in 1=external pod" },
    { "ecm_power",          "u16 bitmask: $10=radar jammer $100=IR jammer; $0=passive only" },
    { "chaff_eff",          "u8 chaff effectiveness (0=no chaff dispensers)" },
    { "radar_scale",        "u8 radar jammer effect scale (passed to MakeObjRotationMatrix)" },
    { "radar_az",           "u8 radar jammer azimuth half-angle (valueÃ—256 = fixed-point deg)" },
    { "radar_el",           "u8 radar jammer elevation half-angle (valueÃ—256)" },
    { "flare_eff",          "u8 flare effectiveness (0=no flare dispensers)" },
    { "ir_scale",           "u8 IR jammer effect scale" },
    { "ir_az",              "u8 IR jammer azimuth half-angle (valueÃ—256)" },
    { "ir_el",              "u8 IR jammer elevation half-angle (valueÃ—256)" },
    { "radar_pk_red",       "u8 radar Pk reduction % (Pk_final = (100-byte)Ã—Pk_base/100)" },
    { "radar_strength",     "u16 overall radar ECM strength (100=full 0=none)" },
    { "unk_ecm_1",          "u8" },
    { "unk_ecm_2",          "u8" },
    { "ir_pk_red",          "u8 IR Pk reduction % applied against IR-guided weapons" },
    { "ir_strength",        "u16 overall IR ECM strength" },
    { "unk_ecm_3",          "u8" },
};
const int ECM_COUNT = (int)(sizeof(ECM_FIELDS) / sizeof(ECM_FIELDS[0]));

// ---------------------------------------------------------------------------
// GAS (External Fuel Tank) -- standalone BRF, struct_type=8
// Derived from GAS.md; only 4 files exist (F150/F250/F350/F500.GAS).
// ---------------------------------------------------------------------------
const OtField GAS_FIELDS[] = {
    { "struct_type",        "u8 always 8 (fuel tank)" },
    { "names",              "ptr -> short, long, filename strings" },
    { "empty_weight",       "u16 lbs â€” tank structural weight when empty" },
    { "flags",              "u8 always $1 (fuel-tank category flag)" },
    { "fuel_weight",        "u32 lbs â€” usable fuel weight when full (JP-8: 6.6 lb/US gal)" },
};
const int GAS_COUNT = (int)(sizeof(GAS_FIELDS) / sizeof(GAS_FIELDS[0]));

// ---------------------------------------------------------------------------
// Info printer
// ---------------------------------------------------------------------------

static void print_field(int idx, const BrfField& f, const OtField* schema, int schema_count,
                         const BrfDoc& doc) {
    const char* name = (schema && idx < schema_count && schema[idx].name[0])
                       ? schema[idx].name : "";
    const char* note = (schema && idx < schema_count) ? schema[idx].note : "";

    if (f.type == "ptr") {
        // Resolve the pointer to its string table
        const BrfTable* tbl = doc.find_table(f.value);
        if (tbl && !tbl->strings.empty()) {
            if (name[0]) printf("  [%3d] %-24s = %s -> \"%s\"",
                                idx, name, f.value.c_str(), tbl->strings[0].c_str());
            else         printf("  [%3d] %-24s   %s -> \"%s\"",
                                idx, "", f.value.c_str(), tbl->strings[0].c_str());
        } else {
            if (name[0]) printf("  [%3d] %-24s = ptr %s", idx, name, f.value.c_str());
            else         printf("  [%3d] %-24s   ptr %s", idx, "", f.value.c_str());
        }
    } else if (f.type == "symbol") {
        if (name[0]) printf("  [%3d] %-24s = %s", idx, name, f.value.c_str());
        else         printf("  [%3d] %-24s   %s", idx, "", f.value.c_str());
    } else {
        // Numeric: decode ^X and $HEX for display
        int64_t ival = brf_parse_int(f.value);
        if (name[0]) printf("  [%3d] %-24s = %-12s (%lld)", idx, name, f.value.c_str(), (long long)ival);
        else         printf("  [%3d] %-24s   %-12s (%lld)", idx, "", f.value.c_str(), (long long)ival);
    }

    if (note && note[0]) printf("  ; %s", note);
    printf("\n");
}

void brf_print_info(const BrfDoc& doc, const char* format) {
    bool is_pt  = strcmp(format, "pt") == 0;
    bool is_nt  = strcmp(format, "nt") == 0;
    bool is_jt  = strcmp(format, "jt") == 0;

    // Determine struct_type from first field
    int struct_type = 0;
    if (!doc.fields.empty() && doc.fields[0].type == "byte")
        struct_type = (int)brf_parse_int(doc.fields[0].value);

    printf("--- OT/General Section (struct_type=%d) ---\n", struct_type);
    int fi = 0; // field index into doc.fields[]
    int si = 0; // schema index into OT_GENERAL_FIELDS[]

    for (; fi < (int)doc.fields.size() && si < OT_GENERAL_COUNT; ++fi, ++si) {
        print_field(fi, doc.fields[fi], OT_GENERAL_FIELDS, OT_GENERAL_COUNT, doc);
    }

    if (struct_type == 7 || is_jt) {
        printf("\n--- JT/Projectile Extension (struct_type=7) ---\n");
        for (int ji = 0; fi < (int)doc.fields.size(); ++fi, ++ji) {
            print_field(fi, doc.fields[fi], JT_FIELDS, JT_COUNT, doc);
        }
    } else if (struct_type >= 3 || is_nt || is_pt) {
        printf("\n--- NT/Npc Extension (struct_type>=3) ---\n");
        for (int ni = 0; fi < (int)doc.fields.size() && ni < NT_COUNT; ++fi, ++ni) {
            print_field(fi, doc.fields[fi], NT_FIELDS, NT_COUNT, doc);
        }
        if (struct_type >= 5 || is_pt) {
            printf("\n--- PT/Plane Extension (struct_type>=5) ---\n");
            for (int pi = 0; fi < (int)doc.fields.size(); ++fi, ++pi) {
                print_field(fi, doc.fields[fi], PT_FIELDS, PT_COUNT, doc);
            }
        }
    } else {
        // Print any remaining unlabeled fields
        for (; fi < (int)doc.fields.size(); ++fi) {
            print_field(fi, doc.fields[fi], nullptr, 0, doc);
        }
    }

    if (!doc.tables.empty()) {
        printf("\n--- Pointer Tables ---\n");
        for (auto& t : doc.tables) {
            printf("  :%s\n", t.name.c_str());
            for (auto& s : t.strings)
                printf("    \"%s\"\n", s.c_str());
        }
    }
}

} // namespace fx
