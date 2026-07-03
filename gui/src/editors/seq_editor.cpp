#include "seq_editor.h"
#include "../app.h"
#include "../util.h"
#include "imgui.h"
#include "fx/seq.h"
#include <cctype>
#include <string>
#include <cstring>

static fx::SeqFile s_seq;
static int s_lastLib   = -2;
static int s_lastEntry = -2;

// Color per command keyword.
static ImVec4 CmdColor(const std::string& cmd) {
    if (cmd == "palette")            return {0.6f, 1.0f, 0.6f, 1};   // green  — palette load
    if (cmd == "fadein")             return {0.5f, 0.85f, 1.0f, 1};  // cyan   — transition
    if (cmd == "fadeout")            return {0.5f, 0.85f, 1.0f, 1};  // cyan
    if (cmd == "video")              return {1.0f, 0.7f, 0.7f, 1};   // rose   — video playback
    if (cmd == "bitmap")             return {1.0f, 0.8f, 0.5f, 1};   // amber  — still image
    if (cmd == "sound")              return {1.0f, 0.9f, 0.4f, 1};   // yellow — audio
    if (cmd == "font")               return {0.8f, 0.7f, 1.0f, 1};   // lavender — font
    if (cmd == "wait" || cmd == "sync") return {0.75f, 0.75f, 0.75f, 1}; // grey — control flow
    return {0.85f, 0.82f, 0.78f, 1}; // off-white — unknown
}

// Valid time token: optional '+', then at least one digit (seq.cpp rules).
static bool ValidTimeToken(const char* s) {
    if (*s == '+') ++s;
    if (!*s) return false;
    for (; *s; ++s)
        if (!isdigit((unsigned char)*s)) return false;
    return true;
}

// Re-parse one rebuilt event body through the codec so row metadata always
// matches what seq_parse produces — no tokenizer duplicated here.
static fx::SeqEvent ReparseLine(const std::string& body) {
    std::string line = "\t" + body + "\r\n";
    fx::SeqFile f = fx::seq_parse((const uint8_t*)line.data(), line.size());
    return f.events.empty() ? fx::SeqEvent{} : f.events[0];
}

// Compose an event body from row fields, tab-separated per the SEQ.md
// layout: <time>\t[sync\t]<command>[\t<args...>].
static std::string ComposeBody(const char* timeTok, bool sync,
                               const char* cmd, const char* args) {
    std::string body = timeTok;
    body += "\t";
    if (sync) body += "sync\t";
    body += cmd;
    if (args[0]) { body += "\t"; body += args; }
    return body;
}

// Blank event with defaults; relative rows spawn relative "+n" neighbours so
// chains of '+' offsets keep resolving (an absolute time dropped into a
// relative chain rebases every event after it).
static fx::SeqEvent MakeBlankEvent(int ticks, bool relative) {
    std::string tok = (relative ? "+" : "") + std::to_string(ticks);
    return ReparseLine(tok + "\tsound");
}

