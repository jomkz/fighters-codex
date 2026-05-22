#include "seq_editor.h"
#include "../app.h"
#include "imgui.h"
#include "fx/seq.h"
#include <string>
#include <cstring>

static fx::SeqFile s_seq;
static int s_lastLib   = -2;
static int s_lastEntry = -2;

// Color per command keyword.
static ImVec4 CmdColor(const std::string& cmd) {
    if (cmd == "palette")            return {0.6f, 1.0f, 0.6f, 1};   // green  â€” palette load
    if (cmd == "fadein")             return {0.5f, 0.85f, 1.0f, 1};  // cyan   â€” transition
    if (cmd == "fadeout")            return {0.5f, 0.85f, 1.0f, 1};  // cyan
    if (cmd == "video")              return {1.0f, 0.7f, 0.7f, 1};   // rose   â€” video playback
    if (cmd == "bitmap")             return {1.0f, 0.8f, 0.5f, 1};   // amber  â€” still image
    if (cmd == "sound")              return {1.0f, 0.9f, 0.4f, 1};   // yellow â€” audio
    if (cmd == "font")               return {0.8f, 0.7f, 1.0f, 1};   // lavender â€” font
    if (cmd == "wait" || cmd == "sync") return {0.75f, 0.75f, 0.75f, 1}; // grey â€” control flow
    return {0.85f, 0.82f, 0.78f, 1}; // off-white â€” unknown
}

// Build a blank SeqEvent raw line with reasonable defaults.
static fx::SeqEvent MakeBlankEvent(int ticks) {
    fx::SeqEvent e;
    e.relative = false;
    e.ticks    = ticks;
    e.sync     = false;
    e.command  = "sound";
    e.raw      = std::to_string(ticks) + "\tsound";
    return e;
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

    // "Append event" toolbar button
    ImGui::SameLine();
    if (ImGui::SmallButton("+ Add Event")) {
        // Find last tick value to use as base for new event time
        int lastTick = 0;
        for (size_t i = 0; i < s_seq.events.size(); i++)
            if (s_seq.is_event[i] && s_seq.events[i].ticks > lastTick)
                lastTick = s_seq.events[i].ticks;

        fx::SeqEvent ne = MakeBlankEvent(lastTick + 100);
        s_seq.lines.push_back("\t" + ne.raw);
        s_seq.is_event.push_back(true);
        s_seq.events.push_back(ne);
        ed.data     = fx::seq_serialize(s_seq);
        ed.modified = true;
    }

    ImGui::Separator();

    // Deferred structural edits (collected during table iteration, applied after)
    int deleteIdx  = -1;  // lines[] index to delete
    int insertIdx  = -1;  // lines[] index; insert new event AFTER this index
    int insertTick = 0;

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 3));
    if (ImGui::BeginTable("##seq", 5,
            ImGuiTableFlags_ScrollY     |
            ImGuiTableFlags_RowBg       |
            ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Time",    ImGuiTableColumnFlags_WidthFixed,   70);
        ImGui::TableSetupColumn("Cmd",     ImGuiTableColumnFlags_WidthFixed,   72);
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

            // Time column
            ImGui::TableSetColumnIndex(0);
            if (ev.relative)
                ImGui::TextDisabled("+%d", ev.ticks);
            else
                ImGui::Text("%d", ev.ticks);

            // Cmd column â€” coloured by command type
            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(CmdColor(ev.command), "%s", ev.command.c_str());

            // Args column â€” editable; reconstruct raw line on edit
            ImGui::TableSetColumnIndex(2);
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

            char argsBuf[512];
            size_t copyLen = argsStr.size() < sizeof(argsBuf) - 1
                             ? argsStr.size() : sizeof(argsBuf) - 1;
            memcpy(argsBuf, argsStr.c_str(), copyLen);
            argsBuf[copyLen] = '\0';

            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##args", argsBuf, sizeof(argsBuf))) {
                std::string newRaw = (ev.relative ? "+" : "") +
                                     std::to_string(ev.ticks) + "\t";
                if (ev.sync) newRaw += "sync\t";
                newRaw += ev.command;
                if (argsBuf[0]) { newRaw += " "; newRaw += argsBuf; }
                ev.raw = newRaw;
                ev.args.clear();
                const char* ap = argsBuf;
                while (*ap) {
                    while (*ap == ' ' || *ap == '\t') ++ap;
                    if (!*ap) break;
                    if (*ap == '"') {
                        ++ap;
                        std::string tok;
                        while (*ap && *ap != '"') tok += *ap++;
                        if (*ap == '"') ++ap;
                        ev.args.push_back(tok);
                    } else {
                        std::string tok;
                        while (*ap && *ap != ' ' && *ap != '\t') tok += *ap++;
                        ev.args.push_back(tok);
                    }
                }
                s_seq.lines[i] = "\t" + ev.raw;
                ed.modified = true;
            }

            // Sync column
            ImGui::TableSetColumnIndex(3);
            if (ev.sync)
                ImGui::TextDisabled("sync");

            // Actions column
            ImGui::TableSetColumnIndex(4);
            if (ImGui::SmallButton("+")) {
                insertIdx  = (int)i;
                insertTick = ev.ticks + 1;
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
        ed.data     = fx::seq_serialize(s_seq);
        ed.modified = true;
    }
    // Apply deferred insert (after the target index)
    else if (insertIdx >= 0) {
        fx::SeqEvent ne = MakeBlankEvent(insertTick);
        int pos = insertIdx + 1;
        s_seq.lines.insert(   s_seq.lines.begin()    + pos, "\t" + ne.raw);
        s_seq.is_event.insert(s_seq.is_event.begin() + pos, true);
        s_seq.events.insert(  s_seq.events.begin()   + pos, ne);
        ed.data     = fx::seq_serialize(s_seq);
        ed.modified = true;
    }

    if (ed.modified) {
        ed.data = fx::seq_serialize(s_seq);
    }
}
