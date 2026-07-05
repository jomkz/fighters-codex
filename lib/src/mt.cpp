#include "fx/mt.h"

namespace fx {

static std::string trimmed(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r'))
        b--;
    return s.substr(a, b - a);
}

MtInfo mt_info(const TxtDoc& doc) {
    MtInfo info;
    info.sections = txt_count(doc, ".section");

    // Section 1 runs from the first ".section" line to the next one; its
    // content is: identifier line ("--<ID>  (<name>)"), title, mission type.
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
        if (field == 0 && s.size() > 2 && s[0] == '-' && s[1] == '-') {
            size_t open = s.find('(');
            size_t close = s.find(')', open == std::string::npos ? 0 : open);
            std::string id = s.substr(2, (open == std::string::npos
                                              ? s.size()
                                              : open) - 2);
            info.mission_id = trimmed(id);
            if (open != std::string::npos && close != std::string::npos)
                info.source_name = trimmed(s.substr(open + 1, close - open - 1));
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
