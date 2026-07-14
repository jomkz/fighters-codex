#include "fx/brf.h"
#include <cctype>
#include <cstdlib>

namespace fx {

// ---- helpers ----------------------------------------------------------

static bool is_space(char c) { return c == ' ' || c == '\t'; }

static std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// A number, as the loader's StringIsNumber sees it: the `byte`/`word`/`dword` keywords keep
// consuming operands for as long as the next token looks numeric. Only a `$` token is hex --
// accepting hex digits everywhere would let the repeat loop swallow a bare word like `end`
// as if it were a number.
static bool is_number(const std::string& t) {
    size_t i = 0;
    if (i < t.size() && (t[i] == '^' || t[i] == '-' || t[i] == '+')) ++i;
    const bool hex = (i < t.size() && t[i] == '$');
    if (hex) ++i;
    if (i >= t.size()) return false;
    for (; i < t.size(); ++i) {
        const unsigned char c = (unsigned char)t[i];
        if (!(hex ? std::isxdigit(c) : std::isdigit(c))) return false;
    }
    return true;
}

// Next whitespace-delimited token in `line` at or after `i`. Returns its span.
static bool next_token(const std::string& line, size_t i, size_t& start, size_t& end) {
    while (i < line.size() && is_space(line[i])) ++i;
    if (i >= line.size() || line[i] == ';') return false;
    start = i;
    while (i < line.size() && !is_space(line[i])) ++i;
    end = i;
    return true;
}

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t");
    return s.substr(a, b - a + 1);
}

// ---- parse ------------------------------------------------------------

BrfDoc brf_parse(const uint8_t* data, size_t size) {
    BrfDoc doc;

    // Split into lines (CRLF and LF both accepted)
    {
        std::string cur;
        for (size_t i = 0; i < size; ++i) {
            char c = (char)data[i];
            if (c == '\r') {
                if (i + 1 < size && data[i + 1] == '\n') ++i;
                doc.raw_lines.push_back(cur); cur.clear();
            } else if (c == '\n') {
                doc.raw_lines.push_back(cur); cur.clear();
            } else if (c != '\x1A') { // skip DOS EOF marker
                cur += c;
            }
        }
        if (!cur.empty()) doc.raw_lines.push_back(cur);
    }

    // We assemble the image exactly as LoadBrentDLL does: a cursor that only ever advances,
    // by the width of each field emitted. Every offset below is that cursor.
    uint32_t image = 0;

    // The file names its own records: ";---- START OF PLANE_TYPE ----". Track that, and the
    // byte offset within it, so every field carries where it lives -- read from the file
    // rather than assumed from a table.
    std::string current_section;
    uint32_t    section_offset = 0;

    for (uint32_t li = 0; li < (uint32_t)doc.raw_lines.size(); ++li) {
        const std::string& line = doc.raw_lines[li];

        size_t p = 0;
        while (p < line.size() && is_space(line[p])) ++p;
        if (p >= line.size()) continue;

        // A comment -- but a comment may DELIMIT A RECORD, which is a fact.
        if (line[p] == ';') {
            std::string s = line.substr(p);
            size_t s0 = s.find("START OF ");
            size_t e0 = s.find("END OF ");
            if (s0 != std::string::npos) {
                std::string n = s.substr(s0 + 9);
                while (!n.empty() && (n.back() == '-' || n.back() == ' ')) n.pop_back();
                current_section = n;
                section_offset  = 0;
            } else if (e0 != std::string::npos) {
                current_section.clear();
                section_offset = 0;
            }
            continue;
        }

        // The magic line, and any other bracketed tag.
        if (line[p] == '[') continue;

        // `:name` -- a label at the cursor. It emits nothing; it names an offset.
        if (line[p] == ':') {
            size_t a = p + 1, b = a;
            while (b < line.size() && !is_space(line[b]) && line[b] != ';') ++b;
            BrfBlock blk;
            blk.name   = line.substr(a, b - a);
            blk.offset = image;
            doc.blocks.push_back(blk);
            continue;
        }

        size_t ts, te;
        if (!next_token(line, p, ts, te)) continue;
        std::string type = line.substr(ts, te - ts);

        // `end` is the FIRST keyword the loader tests, and it jumps straight to the
        // allocate-and-finish path -- it terminates the FILE, not a block. All 534 shipped
        // records carry exactly one, as their last token.
        if (type == "end") break;

        const bool numeric = (type == "byte" || type == "word" || type == "dword");
        const bool is_str  = (type == "string");
        if (!numeric && !is_str && type != "ptr" && type != "symbol")
            continue;  // the loader would ErrorExit; we just skip

        // The trailing comment, if any -- outside a string's quotes.
        auto comment_from = [&](size_t from) {
            size_t semi = line.find(';', from);
            if (semi == std::string::npos) return std::string();
            return trim(line.substr(semi + 1));
        };

        auto emit = [&](const std::string& value, size_t vpos, size_t vlen,
                        const std::string& comment) {
            BrfField f;
            f.type         = type;
            f.value        = value;
            f.image_offset = image;
            f.section      = current_section;
            f.offset       = section_offset;
            f.block        = doc.blocks.empty() ? std::string() : doc.blocks.back().name;
            f.comment      = comment;
            f.line         = li;
            f.value_pos    = (uint32_t)vpos;
            f.value_len    = (uint32_t)vlen;

            const uint32_t w = brf_field_width(type, value);
            image          += w;
            section_offset += w;

            if (doc.blocks.empty()) doc.fields.push_back(f);
            else {
                doc.blocks.back().fields.push_back(f);
                if (is_str) doc.blocks.back().strings.push_back(value);
            }
        };

        if (is_str) {
            size_t q1 = line.find('"', te);
            if (q1 == std::string::npos) continue;
            size_t q2 = line.find('"', q1 + 1);
            if (q2 == std::string::npos) q2 = line.size();
            emit(line.substr(q1 + 1, q2 - q1 - 1), q1 + 1, q2 - q1 - 1, comment_from(q2));
        } else if (numeric) {
            // `word 1 2 3` emits three words: the keyword consumes operands while they are
            // numbers. No shipped file uses the repeat form, but the loader does, and a
            // decoder that assumes one-per-line would mis-assemble a file that did.
            size_t i = te;
            size_t vs, ve;
            const std::string comment = comment_from(te);
            while (next_token(line, i, vs, ve)) {
                std::string v = line.substr(vs, ve - vs);
                if (!is_number(v)) break;
                emit(v, vs, ve - vs, comment);
                i = ve;
            }
        } else {  // ptr, symbol
            size_t vs, ve;
            if (!next_token(line, te, vs, ve)) continue;
            emit(line.substr(vs, ve - vs), vs, ve - vs, comment_from(ve));
        }
    }

    doc.image_size = image;

    // A block runs until the next label (or the end of the image).
    for (size_t i = 0; i < doc.blocks.size(); ++i) {
        const uint32_t next = (i + 1 < doc.blocks.size()) ? doc.blocks[i + 1].offset
                                                          : doc.image_size;
        doc.blocks[i].width = next - doc.blocks[i].offset;
    }

    return doc;
}

