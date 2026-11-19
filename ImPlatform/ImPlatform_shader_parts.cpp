// dear imgui: Graphics API Abstraction - shader parts
// Builds a complete shader for a backend from ImPlatform_ShaderParts: the application writes its parameter block, extra
// varyings, textures, helper functions and the vertex / pixel bodies once (HLSL syntax plus the IMP_* macros), and the
// entry points, inputs, outputs and resource declarations are generated per shading language.

#include "ImPlatform_Internal.h"

#include <stdio.h>
#include <string.h>

// One declaration of a ';' separated list ("float4 col0; float2 uv[2];"): its type and name (the array suffix kept in
// 'decl' only). A leading "nointerpolation" sets 'flat' and a trailing ": SEMANTIC" fills 'semantic'; neither is in
// 'type' or 'decl'.
struct ImPlatform_PartsDecl
{
    char type[32];
    char name[64];
    char decl[160];
    char semantic[32];
    bool flat;
};

static int ImPlatform_ParseDecls(const char* list, ImPlatform_PartsDecl* out, int max)
{
    // The list without its // comments (a comment can hold a ';').
    ImVector<char> text;
    for (const char* s = list; s && *s; s++)
    {
        if (s[0] == '/' && s[1] == '/')
            while (s[1] && s[1] != '\n')
                s++;
        else
            text.push_back(*s);
    }
    text.push_back(0);
    int count = 0;
    for (const char* p = text.Data; *p && count < max; )
    {
        const char* end = strchr(p, ';');
        if (!end)
            end = p + strlen(p);
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
            p++;
        const char* e = end;
        const bool flat = strncmp(p, "nointerpolation", 15) == 0 && (p[15] == ' ' || p[15] == '\t');
        if (flat)
            for (p += 15; p < end && (*p == ' ' || *p == '\t'); p++) {}
        const char* colon = (const char*)memchr(p, ':', (size_t)(end - p));
        const char* semantic = colon ? colon + 1 : end;
        while (semantic < end && (*semantic == ' ' || *semantic == '\t'))
            semantic++;
        const size_t semantic_len = strcspn(semantic, " \t\r\n;");
        if (colon)
            e = colon;
        while (e > p && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n'))
            e--;
        const char* space = e;
        while (space > p && space[-1] != ' ' && space[-1] != '\t')
            space--;
        if (space > p && (size_t)(e - p) < sizeof(out->decl) && semantic_len < sizeof(out->semantic))
        {
            ImPlatform_PartsDecl& d = out[count++];
            d.flat = flat;
            memcpy(d.semantic, semantic, colon ? semantic_len : 0);
            d.semantic[colon ? semantic_len : 0] = 0;
            const char* type_end = space;
            while (type_end > p && (type_end[-1] == ' ' || type_end[-1] == '\t'))
                type_end--;
            size_t type_len = (size_t)(type_end - p);
            type_len = type_len < sizeof(d.type) - 1 ? type_len : sizeof(d.type) - 1;
            memcpy(d.type, p, type_len);
            d.type[type_len] = 0;
            size_t name_len = strcspn(space, "[ \t;");
            name_len = name_len < sizeof(d.name) - 1 ? name_len : sizeof(d.name) - 1;
            memcpy(d.name, space, name_len);
            d.name[name_len] = 0;
            memcpy(d.decl, p, (size_t)(e - p));
            d.decl[e - p] = 0;
        }
        p = *end ? end + 1 : end;
    }
    return count;
}

// "NAME=VALUE;OTHER" (';' or new lines) -> #define lines; a line starting with '#' is kept as written.
static void ImPlatform_PartsDefines(ImGuiTextBuffer& out, const char* defines)
{
    for (const char* p = defines; p && *p; )
    {
        const char* end = p + strcspn(p, ";\n");
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r'))
            p++;
        const char* eq = (const char*)memchr(p, '=', (size_t)(end - p));
        if (p < end && *p == '#')
            out.appendf("%.*s\n", (int)(end - p), p);
        else if (p < end)
        {
            if (eq)
                out.appendf("#define %.*s %.*s\n", (int)(eq - p), p, (int)(end - eq - 1), eq + 1);
            else
                out.appendf("#define %.*s\n", (int)(end - p), p);
        }
        p = *end ? end + 1 : end;
    }
}

static const char* ImPlatform_PartsText(const char* s) { return s ? s : ""; }
static const char* ImPlatform_PartsTexture0(const ImPlatform_ShaderParts* parts) { return parts->textures[0] ? parts->textures[0] : "texture0"; }
static bool ImPlatform_PartsIs3D(const ImPlatform_ShaderParts* parts, int slot) { return (parts->textures_3d >> slot) & 1u; }

// HLSL: Direct3D 9 (vs_3_0 / ps_3_0) to 12. Entry points vs_main and ps_main.
static void ImPlatform_PartsHLSL(ImGuiTextBuffer& out, const ImPlatform_ShaderParts* parts, bool vertex,
                                 const ImPlatform_PartsDecl* varyings, int varying_count, const ImPlatform_PartsDecl* inputs, int input_count)
{
    out.append(
        "#define IMP_HLSL 1\n"
        "#define IMP_UNROLL [unroll]\n"
        "#define IMP_LOOP [loop]\n"
        "#define IMP_BRANCH [branch]\n"
        "#define IMP_SAMPLE(t, uv) t.Sample(imp_sampler, uv)\n"
        "#define IMP_SAMPLE_LEVEL(t, uv, lod) t.SampleLevel(imp_sampler, uv, lod)\n"
        "#define IMP_LOAD(t, xy) t.Load(int3(xy, 0))\n"
        "#define IMP_MUL(m, v) mul(m, v)\n"
        "#define IMP_CAST(type, x) ((type)(x))\n"
        "#define IMP_OUT(type) out type\n#define IMP_INOUT(type) inout type\n");
    ImPlatform_PartsDefines(out, parts->defines);
    out.append("cbuffer ImpVertex : register(b0) { float4x4 ProjMtx; };\n");
    if (parts->params && parts->params[0])
        out.appendf("cbuffer ImpParams : register(b1) {\n%s\n};\n", parts->params);
    out.appendf("SamplerState imp_sampler : register(s0);\nTexture2D %s : register(t0);\n", ImPlatform_PartsTexture0(parts));
    for (int slot = 1; slot < 9; slot++)
        if (parts->textures[slot])
            out.appendf("%s %s : register(t%d);\n", ImPlatform_PartsIs3D(parts, slot) ? "Texture3D" : "Texture2D", parts->textures[slot], slot);
    out.append("struct ImpVaryings {\n    float4 position : SV_POSITION;\n    float4 col : COLOR0;\n    float2 uv : TEXCOORD0;\n");
    for (int i = 0; i < varying_count; i++)
        out.appendf("    %s%s : TEXCOORD%d;\n", varyings[i].flat ? "nointerpolation " : "", varyings[i].decl, i + 1);
    out.appendf("};\n%s\n", ImPlatform_PartsText(parts->functions));
    if (vertex && input_count)
    {
        out.append("struct ImpVertexInput {\n");
        for (int i = 0; i < input_count; i++)
            out.appendf("    %s : %s;\n", inputs[i].decl, inputs[i].semantic);
        out.append("};\nImpVaryings vs_main(ImpVertexInput imp_in)\n{\n");
        for (int i = 0; i < input_count; i++)
            out.appendf("    %s in_%s = imp_in.%s;\n", inputs[i].type, inputs[i].name, inputs[i].name);
        out.append("    ImpVaryings v;\n    v.col = float4(0.0, 0.0, 0.0, 0.0);\n    v.uv = float2(0.0, 0.0);\n");
        for (int i = 0; i < varying_count; i++)
            out.appendf("    v.%s = (%s)0;\n", varyings[i].name, varyings[i].type);
        out.appendf("    float4 position = float4(0.0, 0.0, 0.0, 1.0);\n    {\n%s\n    }\n"
                    "    v.position = position;\n    return v;\n}\n", ImPlatform_PartsText(parts->vs_body));
    }
    else if (vertex)
    {
        out.append(
            "struct ImpVertexInput { float2 pos : POSITION; float2 uv : TEXCOORD0; float4 col : COLOR0; };\n"
            "ImpVaryings vs_main(ImpVertexInput imp_in)\n{\n"
            "    float2 in_pos = imp_in.pos;\n    float2 in_uv = imp_in.uv;\n    float4 in_col = imp_in.col;\n"
            "    ImpVaryings v;\n    v.col = in_col;\n    v.uv = in_uv;\n");
        for (int i = 0; i < varying_count; i++)
            out.appendf("    v.%s = (%s)0;\n", varyings[i].name, varyings[i].type);
        out.appendf("    float4 position = mul(ProjMtx, float4(in_pos, 0.0, 1.0));\n    {\n%s\n    }\n"
                    "    v.position = position;\n    return v;\n}\n", ImPlatform_PartsText(parts->vs_body));
    }
    else
    {
        out.appendf("float4 ps_main(ImpVaryings v) : SV_Target\n{\n"
                    "    float4 col_out = v.col * %s.Sample(imp_sampler, v.uv);\n    {\n%s\n    }\n"
                    "    return col_out;\n}\n", ImPlatform_PartsTexture0(parts), ImPlatform_PartsText(parts->ps_body));
    }
}

// GLSL 330 (OpenGL 3.3+, macOS included): HLSL names mapped by the preprocessor. Entry point main.
static void ImPlatform_PartsGLSL(ImGuiTextBuffer& out, const ImPlatform_ShaderParts* parts, bool vertex,
                                 const ImPlatform_PartsDecl* varyings, int varying_count, const ImPlatform_PartsDecl* inputs, int input_count)
{
    out.append(
        "#version 330 core\n"
        "#define IMP_GLSL 1\n"
        "#define float2 vec2\n#define float3 vec3\n#define float4 vec4\n"
        "#define int2 ivec2\n#define int3 ivec3\n#define int4 ivec4\n"
        "#define uint2 uvec2\n#define uint3 uvec3\n#define uint4 uvec4\n"
        "#define bool2 bvec2\n#define bool3 bvec3\n#define bool4 bvec4\n"
        "#define float2x2 mat2\n#define float3x3 mat3\n#define float4x4 mat4\n"
        "#define lerp mix\n#define frac fract\n#define rsqrt inversesqrt\n"
        "#define saturate(x) clamp((x), 0.0, 1.0)\n#define rcp(x) (1.0 / (x))\n#define atan2(y, x) atan((y), (x))\n"
        "#define fmod(x, y) ((x) - (y) * trunc((x) / (y)))\n#define mul(a, b) ((a) * (b))\n#define static\n"
        "#define mad(a, b, c) ((a) * (b) + (c))\n#define clip(x) if ((x) < 0.0) discard\n"
        "#define asuint floatBitsToUint\n#define asint floatBitsToInt\n#define asfloat uintBitsToFloat\n"
        // unpackHalf2x16 needs GLSL 4.20: the half to float conversion by hand (zero, subnormals, infinity and NaN kept).
        "float f16tof32(uint h) { uint s = (h & 0x8000u) << 16u; uint e = (h >> 10u) & 0x1Fu; uint m = h & 0x3FFu;"
        " if (e == 0u) return (s != 0u ? -1.0 : 1.0) * float(m) * 5.9604645e-8;"
        " return uintBitsToFloat(s | (e == 31u ? 0x7F800000u : (e + 112u) << 23u) | (m << 13u)); }\n"
        "#define IMP_UNROLL\n#define IMP_LOOP\n#define IMP_BRANCH\n"
        "#define IMP_SAMPLE(t, uv) texture(t, uv)\n"
        "#define IMP_SAMPLE_LEVEL(t, uv, lod) textureLod(t, uv, lod)\n"
        "#define IMP_LOAD(t, xy) texelFetch(t, ivec2(xy), 0)\n"
        "#define IMP_MUL(m, v) ((m) * (v))\n"
        "#define IMP_CAST(type, x) type(x)\n"
        "#define IMP_OUT(type) out type\n#define IMP_INOUT(type) inout type\n");
    // Derivatives exist in fragment shaders only; the helper functions are compiled in both stages, so the vertex stage
    // gets a zero of the argument's type.
    out.append(vertex ? "#define ddx(x) ((x) - (x))\n#define ddy(x) ((x) - (x))\n" : "#define ddx dFdx\n#define ddy dFdy\n");
    ImPlatform_PartsDefines(out, parts->defines);
    // The backend binds these blocks by name to bindings 0 and 1 (the binding qualifier needs GLSL 4.20). Render passes
    // mirror the vertices (render textures are stored bottom-up; imp_mirror.x is 1, imp_mirror.y the mirror line): in_pos
    // is unmirrored and ProjMtx mirrors again, so the bodies work in Dear ImGui coordinates.
    out.append("layout(binding = 0, std140) uniform ImpVertex { mat4 imp_proj; vec4 imp_mirror; };\n"
               "#define ProjMtx (imp_proj * mat4(1.0, 0.0, 0.0, 0.0, 0.0, 1.0 - 2.0 * imp_mirror.x, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, imp_mirror.x * imp_mirror.y, 0.0, 1.0))\n");
    if (parts->params && parts->params[0])
        out.appendf("layout(binding = 1, std140) uniform ImpParams {\n%s\n};\n", parts->params);
    out.appendf("uniform sampler2D %s;\n", ImPlatform_PartsTexture0(parts));
    for (int slot = 1; slot < 9; slot++)
        if (parts->textures[slot])
            out.appendf("uniform %s %s;\n", ImPlatform_PartsIs3D(parts, slot) ? "sampler3D" : "sampler2D", parts->textures[slot]);
    out.appendf("%s ImpVaryings {\n    vec4 col;\n    vec2 uv;\n", vertex ? "out" : "in");
    for (int i = 0; i < varying_count; i++)
        out.appendf("    %s%s;\n", varyings[i].flat ? "flat " : "", varyings[i].decl);
    out.appendf("} v;\n%s\n", ImPlatform_PartsText(parts->functions));
    if (vertex && input_count)
    {
        // Attribute i of the vertex buffer at location i (ImPlatform_CreateVertexInputLayout).
        for (int i = 0; i < input_count; i++)
            out.appendf("layout(location = %d) in %s imp_in_%s;\n", i, inputs[i].type, inputs[i].name);
        out.append("void main()\n{\n");
        for (int i = 0; i < input_count; i++)
            out.appendf("    %s in_%s = imp_in_%s;\n", inputs[i].type, inputs[i].name, inputs[i].name);
        out.append("    v.col = float4(0.0);\n    v.uv = float2(0.0);\n");
        for (int i = 0; i < varying_count; i++)
            out.appendf("    v.%s = %s(0);\n", varyings[i].name, varyings[i].type);
        out.appendf("    float4 position = float4(0.0, 0.0, 0.0, 1.0);\n    {\n%s\n    }\n"
                    "    gl_Position = position;\n}\n", ImPlatform_PartsText(parts->vs_body));
    }
    else if (vertex)
    {
        out.append(
            "in vec2 Position;\nin vec2 UV;\nin vec4 Color;\n"
            "void main()\n{\n    float2 in_pos = float2(Position.x, imp_mirror.x != 0.0 ? imp_mirror.y - Position.y : Position.y);\n    float2 in_uv = UV;\n    float4 in_col = Color;\n"
            "    v.col = in_col;\n    v.uv = in_uv;\n");
        for (int i = 0; i < varying_count; i++)
            out.appendf("    v.%s = %s(0);\n", varyings[i].name, varyings[i].type);
        out.appendf("    float4 position = mul(ProjMtx, float4(in_pos, 0.0, 1.0));\n    {\n%s\n    }\n"
                    "    gl_Position = position;\n}\n", ImPlatform_PartsText(parts->vs_body));
    }
    else
    {
        out.appendf("layout(location = 0) out vec4 imp_out;\n"
                    "void main()\n{\n    float4 col_out = v.col * texture(%s, v.uv);\n    {\n%s\n    }\n"
                    "    imp_out = col_out;\n}\n", ImPlatform_PartsTexture0(parts), ImPlatform_PartsText(parts->ps_body));
    }
}

// MSL (Metal): the helper functions and bodies are members of a context struct that inherits the parameter block, so
// they reach the parameters, ProjMtx and the textures by name as in HLSL. Buffer indices are the registers plus one
// (Dear ImGui's vertices are at buffer 0). Entry points vs_main and ps_main.
static void ImPlatform_PartsMSL(ImGuiTextBuffer& out, const ImPlatform_ShaderParts* parts, bool vertex,
                                const ImPlatform_PartsDecl* varyings, int varying_count, const ImPlatform_PartsDecl* inputs, int input_count)
{
    out.append(
        "#include <metal_stdlib>\nusing namespace metal;\n"
        "#define IMP_MSL 1\n"
        "#define lerp mix\n#define frac fract\n"
        "#define rcp(x) (1.0f / (x))\n#define mul(a, b) ((a) * (b))\n#define static\n"
        "#define mad(a, b, c) ((a) * (b) + (c))\n#define discard discard_fragment()\n#define clip(x) if ((x) < 0.0) discard_fragment()\n"
        "uint asuint(float x) { return as_type<uint>(x); }\nuint2 asuint(float2 x) { return as_type<uint2>(x); }\nuint4 asuint(float4 x) { return as_type<uint4>(x); }\n"
        "int asint(float x) { return as_type<int>(x); }\nfloat asfloat(uint x) { return as_type<float>(x); }\n"
        "float2 asfloat(uint2 x) { return as_type<float2>(x); }\nfloat4 asfloat(uint4 x) { return as_type<float4>(x); }\n"
        "float f16tof32(uint h) { return float(as_type<half>(ushort(h))); }\n"
        "#define IMP_UNROLL\n#define IMP_LOOP\n#define IMP_BRANCH\n"
        "#define IMP_SAMPLE(t, uv) t.sample(imp_sampler, uv)\n"
        "#define IMP_SAMPLE_LEVEL(t, uv, lod) t.sample(imp_sampler, uv, level(lod))\n"
        "#define IMP_LOAD(t, xy) t.read(uint2(xy))\n"
        "#define IMP_MUL(m, v) ((m) * (v))\n"
        "#define IMP_CAST(type, x) type(x)\n"
        "#define IMP_OUT(type) thread type&\n#define IMP_INOUT(type) thread type&\n"
        // HLSL promotes scalars to vectors in these functions: the scalar / vector mixes, forwarded to MSL.
        "template<typename T, int N, typename S, typename = enable_if_t<is_scalar<S>::value>> vec<T, N> mix(vec<T, N> a, vec<T, N> b, S t) { return mix(a, b, vec<T, N>(T(t))); }\n"
        "template<typename T, int N, typename S, typename = enable_if_t<is_scalar<S>::value>> vec<T, N> min(vec<T, N> a, S b) { return min(a, vec<T, N>(T(b))); }\n"
        "template<typename T, int N, typename S, typename = enable_if_t<is_scalar<S>::value>> vec<T, N> max(vec<T, N> a, S b) { return max(a, vec<T, N>(T(b))); }\n"
        "template<typename T, int N, typename S, typename = enable_if_t<is_scalar<S>::value>> vec<T, N> clamp(vec<T, N> x, S a, S b) { return clamp(x, vec<T, N>(T(a)), vec<T, N>(T(b))); }\n"
        "template<typename T, int N, typename S, typename = enable_if_t<is_scalar<S>::value>> vec<T, N> step(S e, vec<T, N> x) { return step(vec<T, N>(T(e)), x); }\n"
        "template<typename T, int N, typename S, typename = enable_if_t<is_scalar<S>::value>> vec<T, N> smoothstep(S a, S b, vec<T, N> x) { return smoothstep(vec<T, N>(T(a)), vec<T, N>(T(b)), x); }\n"
        "template<typename T, int N, typename S, typename = enable_if_t<is_scalar<S>::value>> vec<T, N> pow(vec<T, N> x, S y) { return pow(x, vec<T, N>(T(y))); }\n");
    // Derivatives exist in fragment functions only (as in GLSL): a zero of the argument's type in the vertex stage.
    out.append(vertex ? "#define ddx(x) ((x) - (x))\n#define ddy(x) ((x) - (x))\n" : "#define ddx dfdx\n#define ddy dfdy\n");
    ImPlatform_PartsDefines(out, parts->defines);
    const bool has_params = parts->params && parts->params[0];
    // ImpParams is the buffer layout (float3 is 16 bytes in MSL: packed_float3 keeps the HLSL / std140 offsets);
    // ImpParamsView is what the bodies see: the values, and the arrays as pointers into the buffer (not copied).
    ImPlatform_PartsDecl params[64];
    const int param_count = has_params ? ImPlatform_ParseDecls(parts->params, params, IM_ARRAYSIZE(params)) : 0;
    out.append("struct ImpVertex { float4x4 ProjMtx; };\nstruct ImpParams {\n");
    for (int i = 0; i < param_count; i++)
        out.appendf("    %s%s;\n", strcmp(params[i].type, "float3") == 0 ? "packed_" : "", params[i].decl);
    out.append("};\nstruct ImpParamsView {\n");
    for (int i = 0; i < param_count; i++)
    {
        if (strchr(params[i].decl, '['))
            out.appendf("    constant %s%s* %s;\n", strcmp(params[i].type, "float3") == 0 ? "packed_" : "", params[i].type, params[i].name);
        else
            out.appendf("    %s;\n", params[i].decl);
    }
    out.append("};\nstruct ImpVaryings {\n    float4 position [[position]];\n    float4 col;\n    float2 uv;\n");
    for (int i = 0; i < varying_count; i++)
        out.appendf("    %s%s;\n", varyings[i].decl, varyings[i].flat ? " [[flat]]" : "");
    out.append("};\n");
    if (vertex && input_count)
    {
        out.append("struct ImpVertexInput {\n");
        for (int i = 0; i < input_count; i++)
            out.appendf("    %s [[attribute(%d)]];\n", inputs[i].decl, i);
        out.append("};\n");
    }
    else if (vertex)
        out.append("struct ImpVertexInput { float2 pos [[attribute(0)]]; float2 uv [[attribute(1)]]; float4 col [[attribute(2)]]; };\n");
    out.appendf("struct ImpContext : ImpVertex, ImpParamsView {\n    texture2d<float> %s;\n    sampler imp_sampler;\n", ImPlatform_PartsTexture0(parts));
    for (int slot = 1; slot < 9; slot++)
        if (parts->textures[slot])
            out.appendf("    %s<float> %s;\n", ImPlatform_PartsIs3D(parts, slot) ? "texture3d" : "texture2d", parts->textures[slot]);
    out.appendf("%s\n", ImPlatform_PartsText(parts->functions));
    if (vertex)
    {
        out.append("    ImpVaryings imp_vs(ImpVertexInput imp_in)\n    {\n");
        if (input_count)
        {
            for (int i = 0; i < input_count; i++)
                out.appendf("        %s in_%s = imp_in.%s;\n", inputs[i].type, inputs[i].name, inputs[i].name);
            out.append("        ImpVaryings v;\n        v.col = float4(0.0);\n        v.uv = float2(0.0);\n");
        }
        else
            out.append("        float2 in_pos = imp_in.pos;\n        float2 in_uv = imp_in.uv;\n        float4 in_col = imp_in.col;\n"
                       "        ImpVaryings v;\n        v.col = in_col;\n        v.uv = in_uv;\n");
        for (int i = 0; i < varying_count; i++)
            out.appendf("        v.%s = %s(0);\n", varyings[i].name, varyings[i].type);
        out.appendf("        float4 position = %s;\n        {\n%s\n        }\n"
                    "        v.position = position;\n        return v;\n    }\n};\n",
                    input_count ? "float4(0.0, 0.0, 0.0, 1.0)" : "ProjMtx * float4(in_pos, 0.0, 1.0)", ImPlatform_PartsText(parts->vs_body));
        out.append("vertex ImpVaryings vs_main(ImpVertexInput imp_in [[stage_in]], constant ImpVertex& imp_vertex [[buffer(1)]]");
    }
    else
    {
        out.appendf("    float4 imp_ps(ImpVaryings v)\n    {\n        float4 col_out = v.col * %s.sample(imp_sampler, v.uv);\n"
                    "        {\n%s\n        }\n        return col_out;\n    }\n};\n", ImPlatform_PartsTexture0(parts), ImPlatform_PartsText(parts->ps_body));
        out.append("fragment float4 ps_main(ImpVaryings v [[stage_in]], constant ImpVertex& imp_vertex [[buffer(1)]]");
    }
    if (has_params)
        out.append(", constant ImpParams& imp_params [[buffer(2)]]");
    out.appendf(", texture2d<float> %s [[texture(0)]]", ImPlatform_PartsTexture0(parts));
    for (int slot = 1; slot < 9; slot++)
        if (parts->textures[slot])
            out.appendf(", %s<float> %s [[texture(%d)]]", ImPlatform_PartsIs3D(parts, slot) ? "texture3d" : "texture2d", parts->textures[slot], slot);
    // Values are copied member by member (loads from constant memory); arrays point into the buffer.
    out.append(", sampler imp_sampler [[sampler(0)]])\n{\n    ImpContext ctx;\n    ctx.ProjMtx = imp_vertex.ProjMtx;\n");
    for (int i = 0; i < param_count; i++)
        out.appendf("    ctx.%s = imp_params.%s;\n", params[i].name, params[i].name);
    out.appendf("    ctx.%s = %s;\n    ctx.imp_sampler = imp_sampler;\n", ImPlatform_PartsTexture0(parts), ImPlatform_PartsTexture0(parts));
    for (int slot = 1; slot < 9; slot++)
        if (parts->textures[slot])
            out.appendf("    ctx.%s = %s;\n", parts->textures[slot], parts->textures[slot]);
    out.append(vertex ? "    return ctx.imp_vs(imp_in);\n}\n" : "    return ctx.imp_ps(v);\n}\n");
}

IMPLATFORM_API bool ImPlatform_ParseShaderParts(char* text, ImPlatform_ShaderParts* parts)
{
    if (!text || !parts)
        return false;
    static const char* const names[] = { "params", "inputs", "varyings", "textures", "defines", "functions", "vs", "ps" };
    const char** fields[] = { &parts->params, &parts->vertex_inputs, &parts->varyings, NULL, &parts->defines, &parts->functions, &parts->vs_body, &parts->ps_body };
    char* textures = NULL;
    bool found = false;
    for (char* line = text; line && *line; )
    {
        char* next = strchr(line, '\n');
        if (strncmp(line, "//@", 3) == 0)
        {
            const size_t len = strcspn(line + 3, " \t\r\n");
            for (int i = 0; i < IM_ARRAYSIZE(names); i++)
                if (len == strlen(names[i]) && strncmp(line + 3, names[i], len) == 0)
                {
                    *line = 0;      // Ends the previous section
                    if (fields[i])
                        *fields[i] = next ? next + 1 : "";
                    else
                        textures = next ? next + 1 : NULL;
                    found = true;
                }
        }
        line = next ? next + 1 : NULL;
    }
    // Texture lines "<slot> <name>" or "<slot> <name> 3d": the names are terminated in place.
    for (char* line = textures; line && *line; )
    {
        char* next = strchr(line, '\n');
        char* end = NULL;
        const long slot = strtol(line, &end, 10);
        if (end != line && slot >= 0 && slot < 9)
        {
            while (*end == ' ' || *end == '\t')
                end++;
            const size_t len = strcspn(end, " \t\r\n");
            if (len)
            {
                const char* kind = end + len;
                while (*kind == ' ' || *kind == '\t')
                    kind++;
                if (strncmp(kind, "3d", 2) == 0)
                    parts->textures_3d |= 1u << slot;
                else
                    parts->textures_3d &= ~(1u << slot);
                parts->textures[slot] = end;
                end[len] = 0;
            }
        }
        line = next ? next + 1 : NULL;
    }
    return found;
}
IMPLATFORM_API int ImPlatform_GenerateShaderSource(const ImPlatform_ShaderParts* parts, ImPlatform_ShaderStage stage,
                                                   ImPlatform_ShaderFormat format, char* out, int out_size)
{
    if (!parts || (stage != ImPlatform_ShaderStage_Vertex && stage != ImPlatform_ShaderStage_Fragment))
        return -1;
    ImPlatform_PartsDecl varyings[8];
    const int varying_count = ImPlatform_ParseDecls(parts->varyings, varyings, IM_ARRAYSIZE(varyings));
    ImPlatform_PartsDecl inputs[16];
    const int input_count = ImPlatform_ParseDecls(parts->vertex_inputs, inputs, IM_ARRAYSIZE(inputs));
    for (int i = 0; i < input_count; i++)
        if (!inputs[i].semantic[0])
            snprintf(inputs[i].semantic, sizeof(inputs[i].semantic), "TEXCOORD%d", i);
    const bool vertex = stage == ImPlatform_ShaderStage_Vertex;
    ImGuiTextBuffer text;
    switch (format)
    {
    case ImPlatform_ShaderFormat_HLSL: ImPlatform_PartsHLSL(text, parts, vertex, varyings, varying_count, inputs, input_count); break;
    case ImPlatform_ShaderFormat_GLSL: ImPlatform_PartsGLSL(text, parts, vertex, varyings, varying_count, inputs, input_count); break;
    case ImPlatform_ShaderFormat_MSL:  ImPlatform_PartsMSL(text, parts, vertex, varyings, varying_count, inputs, input_count); break;
    default: return -1;
    }
    if (out && out_size > 0)
    {
        const int n = text.size() < out_size - 1 ? text.size() : out_size - 1;
        memcpy(out, text.c_str(), (size_t)n);
        out[n] = 0;
    }
    return text.size();
}

#if IMPLATFORM_GFX_SUPPORT_SHADER_PARTS && IM_CURRENT_GFX != IM_GFX_CUSTOM
IMPLATFORM_API ImPlatform_Shader ImPlatform_CreateShaderFromParts(const ImPlatform_ShaderParts* parts, ImPlatform_ShaderStage stage)
{
#if IM_CURRENT_GFX == IM_GFX_OPENGL3
    const ImPlatform_ShaderFormat format = ImPlatform_ShaderFormat_GLSL;
#elif IM_CURRENT_GFX == IM_GFX_METAL
    const ImPlatform_ShaderFormat format = ImPlatform_ShaderFormat_MSL;
#else
    const ImPlatform_ShaderFormat format = ImPlatform_ShaderFormat_HLSL;
#endif
    const int size = ImPlatform_GenerateShaderSource(parts, stage, format, NULL, 0);
    if (size < 0)
        return NULL;
    char* source = (char*)IM_ALLOC((size_t)size + 1);
    ImPlatform_GenerateShaderSource(parts, stage, format, source, size + 1);
    ImPlatform_ShaderDesc desc = {};
    desc.stage = stage;
    desc.format = format;
    desc.source_code = source;
    desc.entry_point = format == ImPlatform_ShaderFormat_GLSL ? "main" : stage == ImPlatform_ShaderStage_Vertex ? "vs_main" : "ps_main";
    desc.cache_key = parts->cache_key;
    desc.compile_flags = parts->compile_flags;
    ImPlatform_Shader shader = ImPlatform_CreateShader(&desc);
    if (!shader)
        fprintf(stderr, "[ImPlatform] shader parts: the generated %s shader did not compile:\n%s\n",
                stage == ImPlatform_ShaderStage_Vertex ? "vertex" : "pixel", source);
    IM_FREE(source);
    return shader;
}
#endif
