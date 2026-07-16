/*
 * fa_types.h — recovered FA.EXE datatypes for the reconstruction program (#230).
 *
 * Parsed into the Ghidra project by scripts/ghidra/ApplyTypes.java and consumed by
 * the eventual `fc` clean-room C++ codegen. This header is CONSERVATIVE by design:
 *
 *   - The struct field maps in docs/fa/structs.md are ACCESS-PATTERN annotations
 *     (RecoverStructs.java records every `[reg+const]` dereference), not byte-exact
 *     layouts: offsets overlap (a dword read and a byte read of the same word both
 *     appear), sizes are inferred from the next observed offset, and some inferred
 *     tails do not even close to the known total size. A WRONG datatype is worse
 *     than none, so only fields we are confident are correct are named here; every
 *     unmapped or contradictory region stays explicit `reserved` padding.
 *   - Struct *names* that appear in FA.SMS mangled signatures (NET_ADDRESS, SEQGR,
 *     entity, …) are declared as the type VOCABULARY even when their interiors are
 *     not yet mapped, so pointers can be typed correctly today and interiors filled
 *     in per-subsystem as they are recovered on the bench.
 *
 * Everything is #pragma pack(1): these mirror on-disk / in-memory binary layouts.
 * See db/types/README.md.
 */
#pragma pack(push, 1)

/* --- scalar aliases (match the engine's fixed-width use) --------------------- */
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef signed char    s8;
typedef short          s16;
typedef int            s32;
typedef int            fixed24; /* F24.8 fixed-point (e.g. altitude_f24) */
/* 64-bit, returned in EDX:EAX. The engine itself is 32-bit throughout; this exists because a
 * few CRT routines genuinely return a 64-bit value that way (_atoi64 accumulates it with
 * `add eax / adc edx`). Typing those as `int` would silently drop the high half. */
typedef long long      longlong;

/* --- type vocabulary: named in FA.SMS signatures, interiors recovered later --- */
/* Declared opaque so pointers type-check now; do not assert unknown fields.      */
typedef struct entity      entity;      /* game-object record (defined below)       */
typedef struct OBJ_TYPE    OBJ_TYPE;    /* object TYPE record (defined below)       */
/* The *_TYPE structs below are the per-class EXTENSIONS of the OBJ_TYPE record --
 * not overlays of an entity allocation (see the object model note below, #454).
 * Their interiors alias one another in the extension region, so they stay opaque. */
typedef struct PROJ_TYPE   PROJ_TYPE;   /* projectile/missile type extension        */
typedef struct PT_TYPE     PT_TYPE;     /* aircraft performance type, .PT           */
typedef struct GV_TYPE     GV_TYPE;     /* ground-vehicle type extension            */
typedef struct OT_TYPE     OT_TYPE;     /* ordnance type, .OT                       */
typedef struct NT_TYPE     NT_TYPE;     /* nav target, .NT                          */
typedef struct NET_ADDRESS NET_ADDRESS; /* transport address (network.md)          */
typedef struct NET_PROTOCOL NET_PROTOCOL;
typedef struct RES_LIST    RES_LIST;    /* resource-manager list (memory-resource)  */
typedef struct T_HANDLE    T_HANDLE;    /* type/asset handle (terrain, objects)     */
typedef struct SEQUENCE    SEQUENCE;    /* .SEQ player slot, stride 0x38 (seq.md)   */
typedef struct PILOT       PILOT;       /* pilot save record, 0x25E0 (formats/P.md) */
typedef struct OBJ         OBJ;         /* generic game object alias of entity (objects.md) */
typedef struct AIRPORT     AIRPORT;     /* airport/carrier record, 0x134 (airports.md)*/
typedef struct OBJ_ON_CARRIER OBJ_ON_CARRIER; /* carrier-deck occupancy slot (airports.md) */
typedef struct SEQGR       SEQGR;       /* .SEQ graphic display-list node (seq.md)  */
typedef struct SEQLBL      SEQLBL;      /* .SEQ label node                          */
typedef struct SEQFNT      SEQFNT;      /* .SEQ font node                           */
typedef struct SEQTXT      SEQTXT;      /* .SEQ text node                           */

/* --- vocabulary harvested from the C++-mangled FA.SMS names (#452) ---------------
 * The MSVC C++ decoration spells out its parameter types, so every struct and enum
 * named in a mangled signature is a recovered fact -- the original developers' own type
 * name. These are declared opaque for the same reason as the block above: pointers
 * type-check today, and interiors get filled in per-subsystem as they are recovered.
 */
