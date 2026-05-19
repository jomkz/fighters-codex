#include "ft/ai.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstring>
#include <map>
#include <set>
#include <cstdio>
#include <sstream>

namespace ft {

// ---- Lexer ------------------------------------------------------------------

enum TokType {
    T_EOF, T_EOL,
    T_NUMBER, T_IDENT, T_VAR, T_STRING,
    T_COLON, T_DOT, T_HASH,
    T_EQ, T_PLUS, T_MINUS, T_STAR, T_SLASH, T_PERCENT,
    T_LT, T_GT, T_LE, T_GE, T_EQEQ, T_NE,
    T_ANDAND, T_OROR, T_COMMA
};

struct Token {
    TokType     type;
    std::string str;            // IDENT value or OP string for debug
    int         ival;           // NUMBER value or VAR index (0-3)
    int         line;
    bool        preceded_by_space = false; // true if whitespace appeared before this token
};

struct Lexer {
    const char*         src;
    size_t              len;
    size_t              pos;
    int                 line;
    std::vector<Token>  toks;
    size_t              ti;

    void tokenize() {
        pos = 0; line = 1; ti = 0;
        bool had_space = false; // whitespace appeared since last token
        while (pos < len) {
            char c = src[pos];
            if (c == '\r' || c == '\n') {
                if (toks.empty() || toks.back().type != T_EOL)
                    toks.push_back({T_EOL,"",0,line,had_space});
                if (c == '\r' && pos+1 < len && src[pos+1] == '\n') pos++;
                line++; pos++;
                had_space = false;
                continue;
            }
            if (c == ' ' || c == '\t') { pos++; had_space = true; continue; }
            if (c == ';') {
                while (pos < len && src[pos] != '\r' && src[pos] != '\n') pos++;
                continue;
            }
            // Helper macro: emit a token then reset had_space
#define EMIT(type_, str_, ival_) do { toks.push_back({(type_),(str_),(ival_),line,had_space}); had_space=false; } while(0)
            if (c == '#') { EMIT(T_HASH,"#",0); pos++; continue; }
            if (c == ':') { EMIT(T_COLON,":",0); pos++; continue; }
            if (c == '.') { EMIT(T_DOT,".",0); pos++; continue; }
            if (c == ',') { EMIT(T_COMMA,",",0); pos++; continue; }
            if (c == '+') { EMIT(T_PLUS,"+",0); pos++; continue; }
            if (c == '-') { EMIT(T_MINUS,"-",0); pos++; continue; }
            if (c == '*') { EMIT(T_STAR,"*",0); pos++; continue; }
            if (c == '/') { EMIT(T_SLASH,"/",0); pos++; continue; }
            if (c == '=') {
                if (pos+1 < len && src[pos+1] == '=') { EMIT(T_EQEQ,"==",0); pos+=2; }
                else { EMIT(T_EQ,"=",0); pos++; }
                continue;
            }
            if (c == '<') {
                if (pos+1 < len && src[pos+1] == '=') { EMIT(T_LE,"<=",0); pos+=2; }
                else { EMIT(T_LT,"<",0); pos++; }
                continue;
            }
            if (c == '>') {
                if (pos+1 < len && src[pos+1] == '=') { EMIT(T_GE,">=",0); pos+=2; }
                else { EMIT(T_GT,">",0); pos++; }
                continue;
            }
            if (c == '!' && pos+1 < len && src[pos+1] == '=') {
                EMIT(T_NE,"!=",0); pos+=2; continue;
            }
            if (c == '&' && pos+1 < len && src[pos+1] == '&') {
                EMIT(T_ANDAND,"&&",0); pos+=2; continue;
            }
            if (c == '|' && pos+1 < len && src[pos+1] == '|') {
                EMIT(T_OROR,"||",0); pos+=2; continue;
            }
            if (c == '%') {
                if (pos+1 < len && src[pos+1] >= 'a' && src[pos+1] <= 'd' &&
                    (pos+2 >= len || !isalnum((unsigned char)src[pos+2]))) {
                    int idx = src[pos+1] - 'a';
                    std::string vs; vs += c; vs += src[pos+1];
                    toks.push_back({T_VAR, vs, idx, line, had_space});
                    had_space = false;
                    pos += 2;
                } else {
                    EMIT(T_PERCENT,"%",0); pos++;
                }
                continue;
            }
            if (isdigit((unsigned char)c)) {
                bool ps = had_space;
                int v = 0;
                while (pos < len && isdigit((unsigned char)src[pos]))
                    v = v*10 + (src[pos++] - '0');
                toks.push_back({T_NUMBER,"",v,line,ps});
                had_space = false;
                continue;
            }
            if (isalpha((unsigned char)c) || c == '_') {
                bool ps = had_space;
                std::string s;
                while (pos < len && (isalnum((unsigned char)src[pos]) || src[pos] == '_'))
                    s += src[pos++];
                toks.push_back({T_IDENT,s,0,line,ps});
                had_space = false;
                continue;
            }
            if (c == '"') {
                bool ps = had_space;
                pos++;
                std::string s;
                while (pos < len && src[pos] != '"') s += src[pos++];
                if (pos < len) pos++;
                toks.push_back({T_STRING,s,0,line,ps});
                had_space = false;
                continue;
            }
            pos++; // skip unknown characters
#undef EMIT
        }
        toks.push_back({T_EOF,"",0,line,false});
    }

