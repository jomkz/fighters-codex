// PKWare DCL ("Blast") decompressor.
// Based on blast.c by Mark Adler (Copyright (C) 2003, 2012; zlib/libpng license).
// This file is an altered version -- see LICENSE for the required notice.

#include "fx/blast.h"
#include <cstring>
#include <vector>

namespace fx {

static const int MAXBITS = 13;
static const int END_SYM = 519; // lenbase[15] + 255 = 264 + 255

// Compact RLE Huffman tables from blast.c.
// Each byte: (reps-1) in high 4 bits, code-length in low 4 bits.
// Verified identical to PKWDCL.DLL data segment.
static const uint8_t LENLEN[]  = { 2, 35, 36, 53, 38, 23 };         // 16 length symbols
static const uint8_t DISTLEN[] = { 2, 20, 53, 230, 247, 151, 248 }; // 64 distance symbols

static const int LENBASE[]  = {3,2,4,5,6,7,8,9,10,12,16,24,40,72,136,264};
static const int LENEXTRA[] = {0,0,0,0,0,0,0,0, 1, 2, 3, 4,  5,  6,  7,  8};

struct Huffman {
    int count[MAXBITS + 1];
    int symbol[256];
};

static Huffman build(const uint8_t* rep, int rep_size) {
    Huffman h = {};
    int length[256] = {};
    int sym = 0;
    for (int i = 0; i < rep_size; i++) {
        int reps = (rep[i] >> 4) + 1;
        int clen = rep[i] & 15;
        for (int j = 0; j < reps; j++) length[sym++] = clen;
    }
    int n = sym;
    for (int s = 0; s < n; s++) h.count[length[s]]++;

    int offs[MAXBITS + 1] = {};
    offs[1] = 0;
    for (int len = 1; len < MAXBITS; len++)
        offs[len + 1] = offs[len] + h.count[len];

    for (int s = 0; s < n; s++)
        if (length[s] != 0)
            h.symbol[offs[length[s]]++] = s;

    return h;
}

struct BitStream {
    const uint8_t* data;
    size_t size;
    size_t pos;
    uint32_t buf;
    int cnt;

    BitStream(const uint8_t* d, size_t s, size_t start)
        : data(d), size(s), pos(start), buf(0), cnt(0) {}

    bool fill(int need) {
        while (cnt < need) {
            if (pos >= size) return false;
            buf |= (uint32_t)data[pos++] << cnt;
            cnt += 8;
        }
        return true;
    }

    int read_bits(int n) {
        if (n == 0) return 0;
        if (!fill(n)) return -1;
        int val = (int)(buf & ((1u << n) - 1));
        buf >>= n; cnt -= n;
        return val;
    }

    // CRITICAL: each raw bit is inverted (^1) before accumulating into the code.
    // This matches blast.c exactly and is required for correct decoding.
    int decode(Huffman& h) {
        int code = 0, first = 0, index = 0;
        for (int len = 1; len <= MAXBITS; len++) {
            if (!fill(1)) return -1;
            code |= (int)((buf & 1) ^ 1);
            buf >>= 1; cnt--;
            int cnt_at_len = h.count[len];
            if (code - cnt_at_len < first)
                return h.symbol[index + (code - first)];
            index += cnt_at_len;
            first = (first + cnt_at_len) << 1;
            code <<= 1;
        }
        return -1;
    }
};

int blast_decompress(const uint8_t* in, size_t in_size,
                     uint8_t* out, size_t out_capacity) {
    if (in_size < 2) return -1;
    int litmode  = in[0];
    int dictbits = in[1];
    if (litmode != 0) return -1;
    if (dictbits < 4 || dictbits > 6) return -1;

    Huffman lencode  = build(LENLEN,  sizeof(LENLEN));
    Huffman distcode = build(DISTLEN, sizeof(DISTLEN));

    BitStream bs(in, in_size, 2);

    std::vector<uint8_t> buf;
    buf.reserve(out_capacity);

    while (buf.size() < out_capacity) {
        int flag = bs.read_bits(1);
        if (flag < 0) break;

        if (flag == 0) {
            int lit = bs.read_bits(8);
            if (lit < 0) break;
            buf.push_back((uint8_t)lit);
        } else {
            int lsym = bs.decode(lencode);
            if (lsym < 0) break;
            int len = LENBASE[lsym] + bs.read_bits(LENEXTRA[lsym]);
            if (len < 0) break;
            if (len == END_SYM) break;

            int dsym = bs.decode(distcode);
            if (dsym < 0) break;
            int dist;
            if (len == 2) {
                int ex = bs.read_bits(2);
                if (ex < 0) break;
                dist = (dsym << 2) + ex + 1;
            } else {
                int ex = bs.read_bits(dictbits);
                if (ex < 0) break;
                dist = (dsym << dictbits) + ex + 1;
            }

            int from = (int)buf.size() - dist;
            for (int i = 0; i < len && buf.size() < out_capacity; i++) {
                int src = from + i;
                buf.push_back(src < 0 ? 0 : buf[(size_t)src]);
            }
        }
    }

    size_t n = buf.size();
    if (n > out_capacity) n = out_capacity;
    memcpy(out, buf.data(), n);
    return (int)n;
}

int blast_decompress_ea(const uint8_t* in, size_t in_size,
                        uint8_t* out, size_t out_capacity) {
    if (in_size < 6) return -1;
    uint32_t expected = (uint32_t)in[0] | ((uint32_t)in[1] << 8)
                      | ((uint32_t)in[2] << 16) | ((uint32_t)in[3] << 24);
    size_t cap = (expected < (uint32_t)out_capacity) ? expected : out_capacity;
    return blast_decompress(in + 4, in_size - 4, out, cap);
}

} // namespace fx