typedef struct F24_POINT3   F24_POINT3;   /* collision, sound: F24.8 3-vector        */
typedef struct F24_POINT    F24_POINT;    /* flight-model: F24.8 2-vector (IntersectT) */
typedef struct MovieContext MovieContext; /* video: FMV playback context (DecodeFrame) */
typedef struct WORD_POINT3  WORD_POINT3;  /* terrain: 16-bit 3-vector                */
typedef struct LONG_POINT   LONG_POINT;   /* terrain: 32-bit 2-vector                */
typedef struct PLANE        PLANE;        /* sound: plane equation                   */
typedef struct BOX          BOX;          /* shell-ui: rectangle                     */
typedef struct FVERTEX      FVERTEX;      /* renderer: flat-shaded vertex            */
typedef struct TVERTEX      TVERTEX;      /* renderer: textured vertex               */
typedef struct T_BITMAP     T_BITMAP;     /* renderer: bitmap / texture surface      */
typedef struct GlobalData   GlobalData;   /* renderer, video: codec global state     */
typedef struct FrameHeader  FrameHeader;  /* video: FMV frame header                 */
typedef struct HIT_OBJ_DATA HIT_OBJ_DATA; /* objects, weapons: hit record            */
typedef struct LEAF_LIST    LEAF_LIST;    /* terrain: quadtree leaf list             */
typedef struct MODSPEC      MODSPEC;      /* sound: module / track spec              */
typedef struct ACTION       ACTION;       /* shell-ui: menu action                   */
typedef struct VDO          VDO;          /* video: VDO player context               */
typedef struct VDOHEADER    VDOHEADER;    /* video: VDO stream header (VDOfromVDOHEADER) */
typedef struct CONFIG       CONFIG;       /* startup: EA.CFG settings record (UCONFIG_*) */
typedef struct ANGLE          ANGLE;          /* objects: attitude vector (MPPrepareForInterp) */
typedef struct HARDPOINT      HARDPOINT;      /* flight-model: store station          */
typedef struct HARDPOINT_TYPE HARDPOINT_TYPE; /* flight-model: store-station type     */
typedef struct DIAL           DIAL;           /* shell-ui: dial/knob widget           */
typedef struct ModemStrings   ModemStrings;   /* input: modem init/dial string set    */
typedef struct WAYPOINT       WAYPOINT;       /* campaign: nav waypoint (MPSetWaypoints) */
typedef struct T_MSG          T_MSG;          /* hud: text-message record (MPMsgSend)   */

/* network -- the transport types the NET_/SERIAL_ signatures name */
typedef struct NET_PKT               NET_PKT;
typedef struct CN_INFO_TCP           CN_INFO_TCP;   /* TCP-transport slice of CN_INFO (NetSetFactoryTCP) */
typedef struct NET_ADDRESS_LIST      NET_ADDRESS_LIST;
typedef struct NET_PLAYER_LIST       NET_PLAYER_LIST;
typedef struct SERIAL_PACKET         SERIAL_PACKET;
typedef struct SERIAL_PACKET_WRAPPER SERIAL_PACKET_WRAPPER;
typedef struct SERIAL_QUEUE          SERIAL_QUEUE;
typedef struct sockaddr_ipx          sockaddr_ipx;  /* IPX transport address         */
typedef struct sockaddr_in           sockaddr_in;   /* Winsock TCP/IP address        */
typedef struct socket_state          socket_state;
typedef struct _winsock_funcs        _winsock_funcs; /* resolved WS2_32 entry-point table */
typedef struct fd_set                fd_set;         /* Winsock select() descriptor set   */
typedef struct PKT_PLAYER_AD         PKT_PLAYER_AD; /* game advertisement packet (UDP/SAP query) */
typedef struct MP_INFO               MP_INFO;       /* multiplayer session-info record (MP_Info) */

/* MSVC CRT internals, named in the startup subsystem's exception-handling signatures */
typedef struct _iobuf                _iobuf;  /* the CRT FILE struct, by its real name */
typedef struct EHExceptionRecord     EHExceptionRecord;
typedef struct EHRegistrationNode    EHRegistrationNode;
typedef struct _EXCEPTION_POINTERS   _EXCEPTION_POINTERS;
typedef struct _s_FuncInfo           _s_FuncInfo;

