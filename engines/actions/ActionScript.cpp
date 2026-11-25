// SPDX-License-Identifier: GPL-2.0-or-later

#include "ActionScript.h"
#include "ActionBuiltinGpu.h"
#include "ActionScriptGpu.h"
#include "ActionDetail.h"
#include "ActionMedia.h"
#include "ActionParticleField.h"
#include "ActionRibbonField.h"
#include "ActionVolumeField.h"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <fstream>
#include <locale>
#include <memory>
#include <mutex>
#include <sstream>
#include <unordered_map>

namespace Action
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
    PaintStop,
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
    /** GPU volume body id (`shader:` / `field:` header) — empty = no volume field. */
    std::string field_shader;
    /** Particle field mode (`particles:` header) — GPGPU volumeMain; empty = off. */
    std::string particles;
    /** Ribbon/tube mode (`ribbons:` header) — GPGPU volumeMain; empty = off. */
    std::string ribbons;
    std::vector<std::string> aliases;
    /** Fragment shader translated from the body at load; null = CPU only. */
    std::shared_ptr<const std::string> gpu_source;
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
    FnFieldR,
    FnFieldG,
    FnFieldB,
    FnFieldA,
    FnFieldPaint,
    FnParticleD,
    FnParticlePaint,
    FnRibbonD,
    FnRibbonPaint,
    /* VFX vocabulary (mirrors SpatialVfxLib410; identical on CPU and GPU). */
    FnFbm,
    FnRidge,
    FnGlow,
    FnGauss,
    FnStreak,
    FnSmoothstep,
    FnMix,
    FnExp,
    FnPow,
    FnLen,
    FnBeat,
    FnEnvelope,
};

int VfxOctaves(double o)
{
    return (o <= 0.0) ? 3 : std::clamp((int)std::lround(o), 1, 5);
}

/** Fractal noise 0..1 (shared with fx_fbm in the GPU prelude). */
float VfxFbm(float x, float y, float z, double octaves)
{
    const int n = VfxOctaves(octaves);
    float sum = 0.0f, amp = 0.5f, tot = 0.0f;
    for(int o = 0; o < n; ++o)
    {
        sum += amp * ValueNoise3(x, y, z);
        tot += amp;
        amp *= 0.5f;
        x *= 2.03f; y *= 2.03f; z *= 2.03f;
    }
    return sum / std::max(tot, 1e-4f);
}

/** Ridged noise 0..1: sharp creases, good for lightning, cracks, energy veins. */
float VfxRidge(float x, float y, float z, double octaves)
{
    const int n = std::min(VfxOctaves(octaves), 4);
    float sum = 0.0f, amp = 0.5f, tot = 0.0f;
    for(int o = 0; o < n; ++o)
    {
        sum += amp * (1.0f - std::fabs(ValueNoise3(x, y, z) * 2.0f - 1.0f));
        tot += amp;
        amp *= 0.5f;
        x *= 2.07f; y *= 2.07f; z *= 2.07f;
    }
    return sum / std::max(tot, 1e-4f);
}

/** Hot core with a soft rim around distance 0 (vfxCoreRim). */
double VfxGlow(double d, double soft)
{
    const double s = std::max(soft, 0.008);
    const double core = std::exp(-(d * d) / (s * s * 0.28));
    const double rim = std::exp(-(d * d) / (s * s * 1.20)) * 0.42;
    return std::clamp(core + rim, 0.0, 1.0);
}

double VfxGauss(double d, double soft)
{
    const double s = std::max(soft, 0.008);
    return std::exp(-(d * d) / (s * s));
}

/** Comet head with a tail trailing behind direction u (vfxStreak). */
double VfxStreak(double dx, double dy, double dz, double ux, double uy, double uz, double soft_along, double soft_side)
{
    const double un = std::sqrt(ux * ux + uy * uy + uz * uz);
    if(un > 1e-4) { ux /= un; uy /= un; uz /= un; } else { ux = 0.0; uy = 0.0; uz = 1.0; }
    const double along = dx * ux + dy * uy + dz * uz;
    const double sx = dx - ux * along, sy = dy - uy * along, sz = dz - uz * along;
    const double side = std::sqrt(sx * sx + sy * sy + sz * sz);
    const double a = std::max(soft_along, 0.010);
    const double b = std::max(soft_side, 0.008);
    const double head = std::exp(-(along * along) / (a * a * 0.45));
    const double trail = std::exp(-(along * along) / (a * a * 2.6)) * (along <= 0.0 ? 1.0 : 0.0);
    return std::clamp(std::max(head, trail * 0.62) * std::exp(-(side * side) / (b * b)), 0.0, 1.0);
}