// ---- lookup ----------------------------------------------------------

const BrfBlock* BrfDoc::find_block(const std::string& name) const {
    const std::string want = lower(name);
    for (auto& b : blocks)
        if (lower(b.name) == want) return &b;
    return nullptr;
}

uint32_t brf_declared_size(const BrfDoc& doc) {
    for (auto& f : doc.fields)
        if (f.image_offset == 1 && f.type == "word")
            return (uint32_t)brf_parse_int(f.value);
    return 0;
}

// ---- serialize -------------------------------------------------------

std::vector<uint8_t> brf_serialize(const BrfDoc& doc) {
    std::vector<uint8_t> out;
    for (auto& line : doc.raw_lines) {
        for (char c : line) out.push_back((uint8_t)c);
        out.push_back('\r');
        out.push_back('\n');
    }
    return out;
}

// ---- edit ------------------------------------------------------------

bool brf_set_value(BrfDoc& doc, const BrfField& field, const std::string& value) {
    // `field` usually ALIASES an element of doc.fields, and the fixup below writes to that
    // element -- so take what we need by value first, or we would be reading fields we have
    // already overwritten.
    const uint32_t at_line = field.line;
    const uint32_t at_pos  = field.value_pos;
    const uint32_t old_len = field.value_len;

    if (at_line >= doc.raw_lines.size()) return false;
    std::string& line = doc.raw_lines[at_line];
    if ((size_t)at_pos + old_len > line.size()) return false;

    line.replace(at_pos, old_len, value);

    // Anything else on this line shifted; so did this field's own value.
    const int delta = (int)value.size() - (int)old_len;
    auto fixup = [&](BrfField& f) {
        if (f.line != at_line) return;
        if (f.value_pos == at_pos) {
            f.value = value;
            f.value_len = (uint32_t)value.size();
        } else if (f.value_pos > at_pos) {
            f.value_pos = (uint32_t)((int)f.value_pos + delta);
        }
    };
    for (auto& f : doc.fields) fixup(f);
    for (auto& b : doc.blocks)
        for (auto& f : b.fields) fixup(f);
    return true;
}

// ---- value parsing ---------------------------------------------------

int64_t brf_parse_int(const std::string& value) {
    if (value.empty()) return 0;
    // Negative: ^X means -X
    if (value[0] == '^') return -brf_parse_int(value.substr(1));
    // Hex: $XXXX
    if (value[0] == '$') return (int64_t)std::strtoll(value.c_str() + 1, nullptr, 16);
    // Decimal (may be signed)
    return std::strtoll(value.c_str(), nullptr, 10);
}

} // namespace fx