    Token& cur()              { return toks[ti]; }
    Token& peek(int n = 1)    { size_t i = ti + n; return i < toks.size() ? toks[i] : toks.back(); }
    Token  eat()              { return toks[ti < toks.size()-1 ? ti++ : ti]; }
    bool   at(TokType t)      { return cur().type == t; }
    bool   eat_if(TokType t)  { if (at(t)) { eat(); return true; } return false; }
    void   skip_eols()        { while (at(T_EOL)) eat(); }
    void   skip_line() {
        while (!at(T_EOL) && !at(T_EOF)) eat();
        eat_if(T_EOL);
    }
    // Check for keyword case-insensitively
    bool ident_is(const char* kw) {
        if (!at(T_IDENT)) return false;
        std::string s = cur().str;
        for (char& ch : s) ch = (char)tolower((unsigned char)ch);
        return s == kw;
    }
};

// ---- Code generator ---------------------------------------------------------

struct Backpatch {
    uint32_t    patch_off;  // offset of the 2-byte s16 field in code[]
    std::string label;      // if non-empty: resolve via label map
};

struct Codegen {
    std::vector<uint8_t>         code;
    std::map<std::string,uint32_t> labels;  // lowercase → bytecode offset
    std::vector<Backpatch>       patches;
    std::vector<AiCompileError>& errors;
    int                          stmt_idx = 0;

    explicit Codegen(std::vector<AiCompileError>& e) : errors(e) {}

    void emit(uint8_t b) { code.push_back(b); }

    void emit16(int16_t v) {
        emit((uint8_t)((uint16_t)v & 0xFF));
        emit((uint8_t)((uint16_t)v >> 8));
    }
    void emit32(int32_t v) {
        emit((uint8_t)((uint32_t)v));
        emit((uint8_t)((uint32_t)v >> 8));
        emit((uint8_t)((uint32_t)v >> 16));
        emit((uint8_t)((uint32_t)v >> 24));
    }

    uint32_t offset() const { return (uint32_t)code.size(); }

    void emit_push(int32_t v) {
        if ((uint32_t)v == 0x7FFFFFFFu) { emit(0x01); emit32(v); }
        else if (v >= -128 && v <= 127)  { emit(0x03); emit((uint8_t)(int8_t)v); }
        else if (v >= -32768 && v <= 32767) { emit(0x02); emit16((int16_t)v); }
        else { emit(0x01); emit32(v); }
    }

    void emit_frame(int /*src_line*/) {
        emit(0x28);
        emit16((int16_t)(++stmt_idx));
        emit16(0);  // second field unused (reader not confirmed)
    }

    void emit_call_by_name(const std::string& name) {
        emit(0x27);
        for (char c : name) emit((uint8_t)c);
        emit(0);
    }

    void emit_push_addr(const std::string& str) {
        emit(0x07);
        for (char c : str) emit((uint8_t)c);
        emit(0);
    }

    // Emit GOTO with label placeholder; returns patch field offset
    uint32_t emit_goto_label(const std::string& label) {
        emit(0x20);
        uint32_t off = offset();
        patches.push_back({off, label});
        emit16(0);
        return off;
    }

    // Emit GOTO to absolute known offset
    void emit_goto_abs(uint32_t target) {
        emit(0x20);
        emit16((int16_t)(int32_t)target);
    }

    // Emit IF_FALSE; patch field will be filled by patch_to_here()
    uint32_t emit_if_false_placeholder() {
        emit(0x23);
        uint32_t off = offset();
        emit16(0);  // placeholder — caller must patch
        return off;
    }

