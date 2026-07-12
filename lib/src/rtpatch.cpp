#include "fx/rtpatch.h"

#include <cstring>

// RTPatch 0xB59C codec + §9 opcode applier + §10 checksum + container parser.
//
// The adaptive-Huffman model below is a faithful transliteration of the
// reverse-engineered flat-buffer model from the MIT-licensed rtptool
// (github.com/bwrsandman/rtptool, © Sandy Carter): a single byte buffer holds
// the weight / slot / symtab / groupcnt / limit tables at fixed offsets, exactly
// as the original patcher DLL laid them out. It is intentionally low-level — a
// clean rewrite could not be proven byte-identical — and is validated against
// real FA patch data by the fa_disc_install integration test. Everything outside
// this class is ordinary C++.

namespace fx {
namespace {

// ---- MSB-first bit reader -------------------------------------------------
struct BitIn {
    const uint8_t* buf;
    size_t         pos;
    size_t         end;
    int            bl = 8;   // bits left in the current byte
    bool           bad = false;

    BitIn(const uint8_t* b, size_t off, size_t e) : buf(b), pos(off), end(e) {}

    int bit() {
        if (pos >= end) { bad = true; return 0; }
        int v = (buf[pos] >> (bl - 1)) & 1;
        if (--bl == 0) { ++pos; bl = 8; }
        return v;
    }
    uint32_t bits(int n) {
        uint32_t v = 0;
        for (int i = 0; i < n; ++i) v = (v << 1) | (uint32_t)bit();
        return v;
    }
};

inline int16_t s16(uint16_t v) { return (int16_t)v; }

// ---- flat-buffer adaptive Huffman model -----------------------------------
class HuffModel {
public:
    HuffModel(int esc_bits, int num_levels, int init_period, int upd_period) {
        int alpha = 1 << esc_bits;
        off_groupcnt = 0x34;
        off_symtab   = num_levels * 2 + 0x34;
        off_slot     = num_levels * 6 + 0x38;
        off_weight   = num_levels * 6 + 0x40 + alpha * 4;
        off_limit    = off_weight + 4 + alpha * 2;
        size_t sz    = off_limit + 0xC0 + 0x100;
        m.assign(sz, 0);

        w16(0x32, init_period); w16(0x02, init_period); w16(0x00, init_period);
        w16(0x30, upd_period);  w16(0x2e, upd_period);  w16(0x2c, upd_period);
        w16(0x06, esc_bits);    w16(0x04, num_levels);
        w32(0x0c, off_groupcnt); w32(0x20, off_symtab); w32(0x1c, off_slot);
        w32(0x18, off_weight);   w32(0x14, off_limit);  w32(0x10, off_limit);

        w16(0x0a, 1); w16(0x08, 1);
        w16(off_groupcnt, 2);
        for (int i = 1; i < num_levels; ++i) w16(off_groupcnt + i * 2, 0);

        w32(off_symtab, off_slot);
        for (int i = 1; i <= num_levels; ++i) w32(off_symtab + i * 4, off_slot + 8);

        uint32_t wbase = off_weight + alpha * 2;
        w32(off_slot, wbase);
        for (int i = 1; i < alpha + 2; ++i) w32(off_slot + i * 4, wbase + 2);

        for (int j = 0; j < alpha; ++j) w16(off_weight + j * 2, 0x8000);
        w16(off_weight + alpha * 2, 0);
        w16(off_weight + alpha * 2 + 2, 0);

        w16(0x24, alpha);
        uint32_t lim = r32(0x10);
        for (int k = 0; k < 0x30; ++k) w32(lim + k * 4, 0);

        build_limits(0);
    }