/* Enums (mangled `W4`): 4-byte, int-backed. The enumerators are not recovered, so the
 * width is preserved without inventing constants. */
typedef int JOYRESULT;           /* input:   GetJoystickType / ReadDevice result     */
typedef int PLAYER_ACTION;       /* network: player-action callback code             */
typedef int NET_CONNECTED_STATE; /* network: connection-state callback code          */
typedef int NET_SEND_CANCEL;     /* network: broadcast send/cancel selector          */
typedef int PLAYER_STATE;        /* network: per-player session state                */
typedef int SOCK_STATE;          /* network: socket_state connection state           */
typedef int SOCK_TYPE;           /* network: socket transport type                   */
typedef int ERROR_SEVERITY;      /* network: winsock error-severity level            */

/* `undefinedN` -- N bytes, type not recovered. Ghidra's idiom, and the one this database
 * uses wherever the evidence proves a SIZE but not a SEMANTICS:
 *
 *   - function arguments an `@N` decoration counts but does not describe (#452/#453);
 *   - globals whose access width the instructions prove (#455) -- `MOV EAX,[x]` shows four
 *     bytes are read, but a dword global is equally consistent with a counter and a
 *     pointer, and 12 of the first 32 typed globals turned out to be pointers.
 *
 * An honest unknown, never a guessed type. Sharpening one into its real type as it is
 * recovered is expected; see db/types/README.md. */
typedef unsigned char  undefined1;
typedef unsigned short undefined2;
typedef unsigned int   undefined4;

/* === The object model: two families, both self-describing (#454) =================
 *
 * The engine has two record families, and they are NOT the same struct:
 *
 *   entity   — the per-instance OBJECT record. Bump-allocated by OBJAdd, addressed by
 *              id through _objPtrs, mirrored into the fixed buffer _cg (0x50CE80) while
 *              a handler services it.
 *   OBJ_TYPE — the shared TYPE record the object's +0x05 points at (loaded from the
 *              .OT / .NT / .PT / .JT files). Mirrored into _cgt (0x50D268). The
 *              *_TYPE structs (PROJ_TYPE, PT_TYPE, GV_TYPE, OT_TYPE, NT_TYPE) are its
 *              per-class extensions -- they are TYPE data, not entity overlays.
 *
 * Both records are VARIABLE-SIZE, and each declares its own size:
 *
 *   object bytes = 0xDE + type->obj_ext_size   (dword-rounded)
 *   type   bytes = type->type_size
 *
 * confirmed twice, independently: OBJAdd's only call site (in _T_AddObj, 0x4A73B0) asks
 * for `*(short *)(type + 3) + 0xde` bytes, and GetCurObj (0x4628B0) computes _curObjSize
 * the same way before mirroring the record into _cg. So there is no sizeof(entity) for
 * the whole record: sizeof(entity) below is the COMMON region every class shares, and the
 * class extension begins immediately after it, at 0xDE.
 *
 * WHY THE CLASS EXTENSIONS ARE STILL OPAQUE: every class's extension starts at the same
 * offset, so their fields ALIAS in the address space (an aircraft's flight-model state and
 * a missile's guidance state both live at 0xDE+). They cannot be separated by offset --
 * only by attributing each access to the class of object the accessing code services. That
 * is real RE, and until it is done a named extension field would be a guess. A wrong
 * datatype is worse than none.
 *
 * EVIDENCE for the fields named below: the mirrors make the census unusually strong. Code
 * that reaches a field through the mirror uses an ABSOLUTE address (_cg+N), so the
 * instruction's operand size PROVES the field's width (the #455 rule) and its subsystem is
 * known. Note the asymmetry: presence in that census proves a field exists, but ABSENCE
 * proves nothing (code reaching the same field through a register never shows up). So this
 * layout names what the census and the prose docs corroborate, and reserves the rest.
 */

