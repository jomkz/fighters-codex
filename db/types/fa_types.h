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

/* --- type vocabulary: named in FA.SMS signatures, interiors recovered later --- */
/* Declared opaque so pointers type-check now; do not assert unknown fields.      */
typedef struct entity      entity;      /* game-object base (docs/fa/structs.md §1) */
typedef struct PROJ_TYPE   PROJ_TYPE;   /* projectile/missile extension (§2)        */
typedef struct PT_TYPE     PT_TYPE;     /* aircraft performance type, .JT (§3)      */
typedef struct GV_TYPE     GV_TYPE;     /* ground-vehicle extension (§5)            */
typedef struct OT_TYPE     OT_TYPE;     /* ordnance type, .OT (§6)                  */
typedef struct NT_TYPE     NT_TYPE;     /* nav target, .NT (§7)                     */
typedef struct NET_ADDRESS NET_ADDRESS; /* transport address (network.md)          */
typedef struct NET_PROTOCOL NET_PROTOCOL;
typedef struct RES_LIST    RES_LIST;    /* resource-manager list (memory-resource)  */
typedef struct T_HANDLE    T_HANDLE;    /* type/asset handle (terrain, objects)     */
typedef struct SEQUENCE    SEQUENCE;    /* .SEQ player slot, stride 0x38 (seq.md)   */
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

/* network -- the transport types the NET_/SERIAL_ signatures name */
typedef struct NET_PKT               NET_PKT;
typedef struct NET_ADDRESS_LIST      NET_ADDRESS_LIST;
typedef struct NET_PLAYER_LIST       NET_PLAYER_LIST;
typedef struct SERIAL_PACKET         SERIAL_PACKET;
typedef struct SERIAL_PACKET_WRAPPER SERIAL_PACKET_WRAPPER;
typedef struct SERIAL_QUEUE          SERIAL_QUEUE;
typedef struct sockaddr_ipx          sockaddr_ipx;  /* IPX transport address         */
typedef struct socket_state          socket_state;

/* MSVC CRT internals, named in the startup subsystem's exception-handling signatures */
typedef struct EHExceptionRecord     EHExceptionRecord;
typedef struct EHRegistrationNode    EHRegistrationNode;
typedef struct _EXCEPTION_POINTERS   _EXCEPTION_POINTERS;
typedef struct _s_FuncInfo           _s_FuncInfo;

/* Enums (mangled `W4`): 4-byte, int-backed. The enumerators are not recovered, so the
 * width is preserved without inventing constants. */
typedef int JOYRESULT;           /* input:   GetJoystickType / ReadDevice result     */
typedef int PLAYER_ACTION;       /* network: player-action callback code             */
typedef int NET_CONNECTED_STATE; /* network: connection-state callback code          */

/* `undefined4` -- 4 bytes, type not recovered: Ghidra's idiom, used by the signatures
 * derived from an `@N` decoration, where the name proves the callee pops N bytes but
 * says nothing about what those bytes are. An honest unknown, never a guessed type. */
typedef unsigned int undefined4;

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
