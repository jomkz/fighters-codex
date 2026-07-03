#include "inf_editor.h"
#include "../app.h"
#include "imgui.h"
#include "fx/inf.h"
#include <string>

// INF files are plain text with dot-command directives (.body .right,
// .title .center — see docs/fa/formats/INF.md; they are NOT RTF). The styled
// tab renders and edits per section; the source tab edits the raw text.

static fx::InfFile s_inf;
static std::string s_srcText;
static int         s_lastLib  = -2;
static int         s_lastEntry = -2;
static int         s_editing  = -1;   // section index in edit mode, -1 = none
static std::string s_editBuf;

struct SectionStyle {
    bool title = false;
    int  align = 0;  // 0 left, 1 center, 2 right
};

static SectionStyle ParseStyle(const std::string& directive) {
    SectionStyle st;
    st.title = directive.find(".title")  != std::string::npos;
    if      (directive.find(".center") != std::string::npos) st.align = 1;
    else if (directive.find(".right")  != std::string::npos) st.align = 2;
    return st;
}

// Compose a directive in the corpus's observed shape (".body .left" etc.).
static std::string MakeDirective(const SectionStyle& st) {
    std::string d = st.title ? ".title" : ".body";
    d += (st.align == 1) ? " .center" : (st.align == 2) ? " .right" : " .left";
    return d;
}

// Serialize the current sections into the editor buffer and source mirror.
static void SyncData(App& app) {
    app.editor.data     = fx::inf_serialize(s_inf);
    app.editor.modified = true;
    s_srcText.assign((const char*)app.editor.data.data(),
                     app.editor.data.size());
}

static void ApplyStyle(App& app, fx::InfSection& s, const SectionStyle& st) {
    s.directive = MakeDirective(st);
    fx::inf_rebuild_section(s);
    SyncData(app);
}

// One text line, aligned within the content region.
static void AlignedLine(const char* begin, const char* end, int align) {
    if (align == 0) {
        ImGui::TextWrapped("%.*s", (int)(end - begin), begin);
        return;
    }
    float avail = ImGui::GetContentRegionAvail().x;
    float width = ImGui::CalcTextSize(begin, end).x;
    float x     = (align == 1) ? (avail - width) * 0.5f : avail - width;
    if (x > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + x);
    ImGui::TextUnformatted(begin, end);
}