/* Common region of every object record. The per-class extension begins at 0xDE. */
typedef struct entity {
    u8      obj_class;        /* 0x00  class tag (& 0x1f); 4 = aircraft   confirmed */
    u32     obj_flags;        /* 0x01  & 1 alive, & 0x100000 draw destroyed        */
    u32     obj_type;         /* 0x05  -> OBJ_TYPE (the shared type record)        */
    u8      unk_09;           /* 0x09  1-byte access, 7 subsystems                 */
    u32     unk_0A;           /* 0x0A                                              */
    u16     obj_health;       /* 0x0E  0 = destroyed                     confirmed */
    u8      unk_10;           /* 0x10                                              */
    fixed24 pos_x;            /* 0x11  world position, F24.8            confirmed */
    fixed24 pos_y;            /* 0x15                                              */
    fixed24 pos_z;            /* 0x19                                              */
    u16     heading;          /* 0x1D  (some code reads 0x1D as a dword, i.e.      */
    u16     pitch;            /* 0x1F   heading+pitch together)          confirmed */
    u16     bank;             /* 0x21  heading/pitch/bank is the orientation triple;
                               *       physics.md names this one from the flight
                               *       model. An earlier revision guessed
                               *       "goal_angle" from an access note.  confirmed */
    u16     goal_heading;     /* 0x23                                              */
    u16     goal_altitude;    /* 0x25                                              */
    u8      reserved_027[13]; /* 0x27                                              */
    fixed24 speed;            /* 0x34  F24.8                                       */
    u8      view_target;      /* 0x38  view-target mode + id                       */
    u8      reserved_039[23]; /* 0x39                                              */
    u8      unk_50;           /* 0x50                                              */
    u8      unk_51;           /* 0x51                                              */
    u16     unk_52;           /* 0x52                                              */
    u16     unk_54;           /* 0x54                                              */
    u8      reserved_056[2];  /* 0x56                                              */
    u16     unk_58;           /* 0x58                                              */
    u16     unk_5A;           /* 0x5A                                              */
    u16     unk_5C;           /* 0x5C                                              */
    u32     unk_5E;           /* 0x5E                                              */
    u16     unk_62;           /* 0x62                                              */
    u16     chain_next_id;    /* 0x64  next object in the service chain  confirmed */
    u8      reserved_066[2];  /* 0x66                                              */
    u16     service_key;      /* 0x68  sort key ordering the service chain         */
    u16     unk_6A;           /* 0x6A                                              */
    u32     event_override;   /* 0x6C  optional per-instance proc override         */
    u8      reserved_070[4];  /* 0x70                                              */
    u16     unk_74;           /* 0x74                                              */
    u16     unk_76;           /* 0x76                                              */
    u8      reserved_078[8];  /* 0x78                                              */
    u8      net_state[0x5E];  /* 0x80  per-object network replication block: every  */
                              /*       access in this range is from the network      */
                              /*       subsystem. Interior not mapped.               */
} entity;                     /* 0xDE  == FA_OBJ_COMMON_SIZE                        */

/* The shared type record. Its TOTAL size is type_size (variable, per class); the struct
 * below is the COMMON HEADER -- the fields generic object code reads on any class. */
typedef struct OBJ_TYPE {
    u8      unk_00;           /* 0x00                                              */
    u16     type_size;        /* 0x01  total bytes of THIS type record   confirmed */
    u16     obj_ext_size;     /* 0x03  bytes of class extension on the object      */
                              /*       record: object = 0xDE + this      confirmed */
    u32     unk_05;           /* 0x05                                              */
    u32     type_flags;       /* 0x09  & 0x400 = auto-remove on death    confirmed */
    u16     obj_class;        /* 0x0D  class bitfield; its high byte & 0xC0 gates   */
                              /*       the _b shape slot                 confirmed */
    u32     shape;            /* 0x0F  base shape                        confirmed */
    u32     shadow_shape;     /* 0x13  the SHADOW shape. The retail data names this field
                               *       itself -- every .PT points it at `shadowShape`,
                               *       resolving to `<name>_s.SH` (a10_s, f16_s, kin_s...).
                               *       An earlier revision called it `shape_name` ("the
                               *       suffix template") and marked it confirmed; the game's
                               *       own files say otherwise.             confirmed */
    u32     shape_a;          /* 0x17  destroyed set {A,B} — world pass  confirmed */
    u32     shape_b;          /* 0x1B  destroyed set {A,B} — graphics    confirmed */
    u8      reserved_01F[6];  /* 0x1F                                              */
    u32     shape_c;          /* 0x25  destroyed set {C,D}, aircraft     confirmed */
    u32     shape_d;          /* 0x29  destroyed set {C,D}, aircraft     confirmed */
    u8      reserved_02D[6];  /* 0x2D                                              */
    u32     damage_set;       /* 0x33  == 2 selects the {_C,_D} set      confirmed */
    u8      reserved_037[70]; /* 0x37                                              */
    u32     class_proc;       /* 0x7D  the class's proc selector (GetObjProc)       */
    u8      reserved_081[37]; /* 0x81                                              */
} OBJ_TYPE;                   /* 0xA6 (166) -- the RETAIL DATA states this size    */