    // Decode one symbol; may consume raw escape bits and register a new literal.
    // Returns the byte value; sets bi.bad on truncation.
    uint32_t decode(BitIn& bi) {
        if (bi.pos >= bi.end) { bi.bad = true; return 0; }
        int bl = bi.bl;
        uint32_t cur = bi.buf[bi.pos];
        uint32_t val = ((1u << bl) - 1) & cur;
        int idx = bl - 1;
        int tot = bl;
        uint32_t lim = r32(0x10);

        if (val < r16u(lim + idx * 8)) {
            for (;;) {
                ++bi.pos;
                if (bi.pos >= bi.end) { bi.bad = true; return 0; }
                idx += 8; tot += 8;
                uint32_t nb = bi.buf[bi.pos];
                val = (((val & 0xFF) << 8) | nb) & 0xFFFF;
                if (val >= r16u(lim + idx * 8)) break;
            }
        }

        idx -= 1;
        int cnt = (tot - 1) & 0xFF;
        int local3 = 0;
        if (cnt != 0) {
            for (;;) {
                uint32_t lim_off = lim + 2 + (uint32_t)idx * 8;
                uint32_t threshold = (lim_off < (uint32_t)m.size() - 1) ? r16u(lim_off) : 0;
                if (val < threshold) break;
                idx -= 1; cnt -= 1; val >>= 1; local3 += 1;
                if (cnt == 0) break;
            }
        }

        uint32_t symt = r32(0x20);
        uint16_t level = (uint16_t)((idx + 1) & 0xFFFF);
        uint32_t slot_arr = r32(symt + level * 4);
        uint16_t k = (uint16_t)((val - (uint16_t)r16s(lim + level * 8)) & 0xFFFF);
        uint32_t p = r32(slot_arr + k * 4);
        uint32_t off_w = r32(0x18);
        uint32_t sym = ((p - off_w) & 0xFFFFFFFFu) >> 1;

        if (local3 == 0) { local3 = 8; ++bi.pos; }
        bi.bl = local3;

        if (update_freq(sym)) { rebuild(); build_limits(0); }

        uint16_t esc = r16u(0x24);
        if (sym == esc) {
            uint32_t raw = bi.bits(r16u(0x06));
            int lvl = add_symbol(raw);
            build_limits(lvl);
            return raw;
        }
        return sym;
    }

public:
    // A corrupt bitstream can drive the rebuild rebalance loops far past what a
    // valid model ever needs (the real FA decode peaks near 4k iterations); once
    // a rebuild trips the guard the model is unusable, so decode stops. The cap
    // is ~250x the observed valid maximum — unreachable on real data.
    bool ok() const { return ok_; }

private:
    static const long REBUILD_GUARD = 100000;
    bool ok_ = true;
    std::vector<uint8_t> m;
    uint32_t off_groupcnt, off_symtab, off_slot, off_weight, off_limit;

    // Bounds-safe table access. On valid input every offset is in range (the FA
    // decode is byte-exact); a corrupt bitstream can compute a wild index, so
    // reads clamp to 0 and writes no-op rather than running off the buffer.
    uint16_t r16u(uint32_t o) const {
        if ((size_t)o + 1 >= m.size()) return 0;
        return (uint16_t)(m[o] | (m[o + 1] << 8));
    }
    int16_t  r16s(uint32_t o) const { return s16(r16u(o)); }
    void     w16(uint32_t o, int val) {
        if ((size_t)o + 1 >= m.size()) return;
        uint16_t v = (uint16_t)val; m[o] = v & 0xFF; m[o + 1] = (v >> 8) & 0xFF;
    }
    uint32_t r32(uint32_t o) const {
        if ((size_t)o + 3 >= m.size()) return 0;
        return (uint32_t)m[o] | ((uint32_t)m[o + 1] << 8) | ((uint32_t)m[o + 2] << 16) | ((uint32_t)m[o + 3] << 24);
    }
    void w32(uint32_t o, uint32_t val) {
        if ((size_t)o + 3 >= m.size()) return;
        m[o] = val & 0xFF; m[o + 1] = (val >> 8) & 0xFF; m[o + 2] = (val >> 16) & 0xFF; m[o + 3] = (val >> 24) & 0xFF;
    }

    void build_limits(int start) {
        uint32_t gc = r32(0x0c), lim = r32(0x10);
        int num = r16u(0x04);
        int s3 = (start == 0) ? 2 : r16s(lim + (start - 1) * 8) * 2;
        for (int level = start; level < num; ++level) {
            int s4 = s3 - r16s(gc + level * 2);
            w16(lim + level * 8, s4);
            s3 = s4 * 2;
            w16(lim + level * 8 + 2, s3);
            w16(lim + level * 8 + 4, s4 * 4);
            w16(lim + level * 8 + 6, s4 * 16);
        }
    }

    bool update_freq(uint32_t sym) {
        uint32_t w = r32(0x18);
        w16(w + sym * 2, r16u(w + sym * 2) + 1);
        int cnt = r16s(0x00) - 1;
        w16(0x00, cnt);
        return (cnt & 0xFFFF) == 0;
    }

