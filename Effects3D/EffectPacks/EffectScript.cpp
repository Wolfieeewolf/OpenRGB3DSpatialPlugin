// SPDX-License-Identifier: GPL-2.0-only

#include "EffectScript.h"
#include "EffectPackDetail.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <mutex>
#include <unordered_map>

namespace EffectPack
{
namespace script
{
namespace
{

using detail::HashLed;
using detail::LerpColor;

enum Op
{
    PushNum,
    PushVar,
    Store,
    Add,
    Sub,
    Mul,
    Div,
    Neg,
    Cmp,
    And,
    Or,
    Not,
    Call,
    Jump,
    JumpIfFalse,
    Paint,
    PaintMix,
    Off,
    Halt,
};

struct Ins
{
    Op op = Halt;
    int a = 0;
    double n = 0.0;
};

struct Program
{
    std::vector<Ins> code;
    std::vector<std::string> names;
    std::string title;
    std::string description;
    std::string section;
    int swatch_r = 180;
    int swatch_g = 180;
    int swatch_b = 190;
    bool world = false;
    bool color_ends = false;
    bool preview_pulse = false;
    bool axis_angle = false;
    std::vector<std::string> aliases;
};

std::unordered_map<std::string, Program> g_programs;
std::unordered_map<std::string, std::string> g_alias;
filesystem::path g_dir;
std::mutex g_mu;

float Hash01(unsigned int h)
{
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return (float)(h & 0xFFFFFFu) / (float)0xFFFFFFu;
}

float ValueNoise3(float x, float y, float z)
{
    const int ix = (int)std::floor(x);
    const int iy = (int)std::floor(y);
    const int iz = (int)std::floor(z);
    const float fx = x - (float)ix;
    const float fy = y - (float)iy;
    const float fz = z - (float)iz;
    auto corner = [](int cx, int cy, int cz) {
        return Hash01((unsigned int)(cx * 73856093 ^ cy * 19349663 ^ cz * 83492791));
    };
    const float sx = fx * fx * (3.0f - 2.0f * fx);
    const float sy = fy * fy * (3.0f - 2.0f * fy);
    const float sz = fz * fz * (3.0f - 2.0f * fz);
    const float c000 = corner(ix, iy, iz);
    const float c100 = corner(ix + 1, iy, iz);
    const float c010 = corner(ix, iy + 1, iz);
    const float c110 = corner(ix + 1, iy + 1, iz);
    const float c001 = corner(ix, iy, iz + 1);
    const float c101 = corner(ix + 1, iy, iz + 1);
    const float c011 = corner(ix, iy + 1, iz + 1);
    const float c111 = corner(ix + 1, iy + 1, iz + 1);
    const float x00 = c000 + (c100 - c000) * sx;
    const float x10 = c010 + (c110 - c010) * sx;
    const float x01 = c001 + (c101 - c001) * sx;
    const float x11 = c011 + (c111 - c011) * sx;
    const float y0 = x00 + (x10 - x00) * sy;
    const float y1 = x01 + (x11 - x01) * sy;
    return y0 + (y1 - y0) * sz;
}

enum Fn
{
    FnSin,
    FnCos,
    FnAbs,
    FnMin,
    FnMax,
    FnClamp,
    FnFloor,
    FnSqrt,
    FnAtan2,
    FnFmod,
    FnFract,
    FnBand,
    FnShr,
    FnXor,
    FnHash,
    FnHashPhase,
    FnHashByte,
    FnHash01,
    FnHash01Mix,
    FnNoise,
    FnAxisX,
    FnAxisY,
    FnAxisZ,
    FnSampleAxis,
};

struct Tok
{
    enum Kind { Num, Id, Sym, End } kind = End;
    double n = 0.0;
    std::string text;
};

struct Parser
{
    std::vector<Tok> toks;
    size_t i = 0;
    Program* prog = nullptr;
    std::unordered_map<std::string, int> vars;
    bool ok = true;