    // Patch the 2-byte s16 field at patch_off to hold current offset
    void patch_to_here(uint32_t patch_off) {
        int16_t v = (int16_t)(int32_t)offset();
        code[patch_off]   = (uint8_t)((uint16_t)v & 0xFF);
        code[patch_off+1] = (uint8_t)((uint16_t)v >> 8);
    }

    bool resolve_patches() {
        bool ok = true;
        for (const auto& p : patches) {
            auto it = labels.find(p.label);
            if (it == labels.end()) {
                errors.push_back({0, "undefined label: " + p.label});
                ok = false;
                continue;
            }
            int16_t v = (int16_t)(int32_t)(it->second);
            code[p.patch_off]   = (uint8_t)((uint16_t)v & 0xFF);
            code[p.patch_off+1] = (uint8_t)((uint16_t)v >> 8);
        }
        return ok;
    }
};

// ---- Instruction table ------------------------------------------------------

struct InstrDef {
    const char* name;       // lowercase identifier as it appears in AI source
    int         num_args;   // number of expression arguments
    bool        string_arg; // first arg is a string literal → PUSH_ADDR
};

static const InstrDef kInstrs[] = {
    {"move",        5, false},
    {"movetoalt",   4, false},
    {"homepos",     5, false},
    {"homeangle",   5, false},
    {"jink",        8, false},
    {"circle",      6, false},
    {"maneuver",    1, true},   // string arg → PUSH_ADDR
    {"immelman",    1, false},
    {"invert",      0, false},
    {"yoyo",        3, false},
    {"btoh",        0, false},
    {"wm_break",    2, false},
    {"wm_approach", 3, false},
    {"wm_hspacing", 1, false},
    {"turn",        6, false},
};

static const InstrDef* find_instr(const std::string& lower) {
    for (const auto& d : kInstrs)
        if (lower == d.name) return &d;
    return nullptr;
}

// ---- Parser + codegen -------------------------------------------------------

struct Parser {
    Lexer&                       lex;
    Codegen&                     cg;
    std::vector<AiCompileError>& errors;

    Parser(Lexer& l, Codegen& c, std::vector<AiCompileError>& e)
        : lex(l), cg(c), errors(e) {}

    void err(const std::string& msg) {
        errors.push_back({lex.cur().line, msg});
    }

    static std::string to_lower(const std::string& s) {
        std::string r = s;
        for (char& c : r) c = (char)tolower((unsigned char)c);
        return r;
    }

    // ---- Expression parsers (each leaves one value on the eval stack) ----

    void parse_primary() {
        Token& t = lex.cur();
        if (t.type == T_NUMBER) {
            cg.emit_push(t.ival); lex.eat(); return;
        }
        if (t.type == T_VAR) {
            cg.emit(0x06); cg.emit((uint8_t)t.ival); lex.eat(); return;
        }
        if (t.type == T_IDENT) {
            std::string lower = to_lower(t.str);
            int src_line = t.line;
            lex.eat();

            if (lower == "any") {
                cg.emit_push((int32_t)0x7FFFFFFF); return;
            }
            if (lower == "random") {
                if (lex.at(T_NUMBER)) { cg.emit_push(lex.cur().ival); lex.eat(); }
                else if (lex.at(T_VAR)) { cg.emit(0x06); cg.emit((uint8_t)lex.cur().ival); lex.eat(); }
                else { err("expected number or variable after 'random'"); cg.emit_push(1); }
                cg.emit(0x1D); return;
            }
            if (lower == "percent") {
                if (lex.at(T_NUMBER)) { cg.emit_push(lex.cur().ival); lex.eat(); }
                else if (lex.at(T_VAR)) { cg.emit(0x06); cg.emit((uint8_t)lex.cur().ival); lex.eat(); }
                else { err("expected number or variable after 'percent'"); cg.emit_push(0); }
                cg.emit(0x1E); return;
            }
            if (lower == "chance") {
                if (lex.at(T_NUMBER)) { cg.emit_push(lex.cur().ival); lex.eat(); }
                else if (lex.at(T_VAR)) { cg.emit(0x06); cg.emit((uint8_t)lex.cur().ival); lex.eat(); }
                else { err("expected number or variable after 'chance'"); cg.emit_push(0); }
                cg.emit(0x1F); return;
            }
            // All other identifiers are attribute names → _CTEval_<lowercase>
            (void)src_line;
            cg.emit_call_by_name("_CTEval_" + lower);
            return;
        }
        if (t.type == T_MINUS) {
            lex.eat(); parse_primary(); cg.emit(0x1B); return; // NEG
        }
        // String literal (used by maneuver instruction, handled above at instruction level)
        if (t.type == T_STRING) {
            cg.emit_push_addr(t.str); lex.eat(); return;
        }
        err("unexpected token in expression: '" + t.str + "'");
        lex.eat();
    }

