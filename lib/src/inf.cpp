#include "fx/inf.h"
#include <sstream>

namespace fx {

InfFile inf_parse(const uint8_t* data, size_t size) {
    InfFile result{};
    if (!data || size == 0) return result;

    std::string raw(reinterpret_cast<const char*>(data), size);
    std::istringstream ss(raw);
    std::string line;

    InfSection cur;

    auto flush = [&]() {
        auto& t = cur.text;
        while (!t.empty() && (t.back() == '\n' || t.back() == '\r' || t.back() == ' '))
            t.pop_back();
        if (!cur.directive.empty() || !t.empty())
            result.sections.push_back(cur);
        cur = {};
    };

    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (!line.empty() && line[0] == '.') {
            flush();
            cur.directive = line;
            continue;
        }

        // Detect measurement footer lines in two formats:
        //   Format A: "LENGTH (m): 16.26"   (colon-space separator)
        //   Format B: "LENGTH (m) 16.26"    (space separator, no colon)
        // Both: no leading whitespace, key contains a unit in parens.
        if (!line.empty() && line[0] != ' ') {
            auto colon = line.find(": ");
            if (colon != std::string::npos) {
                const std::string key = line.substr(0, colon);
                const std::string val = line.substr(colon + 2);
                if (key.find('(') != std::string::npos && key.find(')') != std::string::npos) {
                    result.stats[key] = val;
                    continue;
                }
            } else {
                // Format B: find the last ')' followed by a space and a digit
                auto rparen = line.rfind(')');
                if (rparen != std::string::npos && rparen + 2 < line.size() &&
                    line[rparen + 1] == ' ' && std::isdigit((unsigned char)line[rparen + 2])) {
                    const std::string key = line.substr(0, rparen + 1);
                    const std::string val = line.substr(rparen + 2);
                    if (key.find('(') != std::string::npos) {
                        result.stats[key] = val;
                        continue;
                    }
                }
            }
        }

        cur.text += line;
        cur.text += '\n';
    }
    flush();

    result.valid = true;
    return result;
}

} // namespace fx