void DrawSeqEditor(App& app) {
    auto& ed = app.editor;

    if (ed.libIdx != s_lastLib || ed.entryIdx != s_lastEntry) {
        s_lastLib   = ed.libIdx;
        s_lastEntry = ed.entryIdx;
        s_seq = fx::seq_parse(ed.data.data(), ed.data.size());
    }

    int evCount = 0;
    for (bool b : s_seq.is_event) if (b) evCount++;

    ImGui::TextDisabled("%d events", evCount);

    bool changed = false;

    // "Append event" toolbar button
    ImGui::SameLine();
    if (ImGui::SmallButton("+ Add Event")) {
        // Resolve the timeline end: '+' times chain off the previous event,
        // so a plain max over ticks undercounts relative sequences.
        int endTick = 0;
        for (size_t i = 0; i < s_seq.events.size(); i++) {
            if (!s_seq.is_event[i] || s_seq.events[i].command.empty()) continue;
            const auto& ev = s_seq.events[i];
            endTick = ev.relative ? endTick + ev.ticks : ev.ticks;
        }

        fx::SeqEvent ne = MakeBlankEvent(endTick + 100, false);
        s_seq.lines.push_back("\t" + ne.raw);
        s_seq.is_event.push_back(true);
        s_seq.events.push_back(ne);
        changed = true;
    }

    ImGui::Separator();

    // Deferred structural edits (collected during table iteration, applied after)
    int  deleteIdx  = -1;    // lines[] index to delete
    int  insertIdx  = -1;    // lines[] index; insert new event AFTER this index
    int  insertTick = 0;
    bool insertRel  = false;

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 3));
    if (ImGui::BeginTable("##seq", 5,
            ImGuiTableFlags_ScrollY     |
            ImGuiTableFlags_RowBg       |
            ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Time",    ImGuiTableColumnFlags_WidthFixed,   70);
        ImGui::TableSetupColumn("Cmd",     ImGuiTableColumnFlags_WidthFixed,   90);
        ImGui::TableSetupColumn("Args",    ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Sync",    ImGuiTableColumnFlags_WidthFixed,   40);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed,   60);
        ImGui::TableHeadersRow();

        int rowId = 0;
        for (size_t i = 0; i < s_seq.lines.size(); ++i) {
            if (!s_seq.is_event[i]) continue;
            auto& ev = s_seq.events[i];
            ImGui::TableNextRow();
            ImGui::PushID(rowId++);

            // Current field texts. Args come from the raw line so spacing
            // and quotes survive exactly as typed.
            std::string timeTok = (ev.relative ? "+" : "") + std::to_string(ev.ticks);
            std::string argsStr;
            {
                const char* p = ev.raw.c_str();
                while (*p && *p != ' ' && *p != '\t') ++p; // skip time
                while (*p == ' ' || *p == '\t') ++p;
                if (ev.sync) {
                    while (*p && *p != ' ' && *p != '\t') ++p;
                    while (*p == ' ' || *p == '\t') ++p;
                }
                while (*p && *p != ' ' && *p != '\t') ++p; // skip command
                while (*p == ' ' || *p == '\t') ++p;
                argsStr = p;
            }

            char timeBuf[16];
            char cmdBuf[32];
            char argsBuf[512];
            fxg::copy_str(timeBuf, sizeof(timeBuf), timeTok);
            fxg::copy_str(cmdBuf,  sizeof(cmdBuf),  ev.command);
            fxg::copy_str(argsBuf, sizeof(argsBuf), argsStr);

            bool rowEdited = false;

            // Time column — accepts "N" (absolute) or "+N" (relative)
            ImGui::TableSetColumnIndex(0);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##time", timeBuf, sizeof(timeBuf)) &&
                ValidTimeToken(timeBuf))
                rowEdited = true;

            // Cmd column — coloured by command type, free text (unknown
            // commands exist in the wild and must stay editable)
            ImGui::TableSetColumnIndex(1);
            ImGui::PushStyleColor(ImGuiCol_Text, CmdColor(ev.command));
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##cmd", cmdBuf, sizeof(cmdBuf)) && cmdBuf[0])
                rowEdited = true;
            ImGui::PopStyleColor();

            // Args column
            ImGui::TableSetColumnIndex(2);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##args", argsBuf, sizeof(argsBuf)))
                rowEdited = true;

            // Sync column
            ImGui::TableSetColumnIndex(3);
            bool sync = ev.sync;
            if (ImGui::Checkbox("##sync", &sync))
                rowEdited = true;

            if (rowEdited) {
                std::string body = ComposeBody(timeBuf, sync, cmdBuf, argsBuf);
                s_seq.events[i] = ReparseLine(body);
                s_seq.lines[i]  = "\t" + body;
                changed = true;
            }

            // Actions column
            ImGui::TableSetColumnIndex(4);
            if (ImGui::SmallButton("+")) {
                insertIdx  = (int)i;
                insertRel  = ev.relative;
                insertTick = ev.relative ? 1 : ev.ticks + 1;
            }
            ImGui::SameLine(0, 2);
            if (ImGui::SmallButton("x")) {
                deleteIdx = (int)i;
            }

            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();

    // Apply deferred delete
    if (deleteIdx >= 0 && deleteIdx < (int)s_seq.lines.size()) {
        s_seq.lines.erase(   s_seq.lines.begin()    + deleteIdx);
        s_seq.is_event.erase(s_seq.is_event.begin() + deleteIdx);
        s_seq.events.erase(  s_seq.events.begin()   + deleteIdx);
        changed = true;
    }
    // Apply deferred insert (after the target index)
    else if (insertIdx >= 0) {
        fx::SeqEvent ne = MakeBlankEvent(insertTick, insertRel);
        int pos = insertIdx + 1;
        s_seq.lines.insert(   s_seq.lines.begin()    + pos, "\t" + ne.raw);
        s_seq.is_event.insert(s_seq.is_event.begin() + pos, true);
        s_seq.events.insert(  s_seq.events.begin()   + pos, ne);
        changed = true;
    }

    if (changed) {
        ed.data     = fx::seq_serialize(s_seq);
        ed.modified = true;
    }
}