    void parse_unary() {
        if (lex.at(T_IDENT)) {
            std::string lower = to_lower(lex.cur().str);
            if (lower == "not") { lex.eat(); parse_unary(); cg.emit(0x1C); return; }
            if (lower == "abs") { lex.eat(); parse_unary(); cg.emit(0x1A); return; }
            if (lower == "neg") { lex.eat(); parse_unary(); cg.emit(0x1B); return; }
        }
        if (lex.at(T_MINUS)) {
            lex.eat(); parse_unary(); cg.emit(0x1B); return;
        }
        parse_primary();
    }

    void parse_mul() {
        parse_unary();
        for (;;) {
            if      (lex.at(T_STAR))    { lex.eat(); parse_unary(); cg.emit(0x08); }
            else if (lex.at(T_SLASH))   { lex.eat(); parse_unary(); cg.emit(0x09); }
            else if (lex.at(T_PERCENT)) { lex.eat(); parse_unary(); cg.emit(0x0A); }
            else break;
        }
    }

    // arg_mode=true: treat '-' as binary only if the token after it was preceded by space
    // (i.e., '- 10' is binary subtraction; '-10' without space starts a new argument)
    void parse_add(bool arg_mode = false) {
        parse_mul();
        for (;;) {
            if (lex.at(T_PLUS)) {
                lex.eat(); parse_mul(); cg.emit(0x0B);
            } else if (lex.at(T_MINUS)) {
                if (arg_mode && !lex.peek(1).preceded_by_space) break;
                lex.eat(); parse_mul(); cg.emit(0x0C);
            } else break;
        }
    }

    void parse_cmp() {
        parse_add();
        uint8_t op = 0;
        if      (lex.at(T_LT))    { op = 0x12; lex.eat(); }
        else if (lex.at(T_LE))    { op = 0x13; lex.eat(); }
        else if (lex.at(T_GE))    { op = 0x14; lex.eat(); }
        else if (lex.at(T_GT))    { op = 0x15; lex.eat(); }
        else if (lex.at(T_EQEQ))  { op = 0x16; lex.eat(); }
        else if (lex.at(T_NE))    { op = 0x17; lex.eat(); }
        if (op) { parse_add(); cg.emit(op); }
    }

    void parse_and() {
        parse_cmp();
        while (lex.at(T_ANDAND)) {
            lex.eat(); parse_cmp(); cg.emit(0x18); // LAND
        }
    }

    void parse_expr() {
        parse_and();
        while (lex.at(T_OROR)) {
            lex.eat(); parse_and(); cg.emit(0x19); // LOR
        }
    }

    // ---- Statement parsers --------------------------------------------------

    // Returns true if the current position looks like the start of a statement
    // (used to detect "if <cond> <stmt>" vs "if <cond> goto <label>")
    bool is_stmt_start() {
        if (lex.at(T_VAR)) return true;
        if (lex.at(T_DOT)) return true;
        if (lex.at(T_HASH)) return true;
        if (lex.at(T_IDENT)) {
            std::string lower = to_lower(lex.cur().str);
            if (lower == "goto"  || lower == "exit"   || lower == "restart" ||
                lower == "if"    || lower == "switch")
                return true;
            if (find_instr(lower)) return true;
        }
        return false;
    }

    // Check if next tokens are ".else" or ".endif"
    bool is_dot_keyword(const char* kw) {
        if (!lex.at(T_DOT)) return false;
        if (lex.peek(1).type != T_IDENT) return false;
        return to_lower(lex.peek(1).str) == kw;
    }

    // Consume ".keyword"
    void consume_dot_keyword() {
        lex.eat(); // T_DOT
        lex.eat(); // T_IDENT
    }

    // Parse the body of a .if or .else block.
    // Stops (but does NOT consume) when it sees .else or .endif.
    // Returns true if stopped at .else, false if .endif or EOF.
    bool parse_block_body() {
        while (true) {
            lex.skip_eols();
            if (lex.at(T_EOF)) return false;
            if (is_dot_keyword("else"))  return true;
            if (is_dot_keyword("endif")) return false;
            parse_statement(true);
            lex.eat_if(T_EOL);
        }
    }