    int add_symbol(uint32_t newsym) {
        uint32_t w = r32(0x18), slot = r32(0x1c), gc = r32(0x0c), st = r32(0x20);
        uint32_t iv = newsym * 2;
        w16(w + iv, 1);
        int n_slots = r16u(0x08);
        w32(slot + n_slots * 4, w + iv);
        n_slots += 1;
        w16(0x08, n_slots);
        if (n_slots != 2) {
            int n_groups = r16u(0x0a);
            int num = r16u(0x04);
            uint16_t uvar6;
            if (n_groups < num) {
                uvar6 = (uint16_t)((n_groups - 1) & 0xFFFF);
                w16(0x0a, n_groups + 1);
            } else {
                uint16_t u = (uint16_t)((n_groups - 2) & 0xFFFF);
                while (r16s(gc + u * 2) == 0) u = (uint16_t)((u - 1) & 0xFFFF);
                uvar6 = u;
            }
            w16(gc + uvar6 * 2, r16s(gc + uvar6 * 2) - 1);
            w16(gc + 2 + uvar6 * 2, r16s(gc + 2 + uvar6 * 2) + 2);
            w32(st + (uvar6 + 1) * 4, r32(st + (uvar6 + 1) * 4) - 4);
            for (int u = uvar6 + 2; u <= num; ++u) w32(st + u * 4, r32(st + u * 4) + 4);
            return uvar6;
        }
        return 0;
    }

    void rebuild() {
        uint32_t slot = r32(0x1c), symt = r32(0x20), gc = r32(0x0c);
        int n_slots = r16u(0x08);
        int upd = r16s(0x2c);
        int num = r16u(0x04);
        int maxw = 0;
        w16(0x2c, upd - 1);

        // Block A — accumulate max weight, halving on the update boundary.
        if (n_slots != 0) {
            for (int i = 0; i < n_slots; ++i) {
                uint32_t p = r32(slot + i * 4);
                int wv = r16u(p);
                if (((upd - 1) & 0xFFFF) == 0) { wv >>= 1; w16(p, wv); }
                if (maxw < wv) maxw = wv;
            }
        }

        // Block B — bit-radix partition by weight.
        if (maxw != 0) {
            uint16_t mask = 0x8000;
            while ((maxw & mask) == 0) mask = (uint16_t)((mask >> 1) | 0x8000);
            int la = 0;
            long guard = 0;
            if (n_slots != 0) {
                while (la < n_slots) {
                    if (++guard > REBUILD_GUARD) { ok_ = false; return; }
                    uint32_t pcur = r32(slot + la * 4);
                    if ((r16u(pcur) & mask) == 0) {
                        int u17 = la + 1;
                        int ins = la;
                        if (n_slots <= u17) break;
                        int u5 = ins;
                        while (u17 < n_slots) {
                            uint32_t pp = slot + (uint32_t)u17 * 4;
                            uint32_t p9 = r32(pp);
                            u5 = ins;
                            if ((r16u(p9) & mask) != 0) {
                                u5 = ins + 1;
                                uint32_t pcur_val = r32(slot + (uint32_t)ins * 4);
                                w32(slot + (uint32_t)ins * 4, p9);
                                w32(pp, pcur_val);
                            }
                            u17 += 1;
                            ins = u5;
                        }
                        if (u5 != la) la = u5 - 1;
                        uint16_t nm = (uint16_t)(mask >> 1);
                        mask = (uint16_t)(nm | 0x8000);
                        if ((nm & 1) != 0) break;
                    } else {
                        la += 1;
                    }
                }
            }
        }

        // Block C — group rebalance.
        int la = 0;
        int n_groups = r16u(0x0a);
        uint16_t ustk = (uint16_t)((n_groups - 1) & 0xFFFF);
        int moved = 0;
        long guardc = 0;
        if (n_groups != 0) {
            for (;;) {
                if (++guardc > REBUILD_GUARD) { ok_ = false; return; }
                uint32_t pgc = gc + (uint32_t)la * 2;
                uint32_t p2 = symt + (uint32_t)la * 4;
                uint16_t g = r16u(pgc);
                if (g == 0) {
                    la += 1;
                } else {
                    uint32_t p1 = r32(p2 + 4);
                    uint16_t w_first = r16u(r32(r32(p2)));
                    uint16_t w_last  = r16u(r32((p1 - 4) & 0xFFFFFFFFu));
                    uint16_t w_last2 = r16u(r32((p1 - 8) & 0xFFFFFFFFu));
                    if (g < 3 || (((num - 1) & 0xFFFF) == la) || (w_first < (uint16_t)(w_last + w_last2))) {
                        // merge branch
                        uint32_t p16 = p2 + 4;
                        bool found = false;
                        uint16_t acc = r16u(r32((p1 - 4) & 0xFFFFFFFFu));
                        int u17 = la + 2;
                        while (u17 < n_groups) {
                            uint32_t p4 = symt + (uint32_t)u17 * 4;
                            int16_t wv = r16s(r32(r32(p4)));
                            acc = (uint16_t)((acc - wv) & 0xFFFF);
                            uint16_t gcnt17 = r16u(gc + (uint32_t)u17 * 2);
                            uint16_t wn = r16u(r32((r32(p4) + 4) & 0xFFFFFFFFu));
                            if (gcnt17 > 1 && ((acc & 0x8000) != 0 || acc < wn)) { found = true; break; }
                            u17 += 1;
                        }
                        if (!found) {
                            la += 1;
                        } else {
                            w16(pgc, r16s(pgc) - 1);
                            moved += 1;
                            la += 1;
                            w32(p16, r32(p16) - 4);
                            w16(gc + (uint32_t)la * 2, r16s(gc + (uint32_t)la * 2) + 2);
                            w32(p2 + 8, r32(p2 + 8) + 4);
                            if (la < ((u17 - 1) & 0xFFFF)) {
                                int span = (u17 - 1) - la;
                                la += span;
                                for (int s = 0; s < span; ++s) { w32(p16 + 8, r32(p16 + 8) + 4); p16 += 4; }
                            }
                            w16(gc + (uint32_t)la * 2, r16s(gc + (uint32_t)la * 2) + 1);
                            w32(p16 + 4, r32(p16 + 4) + 4);
                            w16(gc + 2 + (uint32_t)la * 2, r16s(gc + 2 + (uint32_t)la * 2) - 2);
                            if (r16s(gc + (uint32_t)ustk * 2) == 0) {
                                n_groups -= 1; w16(0x0a, n_groups);
                                ustk = (uint16_t)((ustk - 1) & 0xFFFF);
                            }
                            la = 0;
                        }
                    } else {
                        // simple branch
                        moved += 1;
                        w16(pgc - 2, r16s(pgc - 2) + 1);
                        w16(pgc, r16s(pgc) - 3);
                        w16(gc + 2 + (uint32_t)la * 2, r16s(gc + 2 + (uint32_t)la * 2) + 2);
                        w32(p2, r32(p2) + 4);
                        w32(p2 + 4, r32(p2 + 4) - 8);
                        if (ustk == la) {
                            n_groups += 1; w16(0x0a, n_groups);
                            ustk = (uint16_t)((ustk + 1) & 0xFFFF);
                        }
                        la = 0;
                    }
                }
                if (la >= n_groups) break;
            }
        }

        // Tail — self-tune the update period.
        if (moved < 0x10) {
            if (moved < 8 && r16u(0x2e) != 1) {
                w16(0x02, r16u(0x02) << 1);
                w16(0x2c, r16u(0x2c) >> 1);
                w16(0x2e, r16u(0x2e) >> 1);
            }
        } else {
            w16(0x02, r16u(0x32));
            w16(0x2e, r16u(0x30));
        }
        w16(0x00, r16u(0x02));
        if (r16u(0x2c) == 0) w16(0x2c, r16u(0x2e));
    }
};

const uint16_t DIFF_MAGIC = 0xB59C;
const int WINDOW_FLAG_8KB = 8;

// ---- §5 VLI reader over a byte stream -------------------------------------
struct VliIn {
    const uint8_t* d;
    size_t         p;
    size_t         n;
    bool           bad = false;