/* === The class extensions (#476) =================================================
 *
 * An object record is `entity` (0xDE) + a per-class extension. The extension's LENGTH is
 * declared per type at OBJ_TYPE::obj_ext_size -- and the retail type files state it
 * outright. Read from all 534 records shipped in the game's LIB archives:
 *
 *   .OT  (static objects, _OBJProc/_STRIPProc)   ext =   0  -- ALL 170 records
 *   .JT  (projectiles,    _PROJProc)             ext =  52  -- ALL 135 records
 *   .NT  (ground/NPC,     _GVProc et al)         ext = 145..247
 *   .PT  (aircraft,       _PLANEProc)            ext = 490..626
 *
 * (The type files are `[brent's_relocatable_format]` text, and they delimit the record
 *  with the developers' own section names: OBJ_TYPE / NPC_TYPE / PLANE_TYPE / PROJ_TYPE.
 *  So OBJ_TYPE is not our coinage after all -- it is the game's own name.)
 *
 * WHY THE LOW BAND CANNOT BE ATTRIBUTED BY ADDRESS. Every class's extension starts at the
 * same offset, so the first 52 bytes are simultaneously a projectile's whole extension, an
 * aircraft's first 52, and a ground vehicle's first 52. An access to `_cg + 0xDE` names no
 * class at all.
 *
 * WHAT DOES ATTRIBUTE A FIELD:
 *   1. SIZE. A ground vehicle's extension is at most 247 bytes, a projectile's is 52 -- so
 *      any field at ext >= 247 can ONLY be an aircraft's. That is proven by the shipped data.
 *   2. A CLASS GUARD IN THE CODE. `ShapeSetup` reads these fields only after testing
 *      `entity.obj_class == 4` (aircraft), which proves the class of what it then reads.
 *
 * And what does NOT: call-graph reachability from the class procs. It looks sound and is
 * not -- a handler routinely mirrors ANOTHER object into `_cg` (GetCurObj on a target id),
 * so "reached from _PROJProc" does not mean "a projectile's field". Measured: it attributes
 * fields at ext 0x99 and 0x9D to projectiles, whose extension is only 52 bytes long.
 */

/* The aircraft extension, at entity + 0xDE. Every aircraft type ships at least 490 bytes of
 * it (the tail beyond that varies per type), so this struct models the guaranteed prefix. */
typedef struct PLANE_EXT {
    u8      reserved_000[5];  /* +0x000                                              */
    u8      pl_state;         /* +0x005  staged into _PLstate for the shape selectors */
    u8      reserved_006[139];/* +0x006  ALIASED with the projectile and GV extensions:
                               *         no field here can be attributed by address    */
    u32     state_flags;      /* +0x091  the aircraft's discrete state word, read by
                               *         ShapeSetup under an obj_class == 4 guard:
                               *           0x20   afterburner
                               *           0x40   gear down     (set/cleared by FMGear)
                               *           0x80   wheel brake
                               *           0x100  slats / flaps
                               *           0x400  arrestor hook
                               *           0x1000 computer flight (autopilot)
                               *         An earlier revision of this header read 0x1000 as
                               *         "gear". It is not: ShapeSetup uses it to set
                               *         _computerFlight. Gear is 0x40, and the gear
                               *         ARTICULATION is not a bit at all -- see gear_pos.
                               *                                            confirmed */
    u8      reserved_095[40]; /* +0x095                                              */
    fixed24 g_load;           /* +0x0BD  0x100 = 1 G (physics.md)          inferred */
    u8      reserved_0C1[79]; /* +0x0C1                                              */
    fixed24 throttle;         /* +0x110  current, smoothed                 confirmed */
    u16     throttle_target;  /* +0x114  commanded %, 0..100               confirmed */
    u8      reserved_116[2];  /* +0x116                                              */
    fixed24 climb_rate;       /* +0x118  compared against -0x2D00 for the flap gate  */
    u16     unk_11C;          /* +0x11C                                              */
    fixed24 fuel;             /* +0x11E  internal quantity                 confirmed */
    u8      reserved_122[12]; /* +0x122                                              */
    u8      stall_state;      /* +0x12E  0 normal, 1 warning, 2 stall      confirmed */
    u8      reserved_12F[43]; /* +0x12F                                              */
    s16     gear_pos;         /* +0x15A  (entity +0x238) gear articulation position.
                               *         0x7FFF is the sentinel for "not articulating";
                               *         ShapeSetup stages this into _PLgearPos/_PLgearInc,
                               *         which the .SH x86 selectors read.     confirmed */
    u8      reserved_15C[58]; /* +0x15C                                              */
    u16     ab_expiry;        /* +0x196  afterburner timer (vs _currentT)  confirmed */
    u16     brake_expiry;     /* +0x198  wheel-brake timer (vs _currentT)  confirmed */
    u8      reserved_19A[80]; /* +0x19A                                              */
} PLANE_EXT;                  /* 490 (0x1EA) -- the smallest aircraft extension shipped */