    // Parse a .if/.else/.endif block.  We have just consumed ".if".
    void parse_block_if(int src_line) {
        cg.emit_frame(src_line);
        parse_expr();
        uint32_t if_false_off = cg.emit_if_false_placeholder();
        lex.eat_if(T_EOL);

        bool has_else = parse_block_body();

        if (has_else) {
            // Emit GOTO to skip the else body, then start else
            uint32_t goto_off;
            {
                cg.emit(0x20);
                goto_off = cg.offset();
                cg.emit16(0); // placeholder
            }
            cg.patch_to_here(if_false_off);
            consume_dot_keyword(); // .else
            lex.eat_if(T_EOL);
            parse_block_body(); // parse else body
            cg.patch_to_here(goto_off);
            if (is_dot_keyword("endif")) consume_dot_keyword();
        } else {
            cg.patch_to_here(if_false_off);
            if (is_dot_keyword("endif")) consume_dot_keyword();
        }
        lex.eat_if(T_EOL);
    }

    // Parse an inline `if <expr> goto <label>` or `if <expr> <stmt>`.
    // We have just consumed "if".
    void parse_inline_if(int src_line) {
        parse_expr(); // condition — leaves bool on stack

        lex.eat_if(T_COMMA); // optional comma separator before goto or statement

        // Peek: is next token "goto"?
        if (lex.at(T_IDENT) && to_lower(lex.cur().str) == "goto") {
            lex.eat();
            if (!lex.at(T_IDENT)) { err("expected label after 'goto'"); return; }
            std::string label = to_lower(lex.cur().str); lex.eat();

            cg.emit_frame(src_line);
            uint32_t if_false_off = cg.emit_if_false_placeholder();
            cg.emit_goto_label(label);
            cg.patch_to_here(if_false_off); // if false: skip GOTO
            return;
        }

        // Inline if with statement: `if <cond> <stmt>`
        cg.emit_frame(src_line);
        uint32_t if_false_off = cg.emit_if_false_placeholder();
        parse_statement(false); // inner stmt — no FRAME (already emitted above)
        cg.patch_to_here(if_false_off);
    }

    // Parse a switch: we have just consumed "switch".
    void parse_switch(int src_line) {
        // Syntax: switch random N <label1> ... <labelN>
        if (!lex.at(T_IDENT) || to_lower(lex.cur().str) != "random") {
            err("expected 'random' after 'switch'"); return;
        }
        lex.eat();
        if (!lex.at(T_NUMBER)) { err("expected count after 'switch random'"); return; }
        int n = lex.cur().ival; lex.eat();

        // Read labels until end of line. N is the random pool size (for weighting);
        // fewer labels than N means the remaining cases fall through.
        std::vector<std::string> labels;
        while (!lex.at(T_EOL) && !lex.at(T_EOF)) {
            if (!lex.at(T_IDENT)) { err("unexpected token in switch"); break; }
            labels.push_back(to_lower(lex.cur().str)); lex.eat();
        }

        cg.emit_frame(src_line);
        // PUSH N, RANDOM → yields random 0..N-1
        cg.emit_push((int32_t)n);
        cg.emit(0x1D); // RANDOM
        // SWITCH opcode: 0x24, label-count byte, then count × s16 targets
        cg.emit(0x24);
        cg.emit((uint8_t)labels.size());
        for (const auto& lbl : labels) {
            uint32_t off = cg.offset();
            cg.patches.push_back({off, lbl});
            cg.emit16(0);
        }
    }