    const Tok& cur() const { return toks[std::min(i, toks.size() - 1)]; }
    void next() { if(i + 1 < toks.size()) { ++i; } }

    int varIndex(const std::string& name)
    {
        auto it = vars.find(name);
        if(it != vars.end())
        {
            return it->second;
        }
        const int idx = (int)prog->names.size();
        prog->names.push_back(name);
        vars[name] = idx;
        return idx;
    }

    void emit(Op op, int a = 0, double n = 0.0)
    {
        prog->code.push_back({op, a, n});
    }

    bool acceptSym(const char* s)
    {
        if(cur().kind == Tok::Sym && cur().text == s)
        {
            next();
            return true;
        }
        return false;
    }

    bool acceptId(const char* s)
    {
        if(cur().kind == Tok::Id && cur().text == s)
        {
            next();
            return true;
        }
        return false;
    }

    void parseExpr();
    void parseOr();
    void parseAnd();
    void parseCmp();
    void parseAdd();
    void parseMul();
    void parseUnary();
    void parsePrimary();
    void parseStmt();
    void parseBlock();
    bool atStop() const;
};

int FnId(const std::string& name)
{
    if(name == "sin") return FnSin;
    if(name == "cos") return FnCos;
    if(name == "abs") return FnAbs;
    if(name == "min") return FnMin;
    if(name == "max") return FnMax;
    if(name == "clamp") return FnClamp;
    if(name == "floor") return FnFloor;
    if(name == "sqrt") return FnSqrt;
    if(name == "atan2") return FnAtan2;
    if(name == "fmod") return FnFmod;
    if(name == "fract") return FnFract;
    if(name == "band") return FnBand;
    if(name == "shr") return FnShr;
    if(name == "xor") return FnXor;
    if(name == "hash") return FnHash;
    if(name == "hash_phase") return FnHashPhase;
    if(name == "hash_byte") return FnHashByte;
    if(name == "hash01") return FnHash01;
    if(name == "hash01_mix") return FnHash01Mix;
    if(name == "noise") return FnNoise;
    if(name == "axis_x") return FnAxisX;
    if(name == "axis_y") return FnAxisY;
    if(name == "axis_z") return FnAxisZ;
    if(name == "sample_axis") return FnSampleAxis;
    return -1;
}

void Parser::parseExpr() { parseOr(); }

void Parser::parseOr()
{
    parseAnd();
    while(acceptSym("||"))
    {
        parseAnd();
        emit(Or);
    }
}

void Parser::parseAnd()
{
    parseCmp();
    while(acceptSym("&&"))
    {
        parseCmp();
        emit(And);
    }
}

void Parser::parseCmp()
{
    parseAdd();
    const char* ops[] = {"<=", ">=", "==", "!=", "<", ">"};
    const int codes[] = {0, 1, 2, 3, 4, 5};
    for(int k = 0; k < 6; ++k)
    {
        if(acceptSym(ops[k]))
        {
            parseAdd();
            emit(Cmp, codes[k]);
            return;
        }
    }
}

void Parser::parseAdd()
{
    parseMul();
    while(cur().kind == Tok::Sym && (cur().text == "+" || cur().text == "-"))
    {
        const bool sub = cur().text == "-";
        next();
        parseMul();
        emit(sub ? Sub : Add);
    }
}

void Parser::parseMul()
{
    parseUnary();
    while(cur().kind == Tok::Sym && (cur().text == "*" || cur().text == "/"))
    {
        const bool div = cur().text == "/";
        next();
        parseUnary();
        emit(div ? Div : Mul);
    }
}

void Parser::parseUnary()
{
    if(acceptSym("-"))
    {
        parseUnary();
        emit(Neg);
        return;
    }
    if(acceptSym("!"))
    {
        parseUnary();
        emit(Not);
        return;
    }
    parsePrimary();
}

void Parser::parsePrimary()
{
    if(cur().kind == Tok::Num)
    {
        emit(PushNum, 0, cur().n);
        next();
        return;
    }
    if(acceptSym("("))
    {
        parseExpr();
        acceptSym(")");
        return;
    }
    if(cur().kind != Tok::Id)
    {
        ok = false;
        emit(PushNum, 0, 0.0f);
        return;
    }
    const std::string name = cur().text;
    next();
    if(acceptSym("("))
    {
        if(name == "paint")
        {
            parseExpr();
            acceptSym(")");
            emit(Paint);
            emit(PushNum, 0, 0.0f);
            return;
        }
        if(name == "paint_mix")
        {
            parseExpr();
            acceptSym(",");
            parseExpr();
            acceptSym(",");
            parseExpr();
            acceptSym(")");
            emit(PaintMix);
            emit(PushNum, 0, 0.0f);
            return;
        }
        const int fn = FnId(name);
        int argc = 0;
        if(!acceptSym(")"))
        {
            parseExpr();
            ++argc;
            while(acceptSym(","))
            {
                parseExpr();
                ++argc;
            }
            acceptSym(")");
        }
        if(fn < 0)
        {
            ok = false;
        }
        emit(Call, fn, (float)argc);
        return;
    }
    emit(PushVar, varIndex(name));
}

bool Parser::atStop() const
{
    return cur().kind == Tok::End
        || (cur().kind == Tok::Id && (cur().text == "end" || cur().text == "else" || cur().text == "elif"));
}

void Parser::parseBlock()
{
    while(ok && !atStop())
    {
        parseStmt();
    }
}

void Parser::parseStmt()
{
    if(acceptId("off"))
    {
        emit(Off);
        return;
    }
    if(acceptId("if"))
    {
        parseExpr();
        const int jump_false = (int)prog->code.size();
        emit(JumpIfFalse, -1);
        parseBlock();
        if(acceptId("else"))
        {
            const int jump_end = (int)prog->code.size();
            emit(Jump, -1);
            prog->code[(size_t)jump_false].a = (int)prog->code.size();
            parseBlock();
            prog->code[(size_t)jump_end].a = (int)prog->code.size();
        }
        else
        {
            prog->code[(size_t)jump_false].a = (int)prog->code.size();
        }
        acceptId("end");
        return;
    }
    if(acceptId("while"))
    {
        const int loop = (int)prog->code.size();
        parseExpr();
        const int jump_false = (int)prog->code.size();
        emit(JumpIfFalse, -1);
        parseBlock();
        emit(Jump, loop);
        prog->code[(size_t)jump_false].a = (int)prog->code.size();
        acceptId("end");
        return;
    }
    if(cur().kind == Tok::Id)
    {
        const size_t ahead = std::min(i + 1, toks.size() - 1);
        if(toks[ahead].kind == Tok::Sym && toks[ahead].text == "(")
        {
            parsePrimary();
            return;
        }
        const std::string name = cur().text;
        next();
        if(!acceptSym("="))
        {
            ok = false;
            return;
        }
        parseExpr();
        emit(Store, varIndex(name));
        return;
    }
    ok = false;
    next();
}

std::vector<Tok> Tokenize(const std::string& body)
{
    std::vector<Tok> out;
    size_t i = 0;
    auto push_sym = [&](const std::string& s) {
        Tok t;
        t.kind = Tok::Sym;
        t.text = s;
        out.push_back(t);
    };
    while(i < body.size())
    {
        const char c = body[i];
        if(c == ' ' || c == '\t' || c == '\r' || c == '\n')
        {
            ++i;
            continue;
        }
        if(c == '#')
        {
            while(i < body.size() && body[i] != '\n')
            {
                ++i;
            }
            continue;
        }
        if((c >= '0' && c <= '9') || (c == '.' && i + 1 < body.size() && body[i + 1] >= '0' && body[i + 1] <= '9'))
        {
            size_t j = i;
            while(j < body.size() && ((body[j] >= '0' && body[j] <= '9') || body[j] == '.'))
            {
                ++j;
            }
            Tok t;
            t.kind = Tok::Num;
            t.n = std::strtod(body.c_str() + i, nullptr);
            out.push_back(t);
            i = j;
            continue;
        }
        if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')
        {
            size_t j = i;
            while(j < body.size())
            {
                const char d = body[j];
                if((d >= 'a' && d <= 'z') || (d >= 'A' && d <= 'Z') || (d >= '0' && d <= '9') || d == '_')
                {
                    ++j;
                    continue;
                }
                break;
            }
            Tok t;
            t.kind = Tok::Id;
            t.text = body.substr(i, j - i);
            out.push_back(t);
            i = j;
            continue;
        }
        if(i + 1 < body.size())
        {
            const std::string two = body.substr(i, 2);
            if(two == "<=" || two == ">=" || two == "==" || two == "!=" || two == "&&" || two == "||")
            {
                push_sym(two);
                i += 2;
                continue;
            }
        }
        push_sym(std::string(1, c));
        ++i;
    }
    Tok end;
    end.kind = Tok::End;
    out.push_back(end);
    return out;
}

bool Compile(const std::string& body, Program* prog)
{
    Parser p;
    p.toks = Tokenize(body);
    p.prog = prog;
    const char* reserved[] = {
        "progress", "axis", "time_ms", "start_ms", "speed", "period_ms", "intensity",
        "min_intensity", "max_intensity", "pulse", "seed", "x", "y", "z",
        "nx", "ny", "nz", "radius", "height", "span_x", "span_y", "span_z",
        "dx", "dy", "dz", "invert", "local_ms",
    };
    for(const char* name : reserved)
    {
        p.varIndex(name);
    }
    while(p.ok && p.cur().kind != Tok::End)
    {
        p.parseStmt();
    }
    p.emit(Halt);
    return p.ok;
}

struct Machine
{
    const Program* prog = nullptr;
    LedView* led = nullptr;
    std::vector<double> vars;
    std::vector<double> stack;
    RGBColor color = 0;
    bool off = false;