    uint8_t byte() { if (p >= n) { bad = true; return 0; } return d[p++]; }
    int64_t vli() {
        uint8_t b = byte();
        bool sign = (b & 0x80) != 0;
        int count = 0; uint8_t mask = 0x40;
        while (mask && (b & mask)) { ++count; mask >>= 1; }
        uint64_t val;
        if (count == 0) val = b & 0x3F;
        else {
            uint64_t high = b & ((0x40u >> count) - 1);
            uint64_t tail = 0;
            for (int i = 0; i < count; ++i) tail |= (uint64_t)byte() << (8 * i);
            val = (high << (8 * count)) | tail;
        }
        return sign ? -(int64_t)val : (int64_t)val;
    }
};

inline int64_t s8(uint8_t b) { return b >= 128 ? (int)b - 256 : (int)b; }

} // namespace

// ---- 0xB59C decompressor --------------------------------------------------
std::vector<uint8_t> rtp_decompress(const uint8_t* data, size_t size,
                                    size_t block_off, size_t hint) {
    if (!data || block_off >= size) return {};
    BitIn bi(data, block_off, size);

    if (bi.bits(16) != DIFF_MAGIC) return {};
    uint32_t use_raw    = bi.bits(8);
    bi.bits(8);                       // reserved
    int init_period     = (int)bi.bits(12);
    int upd_period      = (int)bi.bits(12);
    int wflag           = (int)bi.bits(4);
    int dist_bits       = (wflag == WINDOW_FLAG_8KB) ? 7 : 6;
    if (bi.bad) return {};

    HuffModel len_tree(6, 0x0C, init_period, upd_period);
    HuffModel dist_tree(6, 0x0C, init_period, upd_period);
    HuffModel* lit_tree = (use_raw == 0)
                              ? new HuffModel(8, 0x10, init_period, upd_period) : nullptr;

    std::vector<uint8_t> out;
    out.reserve(std::min(hint, (size_t)128u << 20));   // cap the pre-alloc; the loop still honors hint
    while (out.size() < hint) {
        int flag = bi.bit();
        if (bi.bad) break;
        if (flag == 0) {
            uint32_t sym = lit_tree ? lit_tree->decode(bi) : bi.bits(8);
            if (bi.bad || (lit_tree && !lit_tree->ok())) break;
            out.push_back((uint8_t)(sym & 0xFF));
        } else {
            uint32_t dist_lo = bi.bits(dist_bits);
            uint32_t dist_hi = dist_tree.decode(bi);
            if (bi.bad || !dist_tree.ok()) break;
            uint32_t dist = (dist_hi << dist_bits) | dist_lo;
            if (dist == 0) break;      // END sentinel
            uint32_t length = len_tree.decode(bi) & 0x7F;
            if (bi.bad || !len_tree.ok()) break;
            uint32_t back = dist + 1;
            for (uint32_t i = 0; i < length && out.size() < hint; ++i) {
                size_t pos = out.size();
                out.push_back(pos >= back ? out[pos - back] : 0);
            }
        }
    }
    delete lit_tree;
    return out;
}

// ---- §9 opcode applier ----------------------------------------------------
std::vector<uint8_t> rtp_apply(const uint8_t* src, size_t src_size,
                               const uint8_t* ops, size_t ops_size,
                               size_t dst_size, int src_count) {
    VliIn r{ops, 0, ops_size};
    std::vector<uint8_t> out(dst_size, 0);
    size_t out_pos = 0;
    int64_t poke_pos = 0;
    std::vector<std::pair<size_t, size_t>> gaps;
    std::vector<std::pair<size_t, size_t>> templates;

    // A well-formed program writes exactly dst_size bytes (the write cursor ends
    // there and FLUSH fills to it), so any write past dst_size is malformed —
    // this bounds the output against an attacker-controlled operand that would
    // otherwise resize to gigabytes.
    bool overflow = false;
    auto grow = [&](size_t n) {
        if (n > dst_size) { overflow = true; return; }
        if (n > out.size()) out.resize(n, 0);
    };
    auto fail = [&]() { return std::vector<uint8_t>{}; };

    auto do_copy = [&](size_t advance, size_t src_off, size_t cnt) {
        if (advance > 0) { gaps.emplace_back(out_pos, advance); out_pos += advance; }
        grow(out_pos + cnt);
        if (overflow) return;
        size_t src_end = (src && src_off < src_size) ? std::min(src_off + cnt, src_size) : src_off;
        size_t clen = src_end > src_off ? src_end - src_off : 0;
        if (clen > 0) std::memcpy(&out[out_pos], src + src_off, clen);
        out_pos += cnt;
    };
    auto do_fill = [&](size_t seek, const uint8_t* pat, size_t plen, size_t count) {
        if (seek > 0) { gaps.emplace_back(out_pos, seek); out_pos += seek; }
        grow(out_pos + count);
        if (overflow) return;
        bool nonzero = false;
        for (size_t i = 0; i < plen; ++i) if (pat[i]) { nonzero = true; break; }
        if (plen && nonzero) for (size_t i = 0; i < count; ++i) out[out_pos + i] = pat[i % plen];
        out_pos += count;
    };
    auto poke = [&](int64_t pos, int width, int64_t delta) {
        if (pos < 0) return;
        size_t p = (size_t)pos;
        grow(p + width);
        if (overflow) return;
        int64_t cur = 0;
        for (int i = 0; i < width; ++i) cur |= (int64_t)out[p + i] << (8 * i);
        if (width == 2) cur = (int16_t)cur; else if (width == 4) cur = (int32_t)cur;
        int64_t v = cur + delta;
        for (int i = 0; i < width; ++i) out[p + i] = (uint8_t)((v >> (8 * i)) & 0xFF);
    };
    // The poke cursor is a file offset; a seek that lands outside [0, dst_size)
    // is a no-op in poke(), so accumulate with saturation rather than overflow
    // (UB) on an attacker-controlled VLI.
    auto advance_poke = [&](int64_t seek) {
        if (seek >= 0) poke_pos = (poke_pos > INT64_MAX - seek) ? INT64_MAX : poke_pos + seek;
        else           poke_pos = (poke_pos < INT64_MIN - seek) ? INT64_MIN : poke_pos + seek;
    };

    // Validated operand readers. A length or repeat count that is negative or
    // exceeds the destination is malformed — cast unchecked it would wrap the
    // size_t bound checks. `bad_op` fails the whole apply.
    bool bad_op = false;
    auto len_op = [&]() -> size_t {                 // seek / advance / count on the write cursor
        int64_t v = r.vli();
        if (v < 0 || v > (int64_t)dst_size) { bad_op = true; return 0; }
        return (size_t)v;
    };
    auto off_op = [&]() -> size_t {                 // a source offset (clamped against src in do_copy)
        int64_t v = r.vli();
        if (v < 0) { bad_op = true; return 0; }
        return (size_t)v;
    };

    for (;;) {
        uint8_t op = r.byte();
        if (r.bad) return fail();
        if (op == 0x01) break;                       // END
        else if (op == 0x02) { r.vli(); out_pos = 0; poke_pos = 0; }  // SET_SOURCE
        else if (op == 0x03 || op == 0x04) {         // COPY / COPY+gap
            size_t advance = (op == 0x04) ? len_op() : 0;
            if (src_count >= 2) r.vli();
            size_t src_off = off_op();
            size_t cnt = len_op();
            if (!bad_op) do_copy(advance, src_off, cnt);
        } else if (op == 0x05) {                      // FLUSH
            if (dst_size > out_pos) gaps.emplace_back(out_pos, dst_size - out_pos);
            for (auto& g : gaps) {
                grow(g.first + g.second);
                if (overflow) return fail();
                for (size_t i = 0; i < g.second; ++i) { out[g.first + i] = r.byte(); }
                if (r.bad) return fail();
            }
            gaps.clear();
        } else if (op == 0x06) {                      // POKE1
            int64_t seek = r.vli(); int64_t d = s8(r.byte());
            advance_poke(seek); poke(poke_pos, 1, d);
        } else if (op == 0x07 || op == 0x0E) {        // POKE1xN (const delta)
            poke_pos = 0; int64_t d = s8(r.byte()); size_t c = len_op();
            for (size_t i = 0; i < c && !r.bad; ++i) { advance_poke(r.vli()); poke(poke_pos, 1, d); }
        } else if (op == 0x08) {                      // STORE
            if (src_count >= 2) r.vli();
            size_t src_off = off_op(); size_t cnt = len_op();
            if (!bad_op) templates.emplace_back(src_off, cnt);
        } else if (op == 0x09 || op == 0x0A) {        // TCOPY / TCOPY+gap
            size_t advance = (op == 0x0A) ? len_op() : 0;
            size_t idx = off_op();
            if (bad_op || idx >= templates.size()) return fail();
            do_copy(advance, templates[idx].first, templates[idx].second);
        } else if (op == 0x0B || op == 0x0C) {        // ZFILL / ZFILL+gap
            size_t seek = (op == 0x0C) ? len_op() : 0;
            size_t count = len_op();
            if (!bad_op) do_fill(seek, nullptr, 0, count);
        } else if (op == 0x0D) {                      // POKE1xN var delta
            poke_pos = 0; size_t c = len_op();
            for (size_t i = 0; i < c && !r.bad; ++i) { advance_poke(r.vli()); poke(poke_pos, 1, s8(r.byte())); }
        } else if (op == 0x0F) {                      // POKE16xN
            poke_pos = 0;
            int64_t d = (int16_t)(r.byte() | (r.byte() << 8)); size_t c = len_op();
            for (size_t i = 0; i < c && !r.bad; ++i) { advance_poke(r.vli()); poke(poke_pos, 2, d); }
        } else if (op == 0x10) {                      // POKE32xN
            poke_pos = 0;
            uint32_t u = r.byte(); u |= (uint32_t)r.byte() << 8;
            u |= (uint32_t)r.byte() << 16; u |= (uint32_t)r.byte() << 24;
            int64_t d = (int32_t)u; size_t c = len_op();
            for (size_t i = 0; i < c && !r.bad; ++i) { advance_poke(r.vli()); poke(poke_pos, 4, d); }
        } else if (op == 0x11 || op == 0x14) {        // FILL1
            size_t seek = (op == 0x14) ? len_op() : 0;
            uint8_t pat[1] = { r.byte() }; size_t count = len_op();
            if (!bad_op) do_fill(seek, pat, 1, count);
        } else if (op == 0x12 || op == 0x15) {        // FILL2
            size_t seek = (op == 0x15) ? len_op() : 0;
            uint8_t pat[2] = { r.byte(), r.byte() }; size_t count = len_op();
            if (!bad_op) do_fill(seek, pat, 2, count);
        } else if (op == 0x13 || op == 0x16) {        // FILL4
            size_t seek = (op == 0x16) ? len_op() : 0;
            uint8_t pat[4] = { r.byte(), r.byte(), r.byte(), r.byte() }; size_t count = len_op();
            if (!bad_op) do_fill(seek, pat, 4, count);
        } else {
            return fail();                            // unknown opcode
        }
        if (r.bad || overflow || bad_op) return fail();
    }

    out.resize(dst_size);
    return out;
}

// ---- §10 rolling checksum -------------------------------------------------
uint32_t rtp_checksum(const uint8_t* data, size_t size, int bits) {
    uint32_t mask = (bits >= 32) ? 0xFFFFFFFFu : ((1u << bits) - 1);
    uint32_t w = 0;
    for (size_t i = 0; i < size; ++i) {
        w ^= data[i];
        w = ((w << 8) | (w >> (bits - 8))) & mask;
    }
    return w;
}

// ---- container parser -----------------------------------------------------
namespace {
struct ByteIn {
    const uint8_t* d;
    size_t         p;
    size_t         n;
    bool           bad = false;