    // Parse one instruction call: name args... CALL_BY_NAME
    void parse_instruction(const InstrDef& def, int src_line) {
        cg.emit_frame(src_line);

        if (def.num_args == 0) {
            cg.emit_call_by_name(std::string("_CTDo_") + def.name);
            cg.emit(0x04); // EVAL — discard return value
            return;
        }

        // Parse all args into a temporary vector of emit lambdas.
        // We need to push in REVERSE order (last arg first).
        // Strategy: record the bytecode offset before each arg, emit all in order,
        // then reverse the arg chunks in the bytecode buffer.
        if (def.string_arg) {
            // Single string arg: PUSH_ADDR "<string>"
            if (lex.at(T_STRING)) { cg.emit_push_addr(lex.cur().str); lex.eat(); }
            else { err("expected string for " + std::string(def.name)); cg.emit_push(0); }
            cg.emit_call_by_name(std::string("_CTDo_") + def.name);
            cg.emit(0x04);
            return;
        }

        // Parse N args, collecting start offsets so we can reverse their order.
        std::vector<uint32_t> arg_starts;
        arg_starts.reserve(def.num_args);
        for (int i = 0; i < def.num_args; ++i) {
            if (lex.at(T_EOL) || lex.at(T_EOF)) {
                err("too few arguments for instruction '" + std::string(def.name) + "'");
                break;
            }
            arg_starts.push_back(cg.offset());
            parse_add(true); // arg mode: '-' not binary if next token is adjacent
        }

        // Reverse the argument chunks in the bytecode so last arg is first on stack.
        if (arg_starts.size() >= 2) {
            // Extract each arg's bytes, rebuild in reverse order
            std::vector<std::vector<uint8_t>> chunks;
            for (size_t i = 0; i < arg_starts.size(); ++i) {
                uint32_t from = arg_starts[i];
                uint32_t to   = (i + 1 < arg_starts.size()) ? arg_starts[i+1] : cg.offset();
                chunks.push_back(std::vector<uint8_t>(cg.code.begin() + from,
                                                       cg.code.begin() + to));
            }
            // Rebuild code from arg_starts[0] in reversed order
            uint32_t base = arg_starts[0];
            cg.code.resize(base);
            for (int i = (int)chunks.size()-1; i >= 0; --i) {
                for (uint8_t b : chunks[i]) cg.code.push_back(b);
            }
            // Patches within arg regions need their offsets adjusted
            // (backpatch entries are rare inside args; they happen for label refs in exprs,
            //  but expressions in args don't contain GOTO/IF_FALSE, so no patches to fix)
        }

        cg.emit_call_by_name(std::string("_CTDo_") + def.name);
        cg.emit(0x04); // EVAL
    }

    // Parse one statement.  emit_frame = true for top-level statements; false for
    // the body of `if <cond> <stmt>` (FRAME was already emitted by parse_inline_if).
    void parse_statement(bool emit_frame) {
        lex.skip_eols();
        if (lex.at(T_EOF)) return;

        // #DEBUG ... → skip rest of line
        if (lex.at(T_HASH)) { lex.skip_line(); return; }

        // .if block
        if (lex.at(T_DOT)) {
            int src_line = lex.cur().line;
            lex.eat(); // consume '.'
            if (!lex.at(T_IDENT)) { err("expected 'if' after '.'"); lex.skip_line(); return; }
            std::string kw = to_lower(lex.cur().str); lex.eat();
            if (kw == "if") { parse_block_if(src_line); return; }
            err("unexpected '." + kw + "'"); lex.skip_line(); return;
        }

        if (!lex.at(T_IDENT) && !lex.at(T_VAR)) {
            err("expected statement"); lex.skip_line(); return;
        }

        // %var = expr  (assignment)
        if (lex.at(T_VAR)) {
            int var = lex.cur().ival;
            int src_line = lex.cur().line;
            lex.eat();
            if (!lex.at(T_EQ)) { err("expected '=' after variable"); lex.skip_line(); return; }
            lex.eat();
            if (emit_frame) cg.emit_frame(src_line);
            parse_expr();
            cg.emit(0x05); cg.emit((uint8_t)var); // STORE_VAR
            return;
        }

        // IDENT: keyword or instruction
        std::string ident = lex.cur().str;
        std::string lower = to_lower(ident);
        int src_line = lex.cur().line;

        // Label definition: IDENT ':'
        if (lex.peek(1).type == T_COLON) {
            cg.labels[lower] = cg.offset();
            lex.eat(); lex.eat(); // IDENT + COLON
            // Statement may follow on the same line
            if (!lex.at(T_EOL) && !lex.at(T_EOF))
                parse_statement(true);
            return;
        }

        lex.eat(); // consume the IDENT

        if (lower == "if") {
            parse_inline_if(src_line); return;
        }
        if (lower == "goto") {
            if (emit_frame) cg.emit_frame(src_line);
            if (!lex.at(T_IDENT)) { err("expected label after 'goto'"); return; }
            std::string lbl = to_lower(lex.cur().str); lex.eat();
            cg.emit_goto_label(lbl); return;
        }
        if (lower == "exit") {
            if (emit_frame) cg.emit_frame(src_line);
            cg.emit_call_by_name("_CTDo_exit");
            cg.emit(0x04); return;
        }
        if (lower == "restart") {
            if (emit_frame) cg.emit_frame(src_line);
            cg.emit_goto_abs(0); return;
        }
        if (lower == "switch") {
            parse_switch(src_line); return;
        }

        // Instruction call
        const InstrDef* def = find_instr(lower);
        if (def) {
            parse_instruction(*def, src_line);
            return;
        }

        err("unknown statement: '" + ident + "'");
        lex.skip_line();
    }

