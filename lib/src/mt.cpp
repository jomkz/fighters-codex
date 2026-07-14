#include "fx/mt.h"

namespace fx {

static std::string trimmed(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r'))
        b--;
    return s.substr(a, b - a);
}

// Is this the identifier line? It is the one shaped `<ID>  (<annotation>)`: a single
// unspaced token, then a parenthesised note.
//
// It is NOT identified by a leading `--`. MT.md used to say the `--` was "the engine's cue
// (inferred)" to read the line as an ID -- and the codec believed it. The data says
// otherwise: of the 345 shipped .MT files that carry an identifier line, only 100 write it
// `--AB01`, 244 write a bare `AB01`, and ~FANOTH.MT writes `-RB12` with one dash. So the
// dashes are decoration, and requiring them dropped the ID on 263 of 363 files and shifted
// every other field up by one -- the title came out as the ID line and the mission type as
// the title. The round-trip never saw it: txt_write replays the file's own lines.
//
// The engine settles nothing here, because the engine never parses this: it RENDERS section 1
// through the text interpreter (0x47E1B0) like any other text. So the data's own convention
// is the only authority, and the shape is what it is consistent about.
static bool parse_id_line(const std::string& s, std::string* id, std::string* annotation) {
    size_t a = 0;
    while (a < s.size() && s[a] == '-') a++;   // leading dashes are decoration
    size_t open = s.find('(', a);
    if (open == std::string::npos) return false;
    size_t close = s.find(')', open + 1);
    if (close == std::string::npos) return false;

    std::string token = trimmed(s.substr(a, open - a));
    // A single unspaced token. `QUICK MISSION` is a title, not an identifier.
    if (token.empty() || token.find(' ') != std::string::npos ||
        token.find('\t') != std::string::npos)
        return false;

    *id = token;
    *annotation = trimmed(s.substr(open + 1, close - open - 1));
    return true;
}

MtInfo mt_info(const TxtDoc& doc) {
    MtInfo info;
    info.sections = txt_count(doc, ".section");

    // Section 1 runs from the first ".section" line to the next one; its content is the
    // identifier line, the title, and the mission type -- each optional.
    size_t i = 0;
    while (i < doc.lines.size() && doc.lines[i].directives.empty())
        i++;
    if (i == doc.lines.size()) return info;
    if (doc.lines[i].directives[0] != ".section") return info;
    i++;

    int field = 0;
    for (; i < doc.lines.size(); i++) {
        const TxtLine& line = doc.lines[i];
        if (!line.directives.empty() && line.directives[0] == ".section")
            break;
        std::string s = trimmed(line.raw);
        if (s.empty()) continue;
        if (field == 0 && parse_id_line(s, &info.mission_id, &info.source_name)) {
            field = 1;
        } else if (field <= 1) {
            info.title = s;
            field = 2;
        } else if (field == 2) {
            info.mission_type = s;
            field = 3;
        }
    }
    return info;
}

} // namespace fx