/* The projectile extension, at entity + 0xDE. EXACTLY 52 bytes in all 135 shipped .JT
 * records. The interior is deliberately unmapped: it aliases the aircraft and GV
 * extensions byte for byte, and nothing in the census can tell them apart (see above). */
typedef struct PROJ_EXT {
    u8      reserved_000[52]; /* +0x000  interior not attributable by address        */
} PROJ_EXT;                   /* 52 */

/* --- CN_INFO — network configuration, EA.CFG / NET.DAT (docs/fa/structs.md §4) ---
 * CN_ReadConfig reads a 0xDDC-byte body after a 4-byte checksum. Only the confirmed,
 * 4-aligned dword fields and the adjacent ASCII address strings are named; the large
 * unmapped protocol body and the contradictory binary-address tail stay reserved.
 */
typedef struct CN_INFO {
    u32  cn_version;         /* 0x000  config version: 1 / 2 / 3 (v3 current)      */
    u32  cn_session_id;      /* 0x004  session / IP identifier                     */
    u32  cn_pkt_state;       /* 0x008  packet-processing state                     */
    u8   reserved_00C[4];    /* 0x00C                                              */
    u32  cn_master_ptr;      /* 0x010  master session handle                       */
    u32  cn_player_list;     /* 0x014  player-list pointer / state                 */
    u8   reserved_018[8];    /* 0x018                                              */
    u32  cn_hud_range;       /* 0x020                                              */
    u32  cn_hud_heading;     /* 0x024                                              */
    u32  cn_hud_speed;       /* 0x028                                              */
    u8   reserved_02C[4];    /* 0x02C                                              */
    u32  cn_master_shutdown; /* 0x030                                              */
    u32  cn_hud_alt;         /* 0x034                                              */
    u32  cn_hud_weapon;      /* 0x038                                              */
    u8   reserved_03C[4];    /* 0x03C                                              */
    u32  cn_hud_draw_data;   /* 0x040                                              */
    u32  cn_hud_init_ext;    /* 0x044                                              */
    u8   reserved_048[12];   /* 0x048                                              */
    u32  cn_hud_nearest;     /* 0x054                                              */
    u8   reserved_058[12];   /* 0x058                                              */
    u32  cn_net_slave_state; /* 0x064                                              */
    u32  cn_flight_key;      /* 0x068                                              */
    u8   reserved_06C[16];   /* 0x06C                                              */
    u32  cn_flying_loop;     /* 0x07C                                              */
    u32  cn_unk_080;         /* 0x080                                              */
    u8   reserved_084[28];   /* 0x084                                              */
    u32  cn_hud_disrupt;     /* 0x0A0                                              */
    u8   reserved_0A4[28];   /* 0x0A4                                              */
    u32  cn_ifm_time;        /* 0x0C0  IFM timer                                   */
    u8   reserved_0C4[4];    /* 0x0C4                                              */
    u32  cn_damage;          /* 0x0C8  damage data for net sync                    */
    u8   reserved_0CC[12];   /* 0x0CC                                              */
    u32  cn_slave_fail;      /* 0x0D8  disconnection failure state                 */
    u8   reserved_0DC[0x8E4 - 0x0DC]; /* protocol/session body — not yet mapped    */
    char cn_mac_str[8];      /* 0x8E4  ASCII MAC address string                    */
    char cn_ip_str[12];      /* 0x8EC  ASCII IP address string                     */
    u8   reserved_8F8[0xDDC - 0x8F8]; /* binary-address tail — inferred sizes do    */
                                      /* not close; kept opaque (see header note)  */
} CN_INFO;

#pragma pack(pop)