    uint8_t  u8()  { if (p >= n) { bad = true; return 0; } return d[p++]; }
    uint16_t u16() { uint16_t v = u8(); v |= (uint16_t)u8() << 8; return v; }
    uint32_t u32() { uint32_t v = u16(); v |= (uint32_t)u16() << 16; return v; }
    std::string lpstr() {
        uint32_t len = u8();
        if (len == 0xFF) len = u16();
        if (len == 0) return "";
        std::string s;
        for (uint32_t i = 0; i + 1 < len; ++i) { uint8_t c = u8(); if (c) s.push_back((char)c); }
        if (len) u8();  // NUL
        return s;
    }
    int64_t vli() {
        uint8_t b = u8();
        bool sign = (b & 0x80) != 0;
        int count = 0; uint8_t mask = 0x40;
        while (mask && (b & mask)) { ++count; mask >>= 1; }
        uint64_t val;
        if (count == 0) val = b & 0x3F;
        else {
            uint64_t high = b & ((0x40u >> count) - 1), tail = 0;
            for (int i = 0; i < count; ++i) tail |= (uint64_t)u8() << (8 * i);
            val = (high << (8 * count)) | tail;
        }
        return sign ? -(int64_t)val : (int64_t)val;
    }
};

struct Entry { std::string name; uint32_t size; uint32_t w1; uint32_t w2; };
} // namespace

RtpPatch rtp_read(const uint8_t* data, size_t size) {
    RtpPatch out;
    if (!data || size < 4 || data[0] != 'K' || data[1] != '*') return out;
    ByteIn r{data, 2, size};

    out.version = r.u16();
    out.flags   = r.u16();
    uint32_t ext = (out.flags & 0x8000) ? r.u32() : 0;
    out.extra_mode = (ext & 0x10000) != 0;
    r.u16();                       // option_flags
    r.u32();                       // patch_total_size
    r.u32();                       // reserved_a
    r.u16();                       // default_attrs
    r.u16();                       // reserved_b
    uint16_t cmd_flags = r.u16();
    if (cmd_flags & 0x4) r.u32();  // combine_mode_id
    r.u32();                       // reserved_c

    uint16_t n_special = r.u16();
    for (uint16_t i = 0; i < n_special && !r.bad; ++i) {
        std::string nm = r.lpstr(), prompt = r.lpstr();
        out.specials.emplace_back(nm, prompt);
    }
    uint16_t n_dirs = r.u16();
    for (uint16_t i = 0; i < n_dirs && !r.bad; ++i) r.lpstr();
    if (r.bad) { out.records.clear(); return out; }

    auto read_entry = [&](Entry& e) {
        size_t base = r.p;
        if (base + 34 > size) { r.bad = true; return; }
        char nm[9]; std::memcpy(nm, data + base, 8); nm[8] = 0;
        e.name = std::string(nm, strnlen(nm, 8));
        e.size = (uint32_t)data[base + 16] | ((uint32_t)data[base + 17] << 8)
               | ((uint32_t)data[base + 18] << 16) | ((uint32_t)data[base + 19] << 24);
        const uint8_t* cb = data + base + 24;
        e.w1 = ((uint32_t)cb[2] | ((uint32_t)cb[3] << 8) | ((uint32_t)cb[4] << 16) | ((uint32_t)cb[5] << 24)) & 0x7FFFFFFF;
        e.w2 = ((uint32_t)cb[6] | ((uint32_t)cb[7] << 8) | ((uint32_t)cb[8] << 16) | ((uint32_t)cb[9] << 24)) & 0x3FFFFFFF;
        r.p = base + 34;
        if (out.extra_mode) { r.p += 8; std::string alt = r.lpstr(); if (!alt.empty()) e.name = alt; }
    };

    while (r.p < size - 1 && !r.bad) {
        uint16_t hdr = r.u16();
        int rtype = hdr >> 12;
        if (rtype == 1) break;                 // EOF
        if (rtype == 0 || rtype > 6) break;    // not a record boundary
        // rec_hdr bit 7 gates a disk-set VLI, which flags a file bound for a
        // prompted system directory rather than the app directory.
        bool app_dir = !(hdr & 0x0080);
        if (hdr & 0x0002) r.u16();
        if (hdr & 0x0004) r.lpstr();
        if (hdr & 0x0080) r.vli();
        if (hdr & 0x0100) r.u16();
        if ((hdr & 0x0200) && rtype != 5) { r.lpstr(); r.lpstr(); }
        r.p += 10;                             // metadata

        // A record references a handful of file versions; anything past this is
        // a malformed count that must not size an allocation.
        const int MAX_ENTRIES = 64;

        RtpRecord rec;
        rec.app_dir = app_dir;
        if (rtype == 4) {                      // MODIFY
            r.u16();                           // file_mod_flags
            int src_count = (int)r.vli();
            int dst_count = (int)r.vli();
            if (src_count < 1 || src_count > MAX_ENTRIES ||
                dst_count < 1 || dst_count > MAX_ENTRIES) break;
            r.u32();                           // reserved
            uint32_t payload_len = r.u32();
            std::vector<Entry> srcs(src_count > 0 ? src_count : 0);
            for (auto& e : srcs) read_entry(e);
            std::vector<Entry> dsts(dst_count > 0 ? dst_count : 0);
            for (auto& e : dsts) read_entry(e);
            if (r.bad || srcs.empty() || dsts.empty()) break;
            rec.mode = RtpMode::Modify;
            rec.name = dsts[0].name;
            rec.src_count = src_count;
            rec.src_size = srcs[0].size;
            rec.src_w1 = srcs[0].w1;
            rec.src_w2 = srcs[0].w2;
            rec.dst_size = dsts[0].size;
            rec.payload_len = payload_len;
            rec.block_off = r.p;
            out.records.push_back(rec);
            r.p = rec.block_off + payload_len;
        } else if (rtype == 2) {               // NEW — a full compressed file
            // FA delivers added/replaced files (msapi.dll, ealtest.exe) whole
            // rather than as a diff. The data block carries its own length, so
            // the walk stays cheap and continues past it.
            int src_count = (int)r.vli();
            if (src_count < 1 || src_count > MAX_ENTRIES) break;
            uint32_t usize = r.u32();          // decompressed size
            uint32_t csize = r.u32();          // compressed block length
            std::vector<Entry> es(src_count > 0 ? src_count : 0);
            for (auto& e : es) read_entry(e);
            if (r.bad || es.empty()) break;
            rec.mode = RtpMode::New;
            rec.name = es[0].name;
            rec.src_count = src_count;
            rec.dst_size = usize;
            rec.payload_len = csize;
            rec.block_off = r.p;
            out.records.push_back(rec);
            r.p = rec.block_off + csize;
        } else {
            break;                             // RENAME/DELETE — FA has none mid-stream
        }
    }
    return out;
}

// ---- high-level reconstruct ----------------------------------------------
std::vector<uint8_t> rtp_reconstruct(const uint8_t* data, size_t size,
                                     const RtpRecord& rec,
                                     const uint8_t* src, size_t src_size,
                                     bool verify) {
    if (rec.mode == RtpMode::New) {
        return rtp_decompress(data, size, rec.block_off, rec.dst_size + 4096);
    }
    if (verify && src) {
        if (rtp_checksum(src, src_size, 31) != rec.src_w1 ||
            rtp_checksum(src, src_size, 30) != rec.src_w2) return {};
    }
    size_t hint = (size_t)rec.dst_size + (rec.dst_size / 2) + 4096;
    std::vector<uint8_t> ops = rtp_decompress(data, size, rec.block_off, hint);
    if (ops.empty()) return {};
    return rtp_apply(src, src_size, ops.data(), ops.size(), rec.dst_size, rec.src_count);
}

} // namespace fx