    double pop()
    {
        if(stack.empty())
        {
            return 0.0f;
        }
        const double v = stack.back();
        stack.pop_back();
        return v;
    }

    void push(double v) { stack.push_back(v); }

    double call(int fn, int argc)
    {
        std::vector<double> args(argc);
        for(int i = argc - 1; i >= 0; --i)
        {
            args[(size_t)i] = pop();
        }
        auto a = [&](int i) -> double { return i < argc ? args[(size_t)i] : 0.0; };
        switch(fn)
        {
            case FnSin: return std::sin(a(0));
            case FnCos: return std::cos(a(0));
            case FnAbs: return std::fabs(a(0));
            case FnMin: return std::min(a(0), a(1));
            case FnMax: return std::max(a(0), a(1));
            case FnClamp: return std::clamp(a(0), a(1), a(2));
            case FnFloor: return std::floor(a(0));
            case FnSqrt: return std::sqrt(std::max(0.0, a(0)));
            case FnAtan2: return std::atan2(a(0), a(1));
            case FnFmod:
            {
                const double m = a(1);
                return (m == 0.0) ? 0.0 : std::fmod(a(0), m);
            }
            case FnFract: return a(0) - std::floor(a(0));
            case FnBand: return (float)((unsigned int)a(0) & (unsigned int)a(1));
            case FnShr: return (float)((unsigned int)a(0) >> (unsigned int)a(1));
            case FnXor: return (float)((unsigned int)a(0) ^ (unsigned int)a(1));
            case FnHash: return (float)HashLed((int)a(0), (int)a(1), (int)a(2));
            case FnHashPhase: return (float)(HashLed((int)a(0), (int)a(1), (int)a(2)) & 0xFFFFu) / 65535.0f;
            case FnHashByte: return (float)((HashLed((int)a(0), (int)a(1), (int)a(2)) >> (unsigned)a(3)) & 0xFFu) / 255.0f;
            case FnHash01: return Hash01((unsigned int)a(0));
            case FnHash01Mix: return Hash01((unsigned int)a(0) * (unsigned int)a(1) + (unsigned int)a(2));
            case FnNoise: return ValueNoise3((float)a(0), (float)a(1), (float)a(2));
            case FnAxisX:
            case FnAxisY:
            case FnAxisZ:
            {
                float ux = 0.0f, uy = 0.0f, uz = 0.0f;
                if(led && led->block)
                {
                    AxisUnitVector(*led->block, &ux, &uy, &uz);
                }
                if(fn == FnAxisX) return ux;
                if(fn == FnAxisY) return uy;
                return uz;
            }
            case FnSampleAxis:
                if(!led || !led->block)
                {
                    return 0.0f;
                }
                return SampleAxisPos(*led->block, led->x, led->y, led->z,
                                     led->min_x, led->max_x, led->min_y, led->max_y, led->min_z, led->max_z);
            default:
                return 0.0f;
        }
    }

