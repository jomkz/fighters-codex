#include "fx/xmi.h"
#include <algorithm>
#include <cstring>

namespace fx {

static uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

// ---------------------------------------------------------------------------
// IFF structure walk
// ---------------------------------------------------------------------------

static void parse_sequence(const uint8_t* data, size_t size, uint32_t begin,
                           uint32_t end, XmiSequence& seq) {
    uint32_t p = begin;
    while (p + 8 <= end && p + 8 <= size) {
        XmiChunk c;
        c.tag.assign((const char*)data + p, 4);
        c.size = be32(data + p + 4);
        c.offset = p + 8;
        if ((uint64_t)c.offset + c.size > size) break;
        if (c.tag == "TIMB" && c.size >= 2) {
            // TIMB: u16 entry count (LE), then count x 2-byte (patch, bank).
            seq.timbres = (uint16_t)(data[c.offset] | (data[c.offset + 1] << 8));
        }
        seq.chunks.push_back(c);
        p = c.offset + c.size + (c.size & 1);  // IFF pads to even
    }
}

XmiFile xmi_parse(const uint8_t* data, size_t size) {
    XmiFile f{};
    if (size < 12 || memcmp(data, "FORM", 4) != 0 ||
        memcmp(data + 8, "XDIR", 4) != 0)
        return f;
    f.valid = true;

    uint32_t p = 12;
    while (p + 8 <= size) {
        std::string tag((const char*)data + p, 4);
        uint32_t csize = be32(data + p + 4);
        uint32_t body = p + 8;
        if ((uint64_t)body + csize > size) break;

        if (tag == "INFO" && csize >= 2) {
            // Sequence count, little-endian (Miles convention).
            f.seq_count = (uint16_t)(data[body] | (data[body + 1] << 8));
        } else if (tag == "CAT " && csize >= 4 &&
                   memcmp(data + body, "XMID", 4) == 0) {
            // Catalog of one FORM XMID per sequence.
            uint32_t q = body + 4;
            uint32_t cat_end = body + csize;
            while (q + 8 <= cat_end) {
                std::string t((const char*)data + q, 4);
                uint32_t sz = be32(data + q + 4);
                uint32_t sub = q + 8;
                if ((uint64_t)sub + sz > size) break;
                if (t == "FORM" && sz >= 4 &&
                    memcmp(data + sub, "XMID", 4) == 0) {
                    XmiSequence seq{};
                    parse_sequence(data, size, sub + 4, sub + sz, seq);
                    f.sequences.push_back(std::move(seq));
                }
                q = sub + sz + (sz & 1);
            }
        }
        p = body + csize + (csize & 1);
    }
    return f;
}

// ---------------------------------------------------------------------------
// XMI EVNT -> Standard MIDI File
// ---------------------------------------------------------------------------

// Standard MIDI variable-length quantity (7 bits/byte, MSB = continuation).
static uint32_t read_vlq(const uint8_t* d, size_t size, uint32_t& pos) {
    uint32_t v = 0;
    while (pos < size) {
        uint8_t c = d[pos++];
        v = (v << 7) | (c & 0x7F);
        if (!(c & 0x80)) break;
    }
    return v;
}

static void write_vlq(std::vector<uint8_t>& out, uint32_t v) {
    uint8_t buf[5];
    int n = 0;
    buf[n++] = (uint8_t)(v & 0x7F);
    while ((v >>= 7)) buf[n++] = (uint8_t)((v & 0x7F) | 0x80);
    for (int i = n - 1; i >= 0; i--) out.push_back(buf[i]);
}

struct TimedEvent {
    uint32_t             tick;   // absolute tick
    uint32_t             order;  // creation order (stable interleave)
    std::vector<uint8_t> bytes;  // status + data (no SMF delta)
};

std::vector<uint8_t> xmi_to_smf(const uint8_t* data, size_t size,
                                size_t seq_index, uint16_t ppqn, XmiDecode* out) {
    if (out) *out = XmiDecode{};
    XmiFile f = xmi_parse(data, size);
    if (seq_index >= f.sequences.size()) return {};

    const XmiChunk* evnt = nullptr;
    for (const XmiChunk& c : f.sequences[seq_index].chunks)
        if (c.tag == "EVNT") { evnt = &c; break; }
    if (!evnt) return {};

    const uint8_t* d = data + evnt->offset;
    const size_t   n = evnt->size;
    if (out) out->evnt_size = evnt->size;

    std::vector<TimedEvent> events;
    uint32_t order = 0;
    uint32_t abs = 0;
    uint32_t pos = 0;
    bool saw_end = false;

    while (pos < n) {
        // Delay: accumulate consecutive bytes < 0x80 (AIL sum encoding).
        while (pos < n && d[pos] < 0x80) abs += d[pos++];
        if (pos >= n) break;

        uint8_t status = d[pos++];
        uint8_t hi = status & 0xF0;

        if (status == 0xFF) {  // meta
            if (pos >= n) break;
            uint8_t type = d[pos++];
            uint32_t len = read_vlq(d, n, pos);
            if ((uint64_t)pos + len > n) break;
            if (type == 0x2F) { saw_end = true; break; }  // end-of-track
            std::vector<uint8_t> ev = { 0xFF, type };
            write_vlq(ev, len);
            ev.insert(ev.end(), d + pos, d + pos + len);
            pos += len;
            events.push_back({ abs, order++, std::move(ev) });
        } else if (status == 0xF0 || status == 0xF7) {  // sysex
            uint32_t len = read_vlq(d, n, pos);
            if ((uint64_t)pos + len > n) break;
            std::vector<uint8_t> ev = { status };
            write_vlq(ev, len);
            ev.insert(ev.end(), d + pos, d + pos + len);
            pos += len;
            events.push_back({ abs, order++, std::move(ev) });
        } else if (hi == 0x90) {  // note on (+ XMI duration -> note-off)
            if (pos + 2 > n) break;
            uint8_t note = d[pos++];
            uint8_t vel = d[pos++];
            uint32_t dur = read_vlq(d, n, pos);
            events.push_back({ abs, order++, { status, note, vel } });
            events.push_back({ abs + dur, order++,
                               { (uint8_t)(0x80 | (status & 0x0F)), note, 0x40 } });
        } else if (hi == 0x80 || hi == 0xA0 || hi == 0xB0 || hi == 0xE0) {
            if (pos + 2 > n) break;
            events.push_back({ abs, order++, { status, d[pos], d[pos + 1] } });
            pos += 2;
        } else if (hi == 0xC0 || hi == 0xD0) {
            if (pos + 1 > n) break;
            events.push_back({ abs, order++, { status, d[pos] } });
            pos += 1;
        } else {
            break;  // unknown status — stop rather than desync
        }
    }
    if (out) {
        out->consumed     = pos;
        out->end_of_track = saw_end;
        out->events       = events.size();
    }

    std::stable_sort(events.begin(), events.end(),
                     [](const TimedEvent& a, const TimedEvent& b) {
                         if (a.tick != b.tick) return a.tick < b.tick;
                         return a.order < b.order;
                     });

    // ---- assemble the track ----
    std::vector<uint8_t> track;
    // Default tempo 120 BPM (500000 us/quarter) at tick 0 — XMI's base rate.
    const uint8_t tempo[] = { 0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20 };
    track.insert(track.end(), tempo, tempo + sizeof(tempo));

    uint32_t prev = 0;
    for (const TimedEvent& e : events) {
        write_vlq(track, e.tick - prev);
        track.insert(track.end(), e.bytes.begin(), e.bytes.end());
        prev = e.tick;
    }
    // End of track (always last, after any scheduled note-offs).
    const uint8_t eot[] = { 0x00, 0xFF, 0x2F, 0x00 };
    track.insert(track.end(), eot, eot + sizeof(eot));

    // ---- SMF format 0 ----
    std::vector<uint8_t> smf;
    auto put_be32 = [&](std::vector<uint8_t>& v, uint32_t x) {
        v.push_back((uint8_t)(x >> 24)); v.push_back((uint8_t)(x >> 16));
        v.push_back((uint8_t)(x >> 8));  v.push_back((uint8_t)x);
    };
    auto put_be16 = [&](std::vector<uint8_t>& v, uint16_t x) {
        v.push_back((uint8_t)(x >> 8)); v.push_back((uint8_t)x);
    };
    smf.insert(smf.end(), { 'M', 'T', 'h', 'd' });
    put_be32(smf, 6);
    put_be16(smf, 0);       // format 0
    put_be16(smf, 1);       // one track
    put_be16(smf, ppqn);    // division
    smf.insert(smf.end(), { 'M', 'T', 'r', 'k' });
    put_be32(smf, (uint32_t)track.size());
    smf.insert(smf.end(), track.begin(), track.end());
    return smf;
}

} // namespace fx
