#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// TXT — in-game text / UI layout (see TXT.md): plain ASCII, CRLF, driven by
// the same directive engine as .MT briefings plus the UI additions
// (.button/.picture/.page). Parsing is line-preserving — every line keeps
// its raw bytes and terminator shape — so txt_write is the byte-identical
// inverse of txt_read while the directive structure is exposed on top.

namespace fx {

// Is `token` one of the directives the engine actually knows?
//
// The vocabulary is a FACT read out of the executable, not a guess: the text interpreter
// (0x47E1B0) compares each token against this exact set, and a token it does not recognise
// is just text. So `.ell` -- which a real briefing writes as prose ("get the .ell out",
// ~K30.MT) -- is NOT a directive, and a parser that calls every `.`-token one says it is.
bool txt_is_directive(const std::string& token);

struct TxtLine {
    std::string raw;        // line bytes without the terminator
    bool crlf = false;      // terminated by CRLF (else bare LF)
    bool terminated = true; // false only for a final line with no EOL
    // The directives on this line, in order (".section", "..button", ...). Only tokens the
    // engine recognises: see txt_is_directive.
    std::vector<std::string> directives;
};

struct TxtDoc {
    std::vector<TxtLine> lines;
};

enum class TxtKind {
    CampaignDescription,  // .section skeleton (see TXT.md)
    UiTemplate,           // uses .page / .button / .picture
    PlainText,            // no directives (e.g. CREDITS.TXT)
};

// Parse. Never fails: any byte stream splits into lines losslessly.
TxtDoc txt_read(const uint8_t* data, size_t size);

// Serialize — the byte-identical inverse of txt_read.
std::vector<uint8_t> txt_write(const TxtDoc& doc);

// Classification per the three documented uses.
TxtKind txt_classify(const TxtDoc& doc);

// Count of a given directive across the document (exact match, e.g.
// ".section" does not count "..section").
size_t txt_count(const TxtDoc& doc, const std::string& directive);

} // namespace fx