    bool run()
    {
        size_t ip = 0;
        int guard = 0;
        while(ip < prog->code.size() && guard < 100000)
        {
            ++guard;
            const Ins& ins = prog->code[ip];
            switch(ins.op)
            {
                case PushNum: push(ins.n); ++ip; break;
                case PushVar: push(ins.a >= 0 && ins.a < (int)vars.size() ? vars[(size_t)ins.a] : 0.0f); ++ip; break;
                case Store:
                    if(ins.a >= 0 && ins.a < (int)vars.size())
                    {
                        vars[(size_t)ins.a] = pop();
                    }
                    else
                    {
                        pop();
                    }
                    ++ip;
                    break;
                case Add: { const double r = pop(); push(pop() + r); ++ip; break; }
                case Sub: { const double r = pop(); push(pop() - r); ++ip; break; }
                case Mul: { const double r = pop(); push(pop() * r); ++ip; break; }
                case Div:
                {
                    const double r = pop();
                    const double l = pop();
                    push(r == 0.0 ? 0.0 : l / r);
                    ++ip;
                    break;
                }
                case Neg: push(-pop()); ++ip; break;
                case Cmp:
                {
                    const double r = pop();
                    const double l = pop();
                    bool bit = false;
                    if(ins.a == 0) bit = l <= r;
                    else if(ins.a == 1) bit = l >= r;
                    else if(ins.a == 2) bit = l == r;
                    else if(ins.a == 3) bit = l != r;
                    else if(ins.a == 4) bit = l < r;
                    else bit = l > r;
                    push(bit ? 1.0f : 0.0f);
                    ++ip;
                    break;
                }
                case And: { const double r = pop(); const double l = pop(); push((l != 0.0 && r != 0.0) ? 1.0 : 0.0); ++ip; break; }
                case Or: { const double r = pop(); const double l = pop(); push((l != 0.0 || r != 0.0) ? 1.0 : 0.0); ++ip; break; }
                case Not: push(pop() == 0.0f ? 1.0f : 0.0f); ++ip; break;
                case Call: push(call(ins.a, (int)ins.n)); ++ip; break;
                case Jump: ip = (size_t)ins.a; break;
                case JumpIfFalse:
                    if(pop() == 0.0f) { ip = (size_t)ins.a; }
                    else { ++ip; }
                    break;
                case Paint:
                    if(led && led->block)
                    {
                        color = SampleGradient(*led->block, (float)pop());
                    }
                    else
                    {
                        pop();
                    }
                    ++ip;
                    break;
                case PaintMix:
                {
                    const float k = (float)pop();
                    const float t1 = (float)pop();
                    const float t0 = (float)pop();
                    if(led && led->block)
                    {
                        color = LerpColor(SampleGradient(*led->block, t0), SampleGradient(*led->block, t1), k);
                    }
                    ++ip;
                    break;
                }
                case Off: off = true; return false;
                case Halt: return true;
                default: return true;
            }
        }
        return !off;
    }
};

std::string EffectKey(const filesystem::path& file)
{
    std::string stem = file.stem().string();
    for(char& c : stem)
    {
        if(c >= 'A' && c <= 'Z')
        {
            c = (char)(c - 'A' + 'a');
        }
    }
    return stem;
}

void ReadProgram(const filesystem::path& file, const std::string& section)
{
    std::ifstream in;
#ifdef _WIN32
    in.open(file.wstring());
#else
    in.open(file.string());
#endif
    if(!in)
    {
        return;
    }
    std::string line;
    std::string body;
    Program prog;
    prog.section = section;
    bool header = true;
    while(std::getline(in, line))
    {
        if(!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if(header)
        {
            std::string trimmed = line;
            while(!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t'))
            {
                trimmed.erase(trimmed.begin());
            }
            if(trimmed.empty() || trimmed.front() == '#')
            {
                continue;
            }
            const auto colon = trimmed.find(':');
            if(colon == std::string::npos)
            {
                header = false;
                body += line;
                body += '\n';
                continue;
            }
            line = trimmed;
            const std::string key = line.substr(0, colon);
            std::string val = line.substr(colon + 1);
            while(!val.empty() && val.front() == ' ')
            {
                val.erase(val.begin());
            }
            if(key == "name") prog.title = val;
            else if(key == "description") prog.description = val;
            else if(key == "section") prog.section = val;
            else if(key == "space") prog.world = (val == "world");
            else if(key == "alias") prog.aliases.push_back(val);
            else if(key == "color") prog.color_ends = (val == "ends");
            else if(key == "preview") prog.preview_pulse = (val == "pulse");
            else if(key == "axis") prog.axis_angle = (val == "angle");
            else if(key == "swatch")
            {
                int r = 180, g = 180, b = 190;
                std::sscanf(val.c_str(), "%d %d %d", &r, &g, &b);
                prog.swatch_r = r;
                prog.swatch_g = g;
                prog.swatch_b = b;
            }
            continue;
        }
        body += line;
        body += '\n';
    }
    if(!Compile(body, &prog))
    {
        return;
    }
    const std::string id = EffectKey(file);
    for(const std::string& alias : prog.aliases)
    {
        g_alias[EffectKey(filesystem::path(alias))] = id;
    }
    g_programs[id] = std::move(prog);
}

const Program* FindProgram(const std::string& id)
{
    auto it = g_programs.find(id);
    if(it != g_programs.end())
    {
        return &it->second;
    }
    auto alias = g_alias.find(id);
    if(alias == g_alias.end())
    {
        return nullptr;
    }
    it = g_programs.find(alias->second);
    return it == g_programs.end() ? nullptr : &it->second;
}

void ScanDir(const filesystem::path& dir, const std::string& section, int depth)
{
    std::error_code ec;
    if(!filesystem::is_directory(dir, ec) || depth > 2)
    {
        return;
    }
    for(const auto& entry : filesystem::directory_iterator(dir, ec))
    {
        if(ec)
        {
            break;
        }
        if(entry.is_directory())
        {
            const std::string name = entry.path().filename().string();
            ScanDir(entry.path(), section.empty() ? name : section, depth + 1);
            continue;
        }
        if(!entry.is_regular_file())
        {
            continue;
        }
        const std::string ext = entry.path().extension().string();
        if(ext == ".fx")
        {
            ReadProgram(entry.path(), section);
        }
    }
}

int Var(const Program& prog, const char* name)
{
    for(size_t i = 0; i < prog.names.size(); ++i)
    {
        if(prog.names[i] == name)
        {
            return (int)i;
        }
    }
    return -1;
}

} // namespace

std::string FileId(const Block& block)
{
    return block.effect_id.empty() ? "solid" : block.effect_id;
}

void SetDirectory(const filesystem::path& dir)
{
    std::lock_guard<std::mutex> lock(g_mu);
    if(dir == g_dir && !g_programs.empty())
    {
        return;
    }
    g_dir = dir;
    g_programs.clear();
    g_alias.clear();
    ScanDir(dir, "", 0);
}

bool Has(const std::string& id)
{
    std::lock_guard<std::mutex> lock(g_mu);
    return FindProgram(id) != nullptr;
}

bool UsesWorld(const std::string& id)
{
    std::lock_guard<std::mutex> lock(g_mu);
    const Program* prog = FindProgram(id);
    return prog && prog->world;
}

bool ColorEnds(const std::string& id)
{
    std::lock_guard<std::mutex> lock(g_mu);
    const Program* prog = FindProgram(id);
    return prog && prog->color_ends;
}

bool PreviewPulse(const std::string& id)
{
    std::lock_guard<std::mutex> lock(g_mu);
    const Program* prog = FindProgram(id);
    return prog && prog->preview_pulse;
}

bool AxisAngle(const std::string& id)
{
    std::lock_guard<std::mutex> lock(g_mu);
    const Program* prog = FindProgram(id);
    return prog && prog->axis_angle;
}

bool Run(const std::string& id, LedView* led)
{
    if(!led)
    {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_mu);
    const Program* found = FindProgram(id);
    if(!found)
    {
        return false;
    }
    const Program& prog = *found;
    Machine m;
    m.prog = &prog;
    m.led = led;
    m.color = led->color;
    m.vars.assign(prog.names.size(), 0.0);
    auto set = [&](const char* name, double v) {
        const int idx = Var(prog, name);
        if(idx >= 0)
        {
            m.vars[(size_t)idx] = v;
        }
    };
    const Block* block = led->block;
    set("progress", led->progress);
    set("axis", led->axis);
    set("local_ms", (float)led->local_ms);
    set("start_ms", block ? (float)block->start_ms : 0.0f);
    set("time_ms", block ? (float)(led->local_ms - block->start_ms) : 0.0f);
    set("speed", block ? block->speed : 1.0f);
    set("period_ms", block ? (float)block->period_ms : 1000.0f);
    set("intensity", led->intensity);
    set("min_intensity", block ? block->min_intensity : 0.0f);
    set("max_intensity", block ? block->max_intensity : 1.0f);
    set("pulse", block ? block->pulse_length : 0.25f);
    set("seed", (float)led->seed);
    set("x", led->x);
    set("y", led->y);
    set("z", led->z);
    set("nx", led->nx);
    set("ny", led->ny);
    set("nz", led->nz);
    set("radius", led->radius);
    set("height", led->height);
    set("span_x", led->span_x);
    set("span_y", led->span_y);
    set("span_z", led->span_z);
    set("dx", led->dx);
    set("dy", led->dy);
    set("dz", led->dz);
    set("invert", (block && DirectionInvertsAxis(block->direction)) ? 1.0f : 0.0f);
    if(!m.run() || m.off)
    {
        return false;
    }
    const int ii = Var(prog, "intensity");
    if(ii >= 0)
    {
        led->intensity = (float)m.vars[(size_t)ii];
    }
    led->color = m.color;
    return true;
}

} // namespace script

void SetEffectScriptDirectory(const filesystem::path& dir)
{
    script::SetDirectory(dir);
}

bool EffectScriptLoaded(const std::string& id)
{
    return script::Has(id);
}

bool EffectScriptUsesWorld(const std::string& id)
{
    return script::UsesWorld(id);
}

bool EffectColorEnds(const std::string& id)
{
    return script::ColorEnds(id);
}

bool EffectPreviewPulse(const std::string& id)
{
    return script::PreviewPulse(id);
}

bool EffectAxisAngle(const std::string& id)
{
    return script::AxisAngle(id);
}

std::string BlockFileId(const Block& block)
{
    return script::FileId(block);
}
} // namespace EffectPack