static void DrawStyledTab(App& app) {
    if (ImGui::SmallButton("+ Add Section")) {
        fx::InfSection ns;
        ns.directive = ".body .left";
        fx::inf_rebuild_section(ns);
        s_inf.sections.push_back(ns);
        s_editing = (int)s_inf.sections.size() - 1;
        s_editBuf.clear();
        SyncData(app);
    }
    ImGui::Separator();

    ImGui::BeginChild("##styled", ImVec2(0, -32));

    int deleteIdx = -1, insertIdx = -1;
    for (int i = 0; i < (int)s_inf.sections.size(); i++) {
        auto& s = s_inf.sections[i];
        SectionStyle st = ParseStyle(s.directive);
        ImGui::PushID(i);

        // Section header row: directive label + controls
        ImGui::TextDisabled("%s", s.directive.empty() ? "(no directive)"
                                                      : s.directive.c_str());
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 190);
        if (ImGui::SmallButton("L")) { st.align = 0; ApplyStyle(app, s, st); }
        ImGui::SameLine(0, 2);
        if (ImGui::SmallButton("C")) { st.align = 1; ApplyStyle(app, s, st); }
        ImGui::SameLine(0, 2);
        if (ImGui::SmallButton("R")) { st.align = 2; ApplyStyle(app, s, st); }
        ImGui::SameLine(0, 8);
        if (ImGui::SmallButton(st.title ? "Body" : "Title")) {
            st.title = !st.title;
            ApplyStyle(app, s, st);
        }
        ImGui::SameLine(0, 8);
        if (s_editing == i) {
            if (ImGui::SmallButton("Apply")) {
                s.text = s_editBuf;
                fx::inf_rebuild_section(s);
                s_editing = -1;
                SyncData(app);
            }
            ImGui::SameLine(0, 2);
            if (ImGui::SmallButton("Cancel")) s_editing = -1;
        } else {
            if (ImGui::SmallButton("Edit")) {
                s_editing = i;
                s_editBuf = s.text;
            }
        }
        ImGui::SameLine(0, 8);
        if (ImGui::SmallButton("+")) insertIdx = i;
        ImGui::SameLine(0, 2);
        if (ImGui::SmallButton("x")) deleteIdx = i;

        // Section content: edit box or styled render
        if (s_editing == i) {
            ImGui::InputTextMultiline(
                "##edit", s_editBuf.data(), s_editBuf.size() + 1,
                ImVec2(-1, ImGui::GetTextLineHeight() * 6),
                ImGuiInputTextFlags_AllowTabInput |
                ImGuiInputTextFlags_CallbackResize,
                [](ImGuiInputTextCallbackData* d) -> int {
                    if (d->EventFlag == ImGuiInputTextFlags_CallbackResize) {
                        auto* str = (std::string*)d->UserData;
                        str->resize((size_t)d->BufSize - 1);
                        d->Buf = str->data();
                    }
                    return 0;
                }, &s_editBuf);
        } else if (s.text.empty()) {
            ImGui::TextDisabled("(empty)");
        } else {
            if (st.title) ImGui::SetWindowFontScale(1.35f);
            const char* p   = s.text.c_str();
            const char* fin = p + s.text.size();
            while (p <= fin) {
                const char* nl = p;
                while (nl < fin && *nl != '\n') ++nl;
                if (nl == p) ImGui::Dummy(ImVec2(1, ImGui::GetTextLineHeight()));
                else         AlignedLine(p, nl, st.align);
                if (nl >= fin) break;
                p = nl + 1;
            }
            if (st.title) ImGui::SetWindowFontScale(1.0f);
        }
        ImGui::Separator();
        ImGui::PopID();
    }

    if (deleteIdx >= 0) {
        s_inf.sections.erase(s_inf.sections.begin() + deleteIdx);
        if (s_editing == deleteIdx) s_editing = -1;
        SyncData(app);
    } else if (insertIdx >= 0) {
        fx::InfSection ns;
        ns.directive = ".body .left";
        fx::inf_rebuild_section(ns);
        s_inf.sections.insert(s_inf.sections.begin() + insertIdx + 1, ns);
        s_editing = insertIdx + 1;
        s_editBuf.clear();
        SyncData(app);
    }

    ImGui::EndChild();
}

static void DrawSourceTab(App& app) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    avail.y -= 32;
    if (ImGui::InputTextMultiline("##infsrc", s_srcText.data(),
                                  s_srcText.size() + 1, avail,
                                  ImGuiInputTextFlags_AllowTabInput |
                                  ImGuiInputTextFlags_CallbackResize,
                                  [](ImGuiInputTextCallbackData* d) -> int {
                                      if (d->EventFlag == ImGuiInputTextFlags_CallbackResize) {
                                          auto* s = (std::string*)d->UserData;
                                          s->resize((size_t)d->BufSize - 1);
                                          d->Buf = s->data();
                                      }
                                      return 0;
                                  }, &s_srcText)) {
        app.editor.data.assign(s_srcText.begin(), s_srcText.end());
        app.editor.modified = true;
        s_inf     = fx::inf_parse(app.editor.data.data(), app.editor.data.size());
        s_editing = -1;
    }
}

void DrawInfEditor(App& app) {
    auto& ed = app.editor;

    if (ed.libIdx != s_lastLib || ed.entryIdx != s_lastEntry) {
        s_lastLib   = ed.libIdx;
        s_lastEntry = ed.entryIdx;
        s_inf = fx::inf_parse(ed.data.data(), ed.data.size());
        s_srcText.assign((const char*)ed.data.data(), ed.data.size());
        s_editing = -1;
    }

    if (ImGui::BeginTabBar("##inf-tabs")) {
        if (ImGui::BeginTabItem("Styled")) {
            DrawStyledTab(app);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Source")) {
            DrawSourceTab(app);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    if (ImGui::Button("Save")) {
        app.CommitEntry(ed.data);
    }
}