    // Top-level parse loop
    void parse_all() {
        lex.skip_eols();
        while (!lex.at(T_EOF)) {
            parse_statement(true);
            lex.eat_if(T_EOL);
            lex.skip_eols();
        }
    }
};

// ---- PE builder -------------------------------------------------------------

// Fixed MZ stub (bytes 0x000–0x07F), taken from LARGE.BI.
static const uint8_t kMzStub[128] = {
    0x4D,0x5A,0x80,0x00,0x01,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0xFF,0xFF,0x00,0x00,
    0xB8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x00,0x00,0x00, // 0x3C = PE offset
    0x0E,0x1F,0xBA,0x0E,0x00,0xB4,0x09,0xCD,0x21,0xB8,0x01,0x4C,0xCD,0x21,0x54,0x68,
    0x69,0x73,0x20,0x70,0x72,0x6F,0x67,0x72,0x61,0x6D,0x20,0x63,0x61,0x6E,0x6E,0x6F,
    0x74,0x20,0x62,0x65,0x20,0x72,0x75,0x6E,0x20,0x69,0x6E,0x20,0x44,0x4F,0x53,0x20,
    0x6D,0x6F,0x64,0x65,0x2E,0x0D,0x0D,0x0A,0x24,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

static void write_u16le(uint8_t* p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void write_u32le(uint8_t* p, uint32_t v) {
    p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}

static std::vector<uint8_t> build_pe(const std::vector<uint8_t>& bytecode) {
    // Section layout (matches LARGE.BI):
    //   0x000: MZ stub (128 B)
    //   0x080: Phar Lap PE header ("PL\0\0") + COFF + optional header + section table
    //   0x400: CODE raw data (padded to 0x200 boundary)
    //   0x?00: .idata raw data (0x200 B, zeros)
    //   0x?00: .reloc raw data (0 B)
    //   0x?00: $$DOSX raw data (0x200 B, zeros)

    auto align_up = [](uint32_t v, uint32_t a) { return (v + a - 1) & ~(a - 1); };

    uint32_t code_sz     = (uint32_t)bytecode.size() + 1; // +1 for '%' (END) terminator
    uint32_t code_raw_sz = align_up(code_sz, 0x200);

    uint32_t code_raw_ptr  = 0x400;
    uint32_t idata_raw_ptr = code_raw_ptr + code_raw_sz;
    uint32_t idata_raw_sz  = 0x200;
    uint32_t reloc_raw_ptr = idata_raw_ptr + idata_raw_sz;
    uint32_t reloc_raw_sz  = 0;
    uint32_t dosx_raw_ptr  = reloc_raw_ptr + reloc_raw_sz;
    uint32_t dosx_raw_sz   = 0x200;
    uint32_t file_sz       = dosx_raw_ptr + dosx_raw_sz;

    std::vector<uint8_t> out(file_sz, 0);

    // MZ stub
    memcpy(out.data(), kMzStub, 128);

    // --- Phar Lap PE header at 0x80 ---
    uint8_t* pe = out.data() + 0x80;

    // PE signature "PL\0\0"
    pe[0]='P'; pe[1]='L'; pe[2]=0; pe[3]=0;

    // COFF header (20 bytes at +4)
    write_u16le(pe+4,  0x014C);   // Machine = i386
    write_u16le(pe+6,  4);         // NumberOfSections = 4
    write_u32le(pe+8,  0);         // TimeDateStamp = 0
    write_u32le(pe+12, 0);         // PointerToSymbolTable
    write_u32le(pe+16, 0);         // NumberOfSymbols
    write_u16le(pe+20, 0x00E0);    // SizeOfOptionalHeader = 224
    write_u16le(pe+22, 0xA18E);    // Characteristics (DLL | 32bit | executable | ...)

    // Optional header (PE32, 224 bytes at +24 = 0x98 in file)
    uint8_t* opt = pe + 24;
    write_u16le(opt+0,  0x010B);          // Magic = PE32
    opt[2] = 2; opt[3] = 23;              // Linker version (from LARGE.BI)
    write_u32le(opt+4,  code_raw_sz);     // SizeOfCode
    write_u32le(opt+8,  0x1800);          // SizeOfInitializedData (from LARGE.BI)
    write_u32le(opt+12, 0);               // SizeOfUninitializedData
    write_u32le(opt+16, 0);               // AddressOfEntryPoint = 0
    write_u32le(opt+20, 0x1000);          // BaseOfCode
    write_u32le(opt+24, 0x2000);          // BaseOfData
    write_u32le(opt+28, 0x00010000);      // ImageBase
    write_u32le(opt+32, 0x1000);          // SectionAlignment
    write_u32le(opt+36, 0x0200);          // FileAlignment
    write_u16le(opt+40, 1); write_u16le(opt+42, 0);  // OS version
    write_u16le(opt+44, 0); write_u16le(opt+46, 0);  // Image version
    write_u16le(opt+48, 4); write_u16le(opt+50, 0);  // Subsystem version (from LARGE.BI)
    write_u32le(opt+52, 0);               // Win32VersionValue
    write_u32le(opt+56, 0x5000);          // SizeOfImage (4 sections × 0x1000 + base 0x1000)
    write_u32le(opt+60, 0x0400);          // SizeOfHeaders
    write_u32le(opt+64, 0);               // CheckSum
    write_u16le(opt+68, 0x0002);          // Subsystem = WINDOWS_GUI (from LARGE.BI)
    write_u16le(opt+70, 0x0000);          // DllCharacteristics
    write_u32le(opt+72, 0x00100000);      // SizeOfStackReserve
    write_u32le(opt+76, 0x00001000);      // SizeOfStackCommit
    write_u32le(opt+80, 0x00100000);      // SizeOfHeapReserve
    write_u32le(opt+84, 0x00001000);      // SizeOfHeapCommit
    write_u32le(opt+88, 0);               // LoaderFlags
    write_u32le(opt+92, 16);              // NumberOfRvaAndSizes
    // Data directories (16 × 8 bytes = 128 bytes, all zero except import descriptor)
    // Import table: VA=.idata VMA=0x2000, size=0 (no real imports — CALL_BY_NAME)
    write_u32le(opt+96+8,  0x2000);  // [1] = import descriptor VA
    write_u32le(opt+96+12, 0);       // [1] = import descriptor size (0 = none)
    // All other data directories remain zero.

    // Section table: 4 entries × 40 bytes at offset 0x98 in file = 0x178
    uint8_t* sec = pe + 24 + 0xE0; // = 0x080 + 4 + 20 + 224 = offset 0x178 in file

    // Helper: write one section table entry
    auto write_sec = [&](uint8_t* s, const char* name,
                         uint32_t virt_sz, uint32_t vma,
                         uint32_t raw_sz2, uint32_t raw_ptr2,
                         uint32_t chars)
    {
        memset(s, 0, 40);
        memcpy(s, name, strlen(name));
        write_u32le(s+8,  virt_sz);
        write_u32le(s+12, vma);
        write_u32le(s+16, raw_sz2);
        write_u32le(s+20, raw_ptr2);
        write_u32le(s+36, chars);
    };

    write_sec(sec+0,  "CODE",   code_sz,  0x1000, code_raw_sz,  code_raw_ptr,  0xE0000020u);
    write_sec(sec+40, ".idata", 0,        0x2000, idata_raw_sz, idata_raw_ptr, 0xC0000040u);
    write_sec(sec+80, ".reloc", 0,        0x3000, reloc_raw_sz, reloc_raw_ptr, 0x42000040u);
    write_sec(sec+120,"$$DOSX", 0x200,    0x4000, dosx_raw_sz,  dosx_raw_ptr,  0x42000042u);

    // CODE data: bytecode + '%' END terminator
    {
        uint8_t* dst = out.data() + code_raw_ptr;
        memcpy(dst, bytecode.data(), bytecode.size());
        dst[bytecode.size()] = 0x25; // '%' = END
    }

    // .idata, .reloc, $$DOSX are left as zeros (already zero-initialized)

    return out;
}

// ---- Entry point ------------------------------------------------------------

std::vector<uint8_t> ai_compile(const std::string& source,
                                std::vector<AiCompileError>& errors) {
    Lexer lex;
    lex.src  = source.c_str();
    lex.len  = source.size();
    lex.pos  = 0;
    lex.line = 1;
    lex.ti   = 0;
    lex.tokenize();

    Codegen  cg(errors);
    Parser   parser(lex, cg, errors);
    parser.parse_all();

    if (!cg.resolve_patches()) return {};
    if (!errors.empty()) return {};

    return build_pe(cg.code);
}

} // namespace ft