double VfxSmoothstep(double e0, double e1, double x)
{
    const double w = e1 - e0;
    const double t = (w == 0.0) ? (x < e0 ? 0.0 : 1.0) : std::clamp((x - e0) / w, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

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
    void parseIfTail();
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
    if(name == "field_r") return FnFieldR;
    if(name == "field_g") return FnFieldG;
    if(name == "field_b") return FnFieldB;
    if(name == "field_a") return FnFieldA;
    if(name == "field_paint") return FnFieldPaint;
    if(name == "particle_d") return FnParticleD;
    if(name == "particle_paint") return FnParticlePaint;
    if(name == "ribbon_d") return FnRibbonD;
    if(name == "ribbon_paint") return FnRibbonPaint;
    if(name == "fbm") return FnFbm;
    if(name == "ridge") return FnRidge;
    if(name == "glow") return FnGlow;
    if(name == "gauss") return FnGauss;
    if(name == "streak") return FnStreak;
    if(name == "smoothstep") return FnSmoothstep;
    if(name == "mix") return FnMix;
    if(name == "exp") return FnExp;
    if(name == "pow") return FnPow;
    if(name == "len") return FnLen;
    if(name == "beat") return FnBeat;
    if(name == "envelope") return FnEnvelope;
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
        if(name == "paint_stop" || name == "swatch")
        {
            parseExpr();
            acceptSym(")");
            emit(PaintStop);
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

void Parser::parseIfTail()
{
    /* Condition and body after "if" or "elif"; one "end" closes the whole chain. */
    parseExpr();
    const int jump_false = (int)prog->code.size();
    emit(JumpIfFalse, -1);
    parseBlock();
    const bool is_elif = acceptId("elif");
    if(is_elif || acceptId("else"))
    {
        const int jump_end = (int)prog->code.size();
        emit(Jump, -1);
        prog->code[(size_t)jump_false].a = (int)prog->code.size();
        if(is_elif)
        {
            parseIfTail();
        }
        else
        {
            parseBlock();
            acceptId("end");
        }
        prog->code[(size_t)jump_end].a = (int)prog->code.size();
        return;
    }
    prog->code[(size_t)jump_false].a = (int)prog->code.size();
    acceptId("end");
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
        parseIfTail();
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
            /* Not strtod: Qt sets the system locale on Linux, where "0.5" can parse as 0. */
            std::istringstream num(body.substr(i, j - i));
            num.imbue(std::locale::classic());
            t.n = 0.0;
            num >> t.n;
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

/* ---------------------------------------------------------------------------
 * .fx -> GLSL translator. Runs once per effect file at load. The generated
 * fragment shader evaluates the script for every LED in one draw (one texel per
 * LED) with the same semantics as the CPU Machine: float maths, divide-by-zero = 0,
 * C fmod, the same integer hashes and noise, the same gradient rounding. Scripts
 * that call field / particle / ribbon samplers or sample_axis stay on the CPU.
 * ------------------------------------------------------------------------- */

const char* const kGpuReserved[] = {
    "progress", "axis", "time_ms", "start_ms", "speed", "period_ms", "period_progress",
    "intensity", "min_intensity", "max_intensity", "pulse", "seed", "x", "y", "z",
    "nx", "ny", "nz", "radius", "height", "span_x", "span_y", "span_z",
    "dx", "dy", "dz", "invert", "local_ms", "stops",
    "ox", "oy", "oz", "effect_size",
};

/* Per-LED inputs, 20 floats = 5 RGBA32F texels per LED (see GpuLedInput).
   Slot 16 is sample_axis() precomputed on the CPU (it needs the clip's bounds). */
const char* const kGpuLedAttrs[16] = {
    "axis", "seed", "x", "y", "z", "nx", "ny", "nz",
    "radius", "height", "span_x", "span_y", "span_z", "dx", "dy", "dz",
};

/* Per-clip scalars, uploaded as u_s[] in this order (see RunBatchGpu). */
const char* const kGpuClipScalars[] = {
    "progress", "time_ms", "start_ms", "speed", "period_ms", "period_progress",
    "intensity", "min_intensity", "max_intensity", "pulse", "local_ms", "stops",
    "invert", "ox", "oy", "oz", "effect_size",
};
constexpr int kGpuClipScalarCount = (int)(sizeof(kGpuClipScalars) / sizeof(kGpuClipScalars[0]));
constexpr int kGpuMaxStops = 32;

std::string GlslNumber(double v)
{
    std::ostringstream s;
    s.imbue(std::locale::classic());
    s.precision(9);
    s << v;
    std::string out = s.str();
    if(out.find_first_of(".eE") == std::string::npos)
    {
        out += ".0";
    }
    return out;
}

struct GlslGen
{
    std::vector<Tok> toks;
    size_t i = 0;
    bool ok = true;
    std::vector<std::string> vars;
    std::string out;
    int indent = 1;
    int guard_id = 0;

    const Tok& cur() const { return toks[std::min(i, toks.size() - 1)]; }
    void next() { if(i + 1 < toks.size()) { ++i; } }
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
    void line(const std::string& s)
    {
        out += std::string((size_t)indent * 4u, ' ');
        out += s;
        out += '\n';
    }
    std::string var(const std::string& name)
    {
        if(std::find(vars.begin(), vars.end(), name) == vars.end())
        {
            vars.push_back(name);
        }
        return "s_" + name;
    }
    bool atStop() const
    {
        return cur().kind == Tok::End
            || (cur().kind == Tok::Id && (cur().text == "end" || cur().text == "else" || cur().text == "elif"));
    }

    std::string expr() { return orExpr(); }

    std::string orExpr()
    {
        std::string l = andExpr();
        while(acceptSym("||"))
        {
            l = "fx_or(" + l + ", " + andExpr() + ")";
        }
        return l;
    }

    std::string andExpr()
    {
        std::string l = cmpExpr();
        while(acceptSym("&&"))
        {
            l = "fx_and(" + l + ", " + cmpExpr() + ")";
        }
        return l;
    }

    std::string cmpExpr()
    {
        std::string l = addExpr();
        const char* ops[] = {"<=", ">=", "==", "!=", "<", ">"};
        for(const char* op : ops)
        {
            if(acceptSym(op))
            {
                const std::string r = addExpr();
                return "((" + l + ") " + op + " (" + r + ") ? 1.0 : 0.0)";
            }
        }
        return l;
    }

    std::string addExpr()
    {
        std::string l = mulExpr();
        while(cur().kind == Tok::Sym && (cur().text == "+" || cur().text == "-"))
        {
            const std::string op = cur().text;
            next();
            l = "(" + l + " " + op + " " + mulExpr() + ")";
        }
        return l;
    }

    std::string mulExpr()
    {
        std::string l = unaryExpr();
        while(cur().kind == Tok::Sym && (cur().text == "*" || cur().text == "/"))
        {
            const bool div = cur().text == "/";
            next();
            const std::string r = unaryExpr();
            l = div ? ("fx_div(" + l + ", " + r + ")") : ("(" + l + " * " + r + ")");
        }
        return l;
    }

    std::string unaryExpr()
    {
        if(acceptSym("-"))
        {
            return "(-" + unaryExpr() + ")";
        }
        if(acceptSym("!"))
        {
            return "fx_not(" + unaryExpr() + ")";
        }
        return primary();
    }

    std::string primary()
    {
        if(cur().kind == Tok::Num)
        {
            const std::string n = GlslNumber(cur().n);
            next();
            return n;
        }
        if(acceptSym("("))
        {
            const std::string e = expr();
            acceptSym(")");
            return "(" + e + ")";
        }
        if(cur().kind != Tok::Id)
        {
            ok = false;
            return "0.0";
        }
        const std::string name = cur().text;
        next();
        if(!acceptSym("("))
        {
            return var(name);
        }
        if(name == "paint")
        {
            const std::string a = expr();
            acceptSym(")");
            return "fx_paint(" + a + ")";
        }
        if(name == "paint_stop" || name == "swatch")
        {
            const std::string a = expr();
            acceptSym(")");
            return "fx_paint_stop(" + a + ")";
        }
        if(name == "paint_mix")
        {
            const std::string a = expr();
            acceptSym(",");
            const std::string b = expr();
            acceptSym(",");
            const std::string c = expr();
            acceptSym(")");
            return "fx_paint_mix(" + a + ", " + b + ", " + c + ")";
        }
        std::vector<std::string> args;
        if(!acceptSym(")"))
        {
            args.push_back(expr());
            while(acceptSym(","))
            {
                args.push_back(expr());
            }
            acceptSym(")");
        }
        struct FnMap { const char* name; const char* glsl; int arity; };
        static const FnMap kFns[] = {
            {"sin", "sin", 1}, {"cos", "cos", 1}, {"abs", "abs", 1},
            {"min", "min", 2}, {"max", "max", 2}, {"clamp", "fx_clamp", 3},
            {"floor", "floor", 1}, {"sqrt", "fx_sqrt", 1}, {"atan2", "fx_atan2", 2},
            {"fmod", "fx_fmod", 2}, {"fract", "fract", 1},
            {"band", "fx_band", 2}, {"shr", "fx_shr", 2}, {"xor", "fx_xor", 2},
            {"hash", "fx_hash", 3}, {"hash_phase", "fx_hash_phase", 3}, {"hash_byte", "fx_hash_byte", 4},
            {"hash01", "fx_hash01", 1}, {"hash01_mix", "fx_hash01_mix", 3}, {"noise", "fx_noise", 3},
            {"axis_x", "fx_axis_x", 0}, {"axis_y", "fx_axis_y", 0}, {"axis_z", "fx_axis_z", 0},
            {"sample_axis", "fx_sample_axis", 0},
            {"fbm", "fx_fbm", 4}, {"ridge", "fx_ridge", 4}, {"glow", "fx_glow", 2},
            {"gauss", "fx_gauss", 2}, {"streak", "fx_streak", 8}, {"smoothstep", "fx_smoothstep", 3},
            {"mix", "fx_mix", 3}, {"exp", "fx_exp", 1}, {"pow", "fx_pow", 2},
            {"len", "fx_len", 3}, {"beat", "fx_beat", 2}, {"envelope", "fx_envelope", 1},
        };
        for(const FnMap& f : kFns)
        {
            if(name != f.name)
            {
                continue;
            }
            std::string call = std::string(f.glsl) + "(";
            for(int k = 0; k < f.arity; ++k)
            {
                if(k > 0)
                {
                    call += ", ";
                }
                call += (k < (int)args.size()) ? args[(size_t)k] : std::string("0.0");
            }
            return call + ")";
        }
        /* field_*, particle_*, ribbon_* and unknown names: CPU only. */
        ok = false;
        return "0.0";
    }

    void block()
    {
        while(ok && !atStop())
        {
            stmt();
        }
    }

    void ifTail()
    {
        const std::string c = expr();
        line("if((" + c + ") != 0.0) {");
        ++indent;
        block();
        --indent;
        if(acceptId("elif"))
        {
            line("} else {");
            ++indent;
            ifTail();
            --indent;
            line("}");
            return;
        }
        if(acceptId("else"))
        {
            line("} else {");
            ++indent;
            block();
            --indent;
        }
        line("}");
        acceptId("end");
    }

    void stmt()
    {
        if(acceptId("off"))
        {
            line("g_off = true;");
            line("return;");
            return;
        }
        if(acceptId("if"))
        {
            ifTail();
            return;
        }
        if(acceptId("while"))
        {
            const std::string c = expr();
            const std::string g = "g_loop" + std::to_string(guard_id++);
            line("{");
            ++indent;
            line("int " + g + " = 0;");
            line("while((" + c + ") != 0.0 && " + g + " < 4096) {");
            ++indent;
            line(g + "++;");
            block();
            --indent;
            line("}");
            --indent;
            line("}");
            acceptId("end");
            return;
        }
        if(cur().kind == Tok::Id)
        {
            const size_t ahead = std::min(i + 1, toks.size() - 1);
            if(toks[ahead].kind == Tok::Sym && toks[ahead].text == "(")
            {
                line(primary() + ";");
                return;
            }
            const std::string name = cur().text;
            next();
            if(!acceptSym("="))
            {
                ok = false;
                return;
            }
            const std::string target = var(name);
            line(target + " = " + expr() + ";");
            return;
        }
        ok = false;
        next();
    }
};

const char* kGpuPrelude = R"GLSL(#version 410 core
uniform sampler2D u_attr;
uniform int u_width;
uniform int u_count;
uniform float u_s[17];
uniform vec3 u_axis_vec;
uniform vec3 u_color;
uniform float u_mod;
uniform int u_nstops;
uniform float u_stop_pos[32];
uniform vec3 u_stop_col[32];
out vec4 frag_color;

vec3 g_col;
bool g_off;
float g_sample_axis;

float fx_div(float a, float b) { return b == 0.0 ? 0.0 : a / b; }
float fx_not(float a) { return a == 0.0 ? 1.0 : 0.0; }
float fx_and(float a, float b) { return (a != 0.0 && b != 0.0) ? 1.0 : 0.0; }
float fx_or(float a, float b) { return (a != 0.0 || b != 0.0) ? 1.0 : 0.0; }
float fx_clamp(float v, float lo, float hi) { return min(max(v, lo), hi); }
float fx_sqrt(float a) { return sqrt(max(0.0, a)); }
float fx_atan2(float y, float x) { return (x == 0.0 && y == 0.0) ? 0.0 : atan(y, x); }
float fx_fmod(float a, float m) { return m == 0.0 ? 0.0 : a - m * trunc(a / m); }
uint fx_u(float v) { return v < 0.0 ? uint(int(v)) : uint(v); }
float fx_band(float a, float b) { return float(fx_u(a) & fx_u(b)); }
float fx_shr(float a, float b) { return float(fx_u(a) >> fx_u(b)); }
float fx_xor(float a, float b) { return float(fx_u(a) ^ fx_u(b)); }
uint fx_hashled(float led, float ms, float period)
{
    int p = max(1, int(period));
    uint x = uint(int(led)) * 374761393u + uint(int(ms) / p) * 668265263u;
    x = (x ^ (x >> 13u)) * 1274126177u;
    return x ^ (x >> 16u);
}
float fx_hash(float a, float b, float c) { return float(fx_hashled(a, b, c)); }
float fx_hash_phase(float a, float b, float c) { return float(fx_hashled(a, b, c) & 0xFFFFu) / 65535.0; }
float fx_hash_byte(float a, float b, float c, float d) { return float((fx_hashled(a, b, c) >> fx_u(d)) & 0xFFu) / 255.0; }
float fx_hash01u(uint h)
{
    h ^= h >> 16u;
    h *= 0x7feb352du;
    h ^= h >> 15u;
    h *= 0x846ca68bu;
    h ^= h >> 16u;
    return float(h & 0xFFFFFFu) / 16777215.0;
}
float fx_hash01(float a) { return fx_hash01u(fx_u(a)); }
float fx_hash01_mix(float a, float b, float c) { return fx_hash01u(fx_u(a) * fx_u(b) + fx_u(c)); }
float fx_corner(int x, int y, int z)
{
    return fx_hash01u((uint(x) * 73856093u) ^ (uint(y) * 19349663u) ^ (uint(z) * 83492791u));
}
float fx_noise(float x, float y, float z)
{
    int ix = int(floor(x));
    int iy = int(floor(y));
    int iz = int(floor(z));
    float fx = x - float(ix);
    float fy = y - float(iy);
    float fz = z - float(iz);
    float sx = fx * fx * (3.0 - 2.0 * fx);
    float sy = fy * fy * (3.0 - 2.0 * fy);
    float sz = fz * fz * (3.0 - 2.0 * fz);
    float x00 = mix(fx_corner(ix, iy, iz), fx_corner(ix + 1, iy, iz), sx);
    float x10 = mix(fx_corner(ix, iy + 1, iz), fx_corner(ix + 1, iy + 1, iz), sx);
    float x01 = mix(fx_corner(ix, iy, iz + 1), fx_corner(ix + 1, iy, iz + 1), sx);
    float x11 = mix(fx_corner(ix, iy + 1, iz + 1), fx_corner(ix + 1, iy + 1, iz + 1), sx);
    return mix(mix(x00, x10, sy), mix(x01, x11, sy), sz);
}
int fx_octaves(float o) { return (o <= 0.0) ? 3 : clamp(int(floor(o + 0.5)), 1, 5); }
float fx_fbm(float x, float y, float z, float oct)
{
    int n = fx_octaves(oct);
    float sum = 0.0, amp = 0.5, tot = 0.0;
    vec3 q = vec3(x, y, z);
    for(int o = 0; o < 5; ++o)
    {
        if(o >= n) { break; }
        sum += amp * fx_noise(q.x, q.y, q.z);
        tot += amp;
        amp *= 0.5;
        q *= 2.03;
    }
    return sum / max(tot, 1e-4);
}
float fx_ridge(float x, float y, float z, float oct)
{
    int n = min(fx_octaves(oct), 4);
    float sum = 0.0, amp = 0.5, tot = 0.0;
    vec3 q = vec3(x, y, z);
    for(int o = 0; o < 4; ++o)
    {
        if(o >= n) { break; }
        sum += amp * (1.0 - abs(fx_noise(q.x, q.y, q.z) * 2.0 - 1.0));
        tot += amp;
        amp *= 0.5;
        q *= 2.07;
    }
    return sum / max(tot, 1e-4);
}
float fx_glow(float d, float soft)
{
    float s = max(soft, 0.008);
    return clamp(exp(-(d * d) / (s * s * 0.28)) + exp(-(d * d) / (s * s * 1.20)) * 0.42, 0.0, 1.0);
}
float fx_gauss(float d, float soft) { float s = max(soft, 0.008); return exp(-(d * d) / (s * s)); }
float fx_streak(float dx, float dy, float dz, float ux, float uy, float uz, float sa, float ss)
{
    vec3 u = vec3(ux, uy, uz);
    float un = length(u);
    u = (un > 1e-4) ? u / un : vec3(0.0, 0.0, 1.0);
    vec3 dl = vec3(dx, dy, dz);
    float along = dot(dl, u);
    float side = length(dl - u * along);
    float a = max(sa, 0.010);
    float b = max(ss, 0.008);
    float head = exp(-(along * along) / (a * a * 0.45));
    float trail = exp(-(along * along) / (a * a * 2.6)) * (along <= 0.0 ? 1.0 : 0.0);
    return clamp(max(head, trail * 0.62) * exp(-(side * side) / (b * b)), 0.0, 1.0);
}
float fx_smoothstep(float e0, float e1, float x)
{
    float w = e1 - e0;
    float t = (w == 0.0) ? (x < e0 ? 0.0 : 1.0) : clamp((x - e0) / w, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}
float fx_mix(float a, float b, float t) { return a + (b - a) * t; }
float fx_exp(float x) { return exp(clamp(x, -80.0, 80.0)); }
float fx_pow(float x, float y) { return pow(max(0.0, x), y); }
float fx_len(float x, float y, float z) { return length(vec3(x, y, z)); }
float fx_beat(float t, float s) { return pow(0.5 + 0.5 * sin(t), max(s, 0.5)); }
float fx_envelope(float p)
{
    p = clamp(p, 0.0, 1.0);
    return fx_smoothstep(0.0, 0.12, p) * (1.0 - fx_smoothstep(0.72, 1.0, p));
}
float fx_axis_x() { return u_axis_vec.x; }
float fx_axis_y() { return u_axis_vec.y; }
float fx_axis_z() { return u_axis_vec.z; }
float fx_sample_axis() { return g_sample_axis; }
vec3 fx_lerp(vec3 a, vec3 b, float t)
{
    t = clamp(t, 0.0, 1.0);
    return floor(a * (1.0 - t) + b * t + 0.5);
}
vec3 fx_grad(float t)
{
    t = clamp(t, 0.0, 1.0);
    int n = max(1, u_nstops);
    if(n == 1 || t <= u_stop_pos[0]) { return u_stop_col[0]; }
    if(t >= u_stop_pos[n - 1]) { return u_stop_col[n - 1]; }
    for(int i = 1; i < 32; ++i)
    {
        if(i >= n) { break; }
        if(t <= u_stop_pos[i])
        {
            float span = max(1e-6, u_stop_pos[i] - u_stop_pos[i - 1]);
            return fx_lerp(u_stop_col[i - 1], u_stop_col[i], (t - u_stop_pos[i - 1]) / span);
        }
    }
    return u_stop_col[n - 1];
}
float fx_paint(float t) { g_col = fx_grad(t); return 0.0; }
float fx_paint_stop(float x)
{
    int n = max(1, u_nstops);
    int idx = int(sign(x) * floor(abs(x) + 0.5));
    idx = idx - n * int(floor(float(idx) / float(n)));
    g_col = u_stop_col[clamp(idx, 0, n - 1)];
    return 0.0;
}
float fx_paint_mix(float t0, float t1, float k) { g_col = fx_lerp(fx_grad(t0), fx_grad(t1), k); return 0.0; }
)GLSL";

/** Full fragment shader for one script, or empty when it cannot run on the GPU. */
std::string TranslateToGlsl(const std::string& body)
{
    GlslGen gen;
    gen.toks = Tokenize(body);
    for(const char* name : kGpuReserved)
    {
        gen.var(name);
    }
    while(gen.ok && gen.cur().kind != Tok::End)
    {
        gen.stmt();
    }
    if(!gen.ok)
    {
        return std::string();
    }
    std::string src = kGpuPrelude;
    for(const std::string& v : gen.vars)
    {
        src += "float s_" + v + " = 0.0;\n";
    }
    src += "\nvoid fxMain()\n{\n" + gen.out + "}\n\n";
    src += "void main()\n{\n";
    src += "    int ix = int(floor(gl_FragCoord.x));\n";
    src += "    int iy = int(floor(gl_FragCoord.y));\n";
    src += "    if(iy * u_width + ix >= u_count) { frag_color = vec4(0.0); return; }\n";
    src += "    vec4 a0 = texelFetch(u_attr, ivec2(ix * 5 + 0, iy), 0);\n";
    src += "    vec4 a1 = texelFetch(u_attr, ivec2(ix * 5 + 1, iy), 0);\n";
    src += "    vec4 a2 = texelFetch(u_attr, ivec2(ix * 5 + 2, iy), 0);\n";
    src += "    vec4 a3 = texelFetch(u_attr, ivec2(ix * 5 + 3, iy), 0);\n";
    src += "    g_sample_axis = texelFetch(u_attr, ivec2(ix * 5 + 4, iy), 0).x;\n";
    const char* comps[16] = {"a0.x", "a0.y", "a0.z", "a0.w", "a1.x", "a1.y", "a1.z", "a1.w",
                             "a2.x", "a2.y", "a2.z", "a2.w", "a3.x", "a3.y", "a3.z", "a3.w"};
    for(int k = 0; k < 16; ++k)
    {
        src += std::string("    s_") + kGpuLedAttrs[k] + " = " + comps[k] + ";\n";
    }
    for(int k = 0; k < kGpuClipScalarCount; ++k)
    {
        src += std::string("    s_") + kGpuClipScalars[k] + " = u_s[" + std::to_string(k) + "];\n";
    }
    src += "    g_col = u_color;\n";
    src += "    g_off = false;\n";
    src += "    fxMain();\n";
    src += "    if(g_off) { frag_color = vec4(0.0); return; }\n";
    src += "    float k = clamp(s_intensity * u_mod, 0.0, 1.0);\n";
    src += "    frag_color = vec4(floor(g_col * k + 0.5) / 255.0, 1.0);\n";
    src += "}\n";
    return src;
}

bool Compile(const std::string& body, Program* prog)
{
    Parser p;
    p.toks = Tokenize(body);
    p.prog = prog;
    const char* reserved[] = {
        "progress", "axis", "time_ms", "start_ms", "speed", "period_ms", "period_progress",
        "intensity", "min_intensity", "max_intensity", "pulse", "seed", "x", "y", "z",
        "nx", "ny", "nz", "radius", "height", "span_x", "span_y", "span_z",
        "dx", "dy", "dz", "invert", "local_ms", "stops",
        "ox", "oy", "oz", "effect_size",
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
            case FnFbm: return VfxFbm((float)a(0), (float)a(1), (float)a(2), a(3));
            case FnRidge: return VfxRidge((float)a(0), (float)a(1), (float)a(2), a(3));
            case FnGlow: return VfxGlow(a(0), a(1));
            case FnGauss: return VfxGauss(a(0), a(1));
            case FnStreak: return VfxStreak(a(0), a(1), a(2), a(3), a(4), a(5), a(6), a(7));
            case FnSmoothstep: return VfxSmoothstep(a(0), a(1), a(2));
            case FnMix: return a(0) + (a(1) - a(0)) * a(2);
            case FnExp: return std::exp(std::clamp(a(0), -80.0, 80.0));
            case FnPow: return std::pow(std::max(0.0, a(0)), a(1));
            case FnLen: return std::sqrt(a(0) * a(0) + a(1) * a(1) + a(2) * a(2));
            case FnBeat:
            {
                const double s = std::max(a(1), 0.5);
                return std::pow(0.5 + 0.5 * std::sin(a(0)), s);
            }
            case FnEnvelope:
            {
                const double p = std::clamp(a(0), 0.0, 1.0);
                return VfxSmoothstep(0.0, 0.12, p) * (1.0 - VfxSmoothstep(0.72, 1.0, p));
            }
            case FnAxisX:
            case FnAxisY:
            case FnAxisZ:
            {
                float ux = 0.0f, uy = 0.0f, uz = 0.0f;
                if(led && led->clip)
                {
                    AxisUnitVector(*led->clip, &ux, &uy, &uz);
                }
                if(fn == FnAxisX) return ux;
                if(fn == FnAxisY) return uy;
                return uz;
            }
            case FnSampleAxis:
                if(!led || !led->clip)
                {
                    return 0.0f;
                }
                return ApplyClipAxisMirror(*led->clip,
                    SampleAxisPos(*led->clip, led->x, led->y, led->z,
                                  led->min_x, led->max_x, led->min_y, led->max_y, led->min_z, led->max_z));
            case FnFieldR:
            case FnFieldG:
            case FnFieldB:
            case FnFieldA:
            case FnFieldPaint:
            {
                if(!led)
                {
                    return 0.0f;
                }
                const volume_field::SampleRgb samp = volume_field::Sample01(led->nx, led->ny, led->nz);
                if(fn == FnFieldR)
                {
                    return samp.r;
                }
                if(fn == FnFieldG)
                {
                    return samp.g;
                }
                if(fn == FnFieldB)
                {
                    return samp.b;
                }
                if(fn == FnFieldA)
                {
                    return volume_field::SampleA01(led->nx, led->ny, led->nz);
                }
                const int r = (int)std::lround(std::clamp(samp.r, 0.0f, 1.0f) * 255.0f);
                const int g = (int)std::lround(std::clamp(samp.g, 0.0f, 1.0f) * 255.0f);
                const int b = (int)std::lround(std::clamp(samp.b, 0.0f, 1.0f) * 255.0f);
                color = ToRGBColor(r, g, b);
                return std::max(samp.r, std::max(samp.g, samp.b));
            }
            case FnParticleD:
                if(!led)
                {
                    return 0.0f;
                }
                return particle_field::SampleDensity01(led->nx, led->ny, led->nz);
            case FnParticlePaint:
            {
                if(!led)
                {
                    return 0.0f;
                }
                const particle_field::SampleRgb samp =
                    particle_field::Sample01(led->nx, led->ny, led->nz);
                const int r = (int)std::lround(std::clamp(samp.r, 0.0f, 1.0f) * 255.0f);
                const int g = (int)std::lround(std::clamp(samp.g, 0.0f, 1.0f) * 255.0f);
                const int b = (int)std::lround(std::clamp(samp.b, 0.0f, 1.0f) * 255.0f);
                color = ToRGBColor(r, g, b);
                return std::max(samp.r, std::max(samp.g, samp.b));
            }
            case FnRibbonD:
                if(!led)
                {
                    return 0.0f;
                }
                return ribbon_field::SampleDensity01(led->nx, led->ny, led->nz);
            case FnRibbonPaint:
            {
                if(!led)
                {
                    return 0.0f;
                }
                const ribbon_field::SampleRgb samp =
                    ribbon_field::Sample01(led->nx, led->ny, led->nz);
                const int r = (int)std::lround(std::clamp(samp.r, 0.0f, 1.0f) * 255.0f);
                const int g = (int)std::lround(std::clamp(samp.g, 0.0f, 1.0f) * 255.0f);
                const int b = (int)std::lround(std::clamp(samp.b, 0.0f, 1.0f) * 255.0f);
                color = ToRGBColor(r, g, b);
                return std::max(samp.r, std::max(samp.g, samp.b));
            }
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
                    if(led && led->clip)
                    {
                        color = SampleGradient(*led->clip, (float)pop());
                    }
                    else
                    {
                        pop();
                    }
                    ++ip;
                    break;
                case PaintStop:
                    if(led && led->clip)
                    {
                        color = SampleGradientStop(*led->clip, (int)std::lround(pop()));
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
                    if(led && led->clip)
                    {
                        color = LerpColor(SampleGradient(*led->clip, t0), SampleGradient(*led->clip, t1), k);
                    }
                    ++ip;
                    break;
                }
                case Off:
                    off = true;
                    if(led)
                    {
                        led->turned_off = true;
                    }
                    return false;
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
            else if(key == "shader" || key == "field")
            {
                prog.field_shader = val;
                while(!prog.field_shader.empty()
                      && (prog.field_shader.back() == ' ' || prog.field_shader.back() == '\t'))
                {
                    prog.field_shader.pop_back();
                }
                for(char& c : prog.field_shader)
                {
                    if(c >= 'A' && c <= 'Z')
                    {
                        c = (char)(c - 'A' + 'a');
                    }
                }
                if(!prog.field_shader.empty())
                {
                    prog.world = true;
                }
            }
            else if(key == "particles")
            {
                prog.particles = val;
                while(!prog.particles.empty()
                      && (prog.particles.back() == ' ' || prog.particles.back() == '\t'))
                {
                    prog.particles.pop_back();
                }
                for(char& c : prog.particles)
                {
                    if(c >= 'A' && c <= 'Z')
                    {
                        c = (char)(c - 'A' + 'a');
                    }
                }
                if(!prog.particles.empty() && prog.particles != "off" && prog.particles != "none")
                {
                    prog.world = true;
                }
                else
                {
                    prog.particles.clear();
                }
            }
            else if(key == "ribbons" || key == "ribbon")
            {
                prog.ribbons = val;
                while(!prog.ribbons.empty()
                      && (prog.ribbons.back() == ' ' || prog.ribbons.back() == '\t'))
                {
                    prog.ribbons.pop_back();
                }
                for(char& c : prog.ribbons)
                {
                    if(c >= 'A' && c <= 'Z')
                    {
                        c = (char)(c - 'A' + 'a');
                    }
                }
                if(!prog.ribbons.empty() && prog.ribbons != "off" && prog.ribbons != "none")
                {
                    prog.world = true;
                }
                else
                {
                    prog.ribbons.clear();
                }
            }
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
    if(prog.field_shader.empty() && prog.particles.empty() && prog.ribbons.empty())
    {
        std::string glsl = TranslateToGlsl(body);
        if(!glsl.empty())
        {
            prog.gpu_source = std::make_shared<const std::string>(std::move(glsl));
        }
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
    /* Sorted: directory order is arbitrary on Linux, and a later duplicate wins. */
    std::vector<filesystem::directory_entry> entries;
    for(const auto& entry : filesystem::directory_iterator(dir, ec))
    {
        if(ec)
        {
            break;
        }
        entries.push_back(entry);
    }
    std::sort(entries.begin(), entries.end());
    for(const auto& entry : entries)
    {
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

std::string FileId(const Clip& clip)
{
    return clip.effect_id.empty() ? "solid" : clip.effect_id;
}

void SetDirectory(const filesystem::path& dir)
{
    {
        std::lock_guard<std::mutex> lock(g_mu);
        if(dir == g_dir && !g_programs.empty())
        {
            volume_field::SetToolboxDirectory(dir);
            return;
        }
        g_dir = dir;
        g_programs.clear();
        g_alias.clear();
        ScanDir(dir, "", 0);
    }
    volume_field::SetToolboxDirectory(dir);
    volume_field::Reset();
    particle_field::Reset();
    ribbon_field::Reset();
    builtin_gpu::Reset();
    ResetMediaGpu();
}

void ReloadDirectory()
{
    {
        std::lock_guard<std::mutex> lock(g_mu);
        if(g_dir.empty())
        {
            return;
        }
        g_programs.clear();
        g_alias.clear();
        ScanDir(g_dir, "", 0);
    }
    volume_field::Reset();
    particle_field::Reset();
    ribbon_field::Reset();
    builtin_gpu::Reset();
    ResetMediaGpu();
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

std::string FieldShader(const std::string& id)
{
    std::lock_guard<std::mutex> lock(g_mu);
    const Program* prog = FindProgram(id);
    return prog ? prog->field_shader : std::string();
}

bool UsesField(const std::string& id)
{
    return !FieldShader(id).empty();
}

bool UsesParticles(const std::string& id)
{
    return !ParticlesMode(id).empty();
}

std::string ParticlesMode(const std::string& id)
{
    std::lock_guard<std::mutex> lock(g_mu);
    const Program* prog = FindProgram(id);
    return prog ? prog->particles : std::string();
}

bool UsesRibbons(const std::string& id)
{
    return !RibbonsMode(id).empty();
}

std::string RibbonsMode(const std::string& id)
{
    std::lock_guard<std::mutex> lock(g_mu);
    const Program* prog = FindProgram(id);
    return prog ? prog->ribbons : std::string();
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
    led->turned_off = false;
    m.vars.assign(prog.names.size(), 0.0);
    auto set = [&](const char* name, double v) {
        const int idx = Var(prog, name);
        if(idx >= 0)
        {
            m.vars[(size_t)idx] = v;
        }
    };
    const Clip* clip = led->clip;
    set("progress", led->progress);
    set("axis", led->axis);
    set("local_ms", (float)led->local_ms);
    set("start_ms", clip ? (float)clip->start_ms : 0.0f);
    // Prefer continuous anim clock when playing forever/while_active so blink/pulse/noise
    // tools do not jump when LocalMs wraps (GpuAnimMs set by CueRuntime / preview tick).
    {
        const int anim_ms = GpuAnimMsOr(led->local_ms);
        set("time_ms", clip ? (float)std::max(0, anim_ms - clip->start_ms) : 0.0f);
    }
    set("speed", clip ? clip->speed : 1.0f);
    set("period_ms", clip ? (float)clip->period_ms : 1000.0f);
    set("period_progress", clip ? ClipPeriodProgress(*clip, led->local_ms) : 0.0f);
    set("intensity", led->intensity);
    set("min_intensity", clip ? clip->min_intensity : 0.0f);
    set("max_intensity", clip ? clip->max_intensity : 1.0f);
    set("pulse", clip ? clip->pulse_length : 0.25f);
    set("seed", (float)led->seed);
    set("stops", clip ? (float)UniqueGradientStopCount(*clip) : 1.0f);
    set("x", led->x);
    set("y", led->y);
    set("z", led->z);
    set("nx", led->nx);
    set("ny", led->ny);
    set("nz", led->nz);
    {
        const ClipOrigin01 origin = GetActiveClipOrigin01();
        set("ox", origin.ox);
        set("oy", origin.oy);
        set("oz", origin.oz);
        set("effect_size", origin.size);
    }
    set("radius", led->radius);
    set("height", led->height);
    set("span_x", led->span_x);
    set("span_y", led->span_y);
    set("span_z", led->span_z);
    set("dx", led->dx);
    set("dy", led->dy);
    set("dz", led->dz);
    set("invert", (clip && DirectionInvertsAxis(clip->direction)) ? 1.0f : 0.0f);
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

bool GpuCapable(const std::string& id)
{
    if(builtin_gpu::Uses(id))
    {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(g_mu);
        const Program* prog = FindProgram(id);
        if(!prog || !prog->gpu_source)
        {
            return false;
        }
    }
    return script_gpu::Available();
}

bool RunBatchGpu(const std::string& id,
                 const Clip& clip,
                 int local_ms,
                 const std::vector<GpuLedInput>& leds,
                 std::vector<RGBColor>* colors,
                 std::vector<unsigned char>* lit)
{
    if(leds.empty() || !colors || !lit)
    {
        return false;
    }
    std::shared_ptr<const std::string> source;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        const Program* prog = FindProgram(id);
        if(!prog || !prog->gpu_source)
        {
            return false;
        }
        source = prog->gpu_source;
    }

    /* Same per-clip inputs as Run(), in kGpuClipScalars order. */
    script_gpu::Uniforms u;
    const ClipOrigin01 origin = GetActiveClipOrigin01();
    const float scalars[kGpuClipScalarCount] = {
        ClipSweepProgress(clip, local_ms),
        (float)std::max(0, GpuAnimMsOr(local_ms) - clip.start_ms),
        (float)clip.start_ms,
        clip.speed,
        (float)clip.period_ms,
        ClipPeriodProgress(clip, local_ms),
        std::clamp(clip.intensity, 0.0f, 1.0f),
        clip.min_intensity,
        clip.max_intensity,
        clip.pulse_length,
        (float)local_ms,
        (float)UniqueGradientStopCount(clip),
        DirectionInvertsAxis(clip.direction) ? 1.0f : 0.0f,
        origin.ox,
        origin.oy,
        origin.oz,
        origin.size,
    };
    std::copy(scalars, scalars + kGpuClipScalarCount, u.scalars);
    AxisUnitVector(clip, &u.axis_vec[0], &u.axis_vec[1], &u.axis_vec[2]);
    u.color[0] = (float)RGBGetRValue(clip.color);
    u.color[1] = (float)RGBGetGValue(clip.color);
    u.color[2] = (float)RGBGetBValue(clip.color);
    /* ModulateClipIntensity as a factor: the shader clamps (script intensity * factor). */
    u.intensity_mod = 1.0f;
    if(!clip.intensity_curve.empty())
    {
        u.intensity_mod *= SampleCurve(clip.intensity_curve, ClipSpanProgress(clip, local_ms));
    }
    if(!clip.period_curve.empty())
    {
        u.intensity_mod *= SampleCurve(clip.period_curve, ClipPeriodProgress(clip, local_ms));
    }
    Clip stops = clip;
    EnsureClipGradient(&stops);
    if(stops.gradient.empty())
    {
        stops.gradient.push_back({0.0f, clip.color});
    }
    u.stop_count = (int)std::min<size_t>(stops.gradient.size(), (size_t)kGpuMaxStops);
    for(int k = 0; k < u.stop_count; ++k)
    {
        const GradientStop& s = stops.gradient[(size_t)k];
        u.stop_pos[k] = s.pos;
        u.stop_col[k * 3 + 0] = (float)RGBGetRValue(s.color);
        u.stop_col[k * 3 + 1] = (float)RGBGetGValue(s.color);
        u.stop_col[k * 3 + 2] = (float)RGBGetBValue(s.color);
    }

    std::vector<unsigned char> rgba(leds.size() * 4u);
    if(!script_gpu::Run(source, u, leds.front().v, (int)leds.size(), rgba.data()))
    {
        return false;
    }
    colors->resize(leds.size());
    lit->resize(leds.size());
    for(size_t i = 0; i < leds.size(); ++i)
    {
        const unsigned char* px = rgba.data() + i * 4u;
        (*colors)[i] = ToRGBColor(px[0], px[1], px[2]);
        (*lit)[i] = px[3] > 127 ? 1 : 0;
    }
    return true;
}

} // namespace script

void SetActionScriptDirectory(const filesystem::path& dir)
{
    script::SetDirectory(dir);
}

void ReloadActionScripts()
{
    script::ReloadDirectory();
}

bool ActionScriptUsesWorld(const std::string& id)
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

std::string ClipFileId(const Clip& clip)
{
    return script::FileId(clip);
}
} // namespace Action
