
#ifdef _WIN32
extern "C" {
	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>

// Platform and Graphics API are defined by the project configuration via:
// IM_CONFIG_PLATFORM and IM_CONFIG_GFX preprocessor defines
//
// Available configurations:
//   Win32_DX9        - Windows + DirectX 9
//   Win32_DX10       - Windows + DirectX 10
//   Win32_DX11       - Windows + DirectX 11
//   Win32_DX12       - Windows + DirectX 12
//   Win32_OpenGL3    - Windows + OpenGL 3
//   GLFW_OpenGL3     - GLFW + OpenGL 3
//   GLFW_Vulkan      - GLFW + Vulkan
//   GLFW_WebGPU      - GLFW + WebGPU (Dawn/wgpu-native)

#ifndef IM_CONFIG_PLATFORM
#error "IM_CONFIG_PLATFORM must be defined by project configuration"
#endif

#ifndef IM_CONFIG_GFX
#error "IM_CONFIG_GFX must be defined by project configuration"
#endif

// Define platform and graphics API from project configuration (CMake already defines them for its static library)
#ifndef IM_CURRENT_PLATFORM
#define IM_CURRENT_PLATFORM IM_CONFIG_PLATFORM
#endif
#ifndef IM_CURRENT_GFX
#define IM_CURRENT_GFX IM_CONFIG_GFX
#endif

// Define implementation to include the backend code
#define IMPLATFORM_IMPLEMENTATION

#include <ImPlatform.h>

#ifdef __EMSCRIPTEN__
#include "../imgui/examples/libs/emscripten/emscripten_mainloop_stub.h"
#endif

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

// DPI change callback - rescale ImGui style when DPI changes at runtime
static void OnDpiChanged(float new_scale, void* /*user_data*/)
{
	ImGuiStyle& style = ImGui::GetStyle();
	style = ImGuiStyle(); // Reset to defaults
	style.ScaleAllSizes(new_scale);
	style.FontScaleDpi = new_scale;
}

#define IM_HAS_STBI 0

#if IM_HAS_STBI
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#endif

// ImPlatform_ReadTexture where the backend has it (IMPLATFORM_GFX_SUPPORT_READBACK); the tests skip without it.
static bool DemoReadTexture( ImTextureID texture, unsigned int x, unsigned int y, unsigned int w, unsigned int h, void* px, size_t size )
{
#if IMPLATFORM_GFX_SUPPORT_READBACK
	return ImPlatform_ReadTexture( texture, x, y, w, h, px, size );
#else
	(void)texture; (void)x; (void)y; (void)w; (void)h; (void)px; (void)size;
	return false;
#endif
}

#ifdef IMGUI_HAS_TEXTURES
// Render passes: a draw list rendered into a render texture before the frame (ImPlatform_QueueRenderPass).
struct RenderPassScene
{
	ImDrawList*	List = nullptr;
	ImDrawData	Data;
	ImTextureID	Target = 0;
};

static void DestroyRenderPassScene( RenderPassScene& s )
{
	if ( s.List )
		IM_DELETE( s.List );
	if ( s.Target )
		ImPlatform_DestroyTexture( s.Target );
	s = RenderPassScene();
}

static bool QueueRenderPassScene( RenderPassScene& s, int size, void ( *draw )( ImDrawList* ) )
{
	if ( !s.Target )
	{
		ImPlatform_TextureDesc desc = ImPlatform_TextureDesc_Default( size, size );
		desc.format = ImPlatform_GetRenderPassFormat();
		s.Target = ImPlatform_CreateRenderTexture( &desc );
	}
	if ( !s.List )
		s.List = IM_NEW( ImDrawList )( ImGui::GetDrawListSharedData() );
	s.List->_ResetForNewFrame();
	s.List->_SetPixelDensity( 1.0f );	// Set by Dear ImGui for window draw lists only (unset: AddPolyline() draws nothing).
	s.List->PushClipRect( ImVec2( 0, 0 ), ImVec2( (float)size, (float)size ) );
	s.List->PushTexture( ImGui::GetIO().Fonts->TexRef );
	draw( s.List );
	s.List->PopTexture();
	s.List->PopClipRect();
	s.Data.Clear();
	s.Data.Valid = true;
	s.Data.AddDrawList( s.List );
	s.Data.CmdListsCount = s.Data.CmdLists.Size;
	s.Data.DisplayPos = ImVec2( 0, 0 );
	s.Data.DisplaySize = ImVec2( (float)size, (float)size );
	s.Data.FramebufferScale = ImVec2( 1, 1 );
	return s.Target && ImPlatform_QueueRenderPass( s.Target, &s.Data, true );
}

// Translucent overlapping shapes and text over transparent: what a texture of a vector drawing holds.
static void DrawRenderPassScene( ImDrawList* dl, ImVec2 o )
{
	dl->AddCircleFilled( ImVec2( o.x + 44, o.y + 44 ), 30, IM_COL32( 255, 80, 80, 170 ) );
	dl->AddCircleFilled( ImVec2( o.x + 84, o.y + 44 ), 30, IM_COL32( 80, 160, 255, 170 ) );
	dl->AddCircleFilled( ImVec2( o.x + 64, o.y + 78 ), 30, IM_COL32( 90, 230, 120, 170 ) );
	dl->AddLine( ImVec2( o.x + 8, o.y + 120 ), ImVec2( o.x + 120, o.y + 100 ), IM_COL32( 255, 255, 255, 120 ), 3.0f );
	dl->AddText( ImVec2( o.x + 8, o.y + 2 ), IM_COL32( 255, 255, 255, 255 ), "pass" );
}

// The scene rendered into a texture (left, drawn with premultiplied alpha) and drawn directly (right): they match.
static RenderPassScene g_RenderPassDemo;
static void RenderPassDemo()
{
	RenderPassScene& scene = g_RenderPassDemo;
	ImGui::Text( "Render pass (ImPlatform_QueueRenderPass): texture | direct" );
	if ( !QueueRenderPassScene( scene, 128, []( ImDrawList* dl ) { DrawRenderPassScene( dl, ImVec2( 0, 0 ) ); } ) )
	{
		ImGui::TextDisabled( "Not supported by this backend." );
		return;
	}
	ImPlatform_PushPremultipliedAlpha( nullptr );
	ImGui::Image( scene.Target, ImVec2( 128, 128 ) );
	ImPlatform_PopPremultipliedAlpha( nullptr );
	ImGui::SameLine();
	const ImVec2 p = ImGui::GetCursorScreenPos();
	ImGui::Dummy( ImVec2( 128, 128 ) );
	DrawRenderPassScene( ImGui::GetWindowDrawList(), p );
}

#ifndef __EMSCRIPTEN__
// --render-pass-test: an opaque red square, a half transparent blue square, transparent elsewhere, read back.
// Checks the clear, the orientation (top-left origin) and premultiplied colors. Exit code: 0 pass, 1 fail, 2 unsupported.
static RenderPassScene g_RenderPassTest;
static void DrawRenderPassTest( ImDrawList* dl )
{
	dl->AddRectFilled( ImVec2( 0, 0 ), ImVec2( 32, 32 ), IM_COL32( 255, 0, 0, 255 ) );
	dl->AddRectFilled( ImVec2( 32, 0 ), ImVec2( 64, 32 ), IM_COL32( 0, 0, 255, 128 ) );
}

// Next frame, a second pass draws gray then the first target with premultiplied alpha (the composite), and again at
// half opacity on the right (the vertex color premultiplied too).
static RenderPassScene g_RenderPassComposite;
static void DrawRenderPassComposite( ImDrawList* dl )
{
	dl->AddRectFilled( ImVec2( 0, 0 ), ImVec2( 128, 64 ), IM_COL32( 100, 100, 100, 255 ) );
	ImPlatform_PushPremultipliedAlpha( dl );
	dl->AddImage( g_RenderPassTest.Target, ImVec2( 0, 0 ), ImVec2( 64, 64 ) );
	dl->AddImage( g_RenderPassTest.Target, ImVec2( 64, 0 ), ImVec2( 128, 64 ), ImVec2( 0, 0 ), ImVec2( 1, 1 ), IM_COL32( 128, 128, 128, 128 ) );
	ImPlatform_PopPremultipliedAlpha( dl );
}

struct RenderPassProbe { int x, y; int rgba[ 4 ]; };
static int CheckRenderPassProbes( const char* what, const unsigned char* px, int stride, const RenderPassProbe* probes, int count, int channels )
{
	const bool bgra = ImPlatform_GetRenderPassFormat() == ImPlatform_PixelFormat_BGRA8;
	int bad = 0;
	for ( int i = 0; i < count; ++i )
	{
		const RenderPassProbe& p = probes[ i ];
		const unsigned char* q = px + ( p.y * stride + p.x ) * 4;
		const int got[ 4 ] = { bgra ? q[ 2 ] : q[ 0 ], q[ 1 ], bgra ? q[ 0 ] : q[ 2 ], q[ 3 ] };
		for ( int c = 0; c < channels; ++c )
			if ( abs( got[ c ] - p.rgba[ c ] ) > 3 )
			{
				printf( "FAIL %s at (%d, %d): got %d %d %d %d, expected %d %d %d %d\n", what, p.x, p.y, got[ 0 ], got[ 1 ], got[ 2 ], got[ 3 ], p.rgba[ 0 ], p.rgba[ 1 ], p.rgba[ 2 ], p.rgba[ 3 ] );
				++bad;
				break;
			}
	}
	return bad;
}

static int CheckRenderPassTest()
{
	static unsigned char px[ 128 * 128 * 4 ];
	if ( !g_RenderPassTest.Target || !DemoReadTexture( g_RenderPassTest.Target, 0, 0, 64, 64, px, sizeof( px ) ) )
	{
		printf( "SKIP: no readback of the pass target\n" );
		return 2;
	}
	// Pass target: the clear, the orientation and premultiplied colors.
	const RenderPassProbe target[] = { { 8, 8, { 255, 0, 0, 255 } }, { 48, 8, { 0, 0, 128, 128 } }, { 8, 48, { 0, 0, 0, 0 } }, { 48, 48, { 0, 0, 0, 0 } } };
	int bad = CheckRenderPassProbes( "target", px, 64, target, 4, 4 );
	// Composite over gray (100) with premultiplied alpha: blue at half coverage is 128 + 100 * 127 / 255.
	// At half opacity: red 128 + 100 * 127 / 255 = 178 (green, blue 50); blue at a quarter, 64 + 100 * 191 / 255 = 139.
	if ( DemoReadTexture( g_RenderPassComposite.Target, 0, 0, 128, 128, px, sizeof( px ) ) )
	{
		const RenderPassProbe composite[] = { { 8, 8, { 255, 0, 0, 255 } }, { 48, 8, { 50, 50, 178, 255 } }, { 8, 48, { 100, 100, 100, 255 } },
			{ 72, 8, { 178, 50, 50, 255 } }, { 112, 8, { 75, 75, 139, 255 } }, { 72, 48, { 100, 100, 100, 255 } } };
		bad += CheckRenderPassProbes( "composite", px, 128, composite, 6, 4 );
	}
	else
		bad++;
	if ( !bad )
		printf( "PASS\n" );
	return bad ? 1 : 0;
}
#endif // __EMSCRIPTEN__
#endif

#if IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER
// Custom shaders are written once in HLSL (shaders/<name>.hlsl, entry points vs_main and ps_main) and compiled for every
// backend by shaders/compile_shaders: Direct3D (9 included) compiles the HLSL at run time, Vulkan takes SPIR-V, OpenGL
// GLSL 330, WebGPU WGSL and Metal MSL.
#if (IM_CURRENT_GFX == IM_GFX_VULKAN)
#include "shaders/arrow_sdf.spv.h"
#include "shaders/gradient.spv.h"
#include "shaders/volume_test.spv.h"
#define DEMO_SHADER_PROGRAM( name, shaders ) CreateDemoProgram( ImPlatform_ShaderFormat_SPIRV, name##_vs_spv, sizeof( name##_vs_spv ), name##_ps_spv, sizeof( name##_ps_spv ), "main", "main", shaders )
#elif (IM_CURRENT_GFX == IM_GFX_OPENGL3)
#include "shaders/arrow_sdf.glsl.h"
#include "shaders/gradient.glsl.h"
#include "shaders/volume_test.glsl.h"
#define DEMO_SHADER_PROGRAM( name, shaders ) CreateDemoProgram( ImPlatform_ShaderFormat_GLSL, name##_vs_glsl, 0, name##_ps_glsl, 0, "main", "main", shaders )
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX9) || (IM_CURRENT_GFX == IM_GFX_DIRECTX10) || (IM_CURRENT_GFX == IM_GFX_DIRECTX11) || (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
#include "shaders/arrow_sdf.hlsl.h"
#include "shaders/gradient.hlsl.h"
#include "shaders/volume_test.hlsl.h"
#define DEMO_SHADER_PROGRAM( name, shaders ) CreateDemoProgram( ImPlatform_ShaderFormat_HLSL, name##_hlsl, 0, name##_hlsl, 0, "vs_main", "ps_main", shaders )
#elif (IM_CURRENT_GFX == IM_GFX_WGPU)
#include "shaders/arrow_sdf.wgsl.h"
#include "shaders/gradient.wgsl.h"
#include "shaders/volume_test.wgsl.h"
#define DEMO_SHADER_PROGRAM( name, shaders ) CreateDemoProgram( ImPlatform_ShaderFormat_WGSL, name##_vs_wgsl, 0, name##_ps_wgsl, 0, "vs_main", "ps_main", shaders )
#elif (IM_CURRENT_GFX == IM_GFX_METAL)
#include "shaders/arrow_sdf.metal.h"
#include "shaders/gradient.metal.h"
#include "shaders/volume_test.metal.h"
#define DEMO_SHADER_PROGRAM( name, shaders ) CreateDemoProgram( ImPlatform_ShaderFormat_MSL, name##_vs_metal, 0, name##_ps_metal, 0, "vs_main", "ps_main", shaders )
#else
#define DEMO_SHADER_NONE	// No demo shader for this backend
#define DEMO_SHADER_PROGRAM( name, shaders ) nullptr
#endif

#ifndef DEMO_SHADER_NONE
// Creates both stages (kept in shaders[] to be destroyed) and the program; size 0: vs and ps are source text.
static ImPlatform_ShaderProgram CreateDemoProgram( ImPlatform_ShaderFormat format, const void* vs, size_t vs_size, const void* ps, size_t ps_size,
												   const char* vs_entry, const char* ps_entry, ImPlatform_Shader shaders[ 2 ] )
{
	for ( int i = 0; i < 2; ++i )
	{
		ImPlatform_ShaderDesc desc = {};
		desc.stage = i == 0 ? ImPlatform_ShaderStage_Vertex : ImPlatform_ShaderStage_Fragment;
		desc.format = format;
		desc.entry_point = i == 0 ? vs_entry : ps_entry;
		const void* code = i == 0 ? vs : ps;
		const size_t size = i == 0 ? vs_size : ps_size;
		if ( size )
		{
			desc.bytecode = code;
			desc.bytecode_size = (unsigned int)size;
		}
		else
			desc.source_code = (const char*)code;
		shaders[ i ] = ImPlatform_CreateShader( &desc );
	}
	return shaders[ 0 ] && shaders[ 1 ] ? ImPlatform_CreateShaderProgram( shaders[ 0 ], shaders[ 1 ] ) : nullptr;
}
#endif // DEMO_SHADER_NONE

#if defined( IMGUI_HAS_TEXTURES ) && !defined( __EMSCRIPTEN__ )
// --shader-test: the gradient shader (shaders/gradient.hlsl) drawn into a render pass target, read back: red to blue on
// the left half, blue to red on the right, the colors set from a draw callback before each half (one program drawn twice
// in a frame keeps each draw's parameters). Checks the custom shader path of each backend: compilation, b0 projection,
// b1 parameters. Exit code as above.
static RenderPassScene g_ShaderTest;
static ImPlatform_ShaderProgram g_ShaderTestProgram = nullptr;
static void SetShaderTestColors( const ImDrawList*, const ImDrawCmd* cmd )
{
	ImPlatform_SetShaderUniform( g_ShaderTestProgram, "pixelBuffer", cmd->UserCallbackData, sizeof( ImVec4 ) * 2 );
}
static void DrawShaderTest( ImDrawList* dl )
{
	static const ImVec4 colors[ 2 ][ 2 ] = { { ImVec4( 1, 0, 0, 1 ), ImVec4( 0, 0, 1, 1 ) }, { ImVec4( 0, 0, 1, 1 ), ImVec4( 1, 0, 0, 1 ) } };
	for ( int i = 0; i < 2; i++ )
	{
		dl->AddCallback( SetShaderTestColors, (void*)colors[ i ] );
		ImPlatform_BeginCustomShader( dl, g_ShaderTestProgram );
		dl->AddImage( ImGui::GetIO().Fonts->TexRef, ImVec2( 32.0f * i, 0 ), ImVec2( 32.0f * i + 32.0f, 64 ) );
		ImPlatform_EndCustomShader( dl );
	}
}

static int CheckShaderTest()
{
	static unsigned char px[ 64 * 64 * 4 ];
	if ( !DemoReadTexture( g_ShaderTest.Target, 0, 0, 64, 64, px, sizeof( px ) ) )
	{
		printf( "SKIP: no readback of the pass target\n" );
		return 2;
	}
	// uv.y at the centers of rows 0, 32 and 63: 0.008, 0.508, 0.992
	const RenderPassProbe probes[] = { { 16, 0, { 253, 0, 2, 255 } }, { 16, 32, { 125, 0, 130, 255 } }, { 16, 63, { 2, 0, 253, 255 } },
									   { 48, 0, { 2, 0, 253, 255 } }, { 48, 63, { 253, 0, 2, 255 } } };
	const int bad = CheckRenderPassProbes( "gradient", px, 64, probes, 5, 4 );
	if ( !bad )
		printf( "PASS\n" );
	return bad ? 1 : 0;
}

// --shader-parts-test: a shader written once as parts (ImPlatform_CreateShaderFromParts) drawn into a render pass and
// read back: its parameter block is read in both stages (the vertex body moves the quad down by Offset), with a
// varying, helper functions, a define, a named texture t1, IMP_UNROLL and IMP_CAST. The gradient of --shader-test, its
// blue halved by t1.
static RenderPassScene g_PartsTest;
static ImPlatform_ShaderProgram g_PartsProgram = nullptr;
static ImPlatform_Shader g_PartsShaders[ 2 ] = {};
static ImTextureID g_PartsTint = 0;
#if IMPLATFORM_GFX_SUPPORT_SHADER_PARTS && IMPLATFORM_GFX_SUPPORT_SHADER_TEXTURES
static void DrawPartsTest( ImDrawList* dl )
{
	ImPlatform_BeginCustomShader( dl, g_PartsProgram );
	dl->AddImage( ImGui::GetIO().Fonts->TexRef, ImVec2( 0, -8 ), ImVec2( 64, 56 ) );
	ImPlatform_EndCustomShader( dl );
}
#endif

static bool CreatePartsTest()
{
#if IMPLATFORM_GFX_SUPPORT_SHADER_PARTS && IMPLATFORM_GFX_SUPPORT_SHADER_TEXTURES
	// The parts as one text, as a shader file would hold them (ImPlatform_ParseShaderParts splits it in place).
	static char text[] =
		"// Gradient moved by Offset in the vertex stage, tinted by t1.\n"
		"//@params\n"
		"float4 ColorStart; // red; a comment may hold a semicolon\n"
		"float4 ColorEnd; float4 Offset;\n"
		"//@varyings\n"
		"float2 world;\n"
		"//@textures\n"
		"1 tint\n"
		"//@defines\n"
		"TINT_SCALE=1.0\n"
		"BITS=1\n"
		"//@functions\n"
		"float4 Gradient(float t) { return lerp(ColorStart, ColorEnd, saturate(t)); }\n"
		"float4 Tint(float2 uv) { return IMP_SAMPLE(tint, uv) * TINT_SCALE; }\n"
		"//@vs\n"
		"position = mul(ProjMtx, float4(in_pos + Offset.xy, 0.0, 1.0));\n"
		"v.world = in_pos + Offset.xy;\n"
		"//@ps\n"
		"float sum = 0.0;\n"
		"IMP_UNROLL for (int i = 0; i < 4; i++) sum += 0.25;\n"
		"#if BITS\n"
		"sum *= asfloat(asuint(1.0)) * f16tof32(0x3C00u);\n"	// both 1.0
		"#endif\n"
		"col_out = Gradient(v.world.y / 64.0) * Tint(v.uv) * IMP_CAST(float, IMP_CAST(int, sum + 0.5));\n";
	ImPlatform_ShaderParts parts = {};
	if ( !ImPlatform_ParseShaderParts( text, &parts ) )
		return false;
#if IM_CURRENT_GFX == IM_GFX_DIRECTX9
	parts.defines = "TINT_SCALE=1.0";	// Shader model 3 has no bit operations: no BITS
#endif
	g_PartsShaders[ 0 ] = ImPlatform_CreateShaderFromParts( &parts, ImPlatform_ShaderStage_Vertex );
	g_PartsShaders[ 1 ] = ImPlatform_CreateShaderFromParts( &parts, ImPlatform_ShaderStage_Fragment );
	if ( g_PartsShaders[ 0 ] && g_PartsShaders[ 1 ] )
		g_PartsProgram = ImPlatform_CreateShaderProgram( g_PartsShaders[ 0 ], g_PartsShaders[ 1 ] );
	const unsigned char tint[ 2 * 2 * 4 ] = { 255, 255, 128, 255, 255, 255, 128, 255, 255, 255, 128, 255, 255, 255, 128, 255 };
	ImPlatform_TextureDesc desc = ImPlatform_TextureDesc_Default( 2, 2 );
	g_PartsTint = ImPlatform_CreateTexture( tint, &desc );
	if ( !g_PartsProgram || !g_PartsTint )
		return false;
	const ImVec4 params[ 3 ] = { ImVec4( 1, 0, 0, 1 ), ImVec4( 0, 0, 1, 1 ), ImVec4( 0, 8, 0, 0 ) };	// ColorStart, ColorEnd, Offset
	ImPlatform_SetShaderUniform( g_PartsProgram, "ImpParams", params, sizeof( params ) );
	ImPlatform_SetShaderTexture( g_PartsProgram, "tint", 1, g_PartsTint );
	return QueueRenderPassScene( g_PartsTest, 64, DrawPartsTest );
#else
	return false;
#endif
}

static int CheckPartsTest()
{
	static unsigned char px[ 64 * 64 * 4 ];
	if ( !DemoReadTexture( g_PartsTest.Target, 0, 0, 64, 64, px, sizeof( px ) ) )
	{
		printf( "SKIP: no readback of the pass target\n" );
		return 2;
	}
	const RenderPassProbe probes[] = { { 32, 0, { 253, 0, 1, 255 } }, { 32, 32, { 125, 0, 65, 255 } }, { 32, 63, { 2, 0, 127, 255 } } };
	const int bad = CheckRenderPassProbes( "shader parts", px, 64, probes, 3, 4 );
	if ( !bad )
		printf( "PASS\n" );
	return bad ? 1 : 0;
}

static void DestroyPartsTest()
{
	if ( g_PartsProgram )
		ImPlatform_DestroyShaderProgram( g_PartsProgram );
	for ( ImPlatform_Shader shader : g_PartsShaders )
		if ( shader )
			ImPlatform_DestroyShader( shader );
	if ( g_PartsTint )
		ImPlatform_DestroyTexture( g_PartsTint );
}

// --texture3d-test: a 2x2x2 texture (voxel x, y, z colored (255 x, 255 y, 255 z)) at t1, drawn into a render pass by
// shaders/volume_test.hlsl (left 64 pixels) and, with shader parts, by the same sampling written as parts with a
// "1 volume 3d" texture line (right 64 pixels): each quad shows voxel (0, 1, 1) on its left half, (1, 0, 0) on its right.
static RenderPassScene g_Texture3DTest;
static ImPlatform_ShaderProgram g_Texture3DPrograms[ 2 ] = {};
static ImPlatform_Shader g_Texture3DShaders[ 4 ] = {};
static ImTextureID g_Texture3D = 0;
static void DrawTexture3DTest( ImDrawList* dl )
{
	for ( int i = 0; i < 2; i++ )
		if ( g_Texture3DPrograms[ i ] )
		{
			ImPlatform_BeginCustomShader( dl, g_Texture3DPrograms[ i ] );
			dl->AddImage( ImGui::GetIO().Fonts->TexRef, ImVec2( 64.0f * i, 0 ), ImVec2( 64.0f * i + 64.0f, 64 ) );
			ImPlatform_EndCustomShader( dl );
		}
}

static bool CreateTexture3DTest()
{
#if !defined( DEMO_SHADER_NONE ) && IMPLATFORM_GFX_SUPPORT_SHADER_TEXTURES
	unsigned char voxels[ 2 * 2 * 2 * 4 ];
	for ( int i = 0; i < 8; i++ )
	{
		voxels[ i * 4 + 0 ] = ( i & 1 ) ? 255 : 0;			// x
		voxels[ i * 4 + 1 ] = ( i & 2 ) ? 255 : 0;			// y
		voxels[ i * 4 + 2 ] = ( i & 4 ) ? 255 : 0;			// z
		voxels[ i * 4 + 3 ] = 255;
	}
	ImPlatform_TextureDesc3D desc = ImPlatform_TextureDesc3D_Default( 2, 2, 2 );
	g_Texture3D = ImPlatform_SupportsTexture3D() ? ImPlatform_CreateTexture3D( voxels, &desc ) : 0;
	g_Texture3DPrograms[ 0 ] = DEMO_SHADER_PROGRAM( volume_test, g_Texture3DShaders );
#if IMPLATFORM_GFX_SUPPORT_SHADER_PARTS
	ImPlatform_ShaderParts parts = {};
	parts.textures[ 1 ] = "volume";
	parts.textures_3d = 1u << 1;
	parts.ps_body = "float3 uvw = v.uv.x < 0.5 ? float3(0.25, 0.75, 0.75) : float3(0.75, 0.25, 0.25);\n"
					"col_out = IMP_SAMPLE_LEVEL(volume, uvw, 0.0);\n";
	g_Texture3DShaders[ 2 ] = ImPlatform_CreateShaderFromParts( &parts, ImPlatform_ShaderStage_Vertex );
	g_Texture3DShaders[ 3 ] = ImPlatform_CreateShaderFromParts( &parts, ImPlatform_ShaderStage_Fragment );
	if ( g_Texture3DShaders[ 2 ] && g_Texture3DShaders[ 3 ] )
		g_Texture3DPrograms[ 1 ] = ImPlatform_CreateShaderProgram( g_Texture3DShaders[ 2 ], g_Texture3DShaders[ 3 ] );
	if ( !g_Texture3DPrograms[ 1 ] )
		return false;
#endif
	if ( !g_Texture3D || !g_Texture3DPrograms[ 0 ] )
		return false;
	for ( ImPlatform_ShaderProgram program : g_Texture3DPrograms )
		if ( program )
			ImPlatform_SetShaderTexture( program, "volume", 1, g_Texture3D );
	return QueueRenderPassScene( g_Texture3DTest, 128, DrawTexture3DTest );
#else
	return false;
#endif
}

static int CheckTexture3DTest()
{
	static unsigned char px[ 128 * 64 * 4 ];
	if ( !DemoReadTexture( g_Texture3DTest.Target, 0, 0, 128, 64, px, sizeof( px ) ) )
	{
		printf( "SKIP: no readback of the pass target\n" );
		return 2;
	}
	const RenderPassProbe probes[] = { { 16, 32, { 0, 255, 255, 255 } }, { 48, 32, { 255, 0, 0, 255 } },
									   { 80, 32, { 0, 255, 255, 255 } }, { 112, 32, { 255, 0, 0, 255 } } };
	const int bad = CheckRenderPassProbes( "texture 3D", px, 128, probes, g_Texture3DPrograms[ 1 ] ? 4 : 2, 4 );
	if ( !bad )
		printf( "PASS\n" );
	return bad ? 1 : 0;
}

// --vertex-inputs-test: shader parts with their own vertex inputs (a float4 position and a float4 of data), drawn by
// ImPlatform_DrawIndexed from a draw callback into a render pass: two quads over the top half, the left one with id 1
// (green), the right one with id 2 (blue), the id passed in an integer flat varying. The position is the projection
// applied row by row (IMP_MUL(float4(1, 0, 0, 0), ProjMtx)...), so a wrong row shows; the bottom half stays clear.
static RenderPassScene g_VertexInputsTest;
static ImPlatform_ShaderProgram g_VertexInputsProgram = nullptr;
static ImPlatform_Shader g_VertexInputsShaders[ 2 ] = {};
#if IMPLATFORM_GFX_SUPPORT_VERTEX_BUFFERS
static ImPlatform_VertexBuffer g_VertexInputsVB = nullptr;
static ImPlatform_IndexBuffer g_VertexInputsIB = nullptr;
#endif
#if IMPLATFORM_GFX_SUPPORT_SHADER_PARTS && IMPLATFORM_GFX_SUPPORT_VERTEX_BUFFERS
static void DrawVertexInputsQuads( const ImDrawList*, const ImDrawCmd* )
{
	ImPlatform_BindBuffers( g_VertexInputsVB, g_VertexInputsIB );
	ImPlatform_DrawIndexed( 0, 12, 0 );
}
static void DrawVertexInputsTest( ImDrawList* dl )
{
	// The backends set the scissor for regular draws only: a draw first, so the callback does not run with a stale one.
	dl->AddRectFilled( ImVec2( 0, 63 ), ImVec2( 1, 64 ), IM_COL32( 0, 0, 0, 255 ) );
	ImPlatform_BeginCustomShader( dl, g_VertexInputsProgram );
	dl->AddCallback( DrawVertexInputsQuads, nullptr );
	ImPlatform_EndCustomShader( dl );
}
#endif

static bool CreateVertexInputsTest()
{
#if IMPLATFORM_GFX_SUPPORT_SHADER_PARTS && IMPLATFORM_GFX_SUPPORT_VERTEX_BUFFERS
	ImPlatform_ShaderParts parts = {};
	parts.vertex_inputs = "float4 pos : POSITION; float4 data : TEXCOORD0;";
#if IM_CURRENT_GFX == IM_GFX_DIRECTX9
	parts.varyings = "float4 id;";	// Shader model 3: no integer or flat varyings
#else
	parts.varyings = "nointerpolation int4 id;";
#endif
	parts.vs_body = "float4 p = float4(in_pos.xy, 0.0, 1.0);\n"
					"position = float4(dot(IMP_MUL(float4(1.0, 0.0, 0.0, 0.0), ProjMtx), p), dot(IMP_MUL(float4(0.0, 1.0, 0.0, 0.0), ProjMtx), p),\n"
					"                  dot(IMP_MUL(float4(0.0, 0.0, 1.0, 0.0), ProjMtx), p), dot(IMP_MUL(float4(0.0, 0.0, 0.0, 1.0), ProjMtx), p));\n"
					"v.id = int4(IMP_CAST(int, in_data.x + 0.5), 0, 0, 0);\n";
	parts.ps_body = "col_out = v.id.x == 1 ? float4(0.0, 1.0, 0.0, 1.0) : v.id.x == 2 ? float4(0.0, 0.0, 1.0, 1.0) : float4(1.0, 0.0, 0.0, 1.0);\n";
	g_VertexInputsShaders[ 0 ] = ImPlatform_CreateShaderFromParts( &parts, ImPlatform_ShaderStage_Vertex );
	g_VertexInputsShaders[ 1 ] = ImPlatform_CreateShaderFromParts( &parts, ImPlatform_ShaderStage_Fragment );
	if ( g_VertexInputsShaders[ 0 ] && g_VertexInputsShaders[ 1 ] )
		g_VertexInputsProgram = ImPlatform_CreateShaderProgram( g_VertexInputsShaders[ 0 ], g_VertexInputsShaders[ 1 ] );
	if ( !g_VertexInputsProgram )
		return false;
	struct Vertex { float pos[ 4 ]; float data[ 4 ]; };
	const Vertex vertices[ 8 ] = {
		{ { 0, 0, 0, 0 }, { 1, 0, 0, 0 } }, { { 32, 0, 0, 0 }, { 1, 0, 0, 0 } }, { { 32, 32, 0, 0 }, { 1, 0, 0, 0 } }, { { 0, 32, 0, 0 }, { 1, 0, 0, 0 } },
		{ { 32, 0, 0, 0 }, { 2, 0, 0, 0 } }, { { 64, 0, 0, 0 }, { 2, 0, 0, 0 } }, { { 64, 32, 0, 0 }, { 2, 0, 0, 0 } }, { { 32, 32, 0, 0 }, { 2, 0, 0, 0 } } };
	const unsigned short indices[ 12 ] = { 0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7 };
	static const ImPlatform_VertexAttribute attributes[ 2 ] = {
		{ ImPlatform_VertexFormat_Float4, 0, "POSITION" }, { ImPlatform_VertexFormat_Float4, 16, "TEXCOORD" } };
	ImPlatform_VertexBufferDesc vb = {};
	vb.vertex_count = 8;
	vb.vertex_stride = sizeof( Vertex );
	vb.usage = ImPlatform_BufferUsage_Static;
	vb.attributes = attributes;
	vb.attribute_count = 2;
	ImPlatform_IndexBufferDesc ib = {};
	ib.index_count = 12;
	ib.format = ImPlatform_IndexFormat_UInt16;
	ib.usage = ImPlatform_BufferUsage_Static;
	g_VertexInputsVB = ImPlatform_CreateVertexBuffer( vertices, &vb );
	g_VertexInputsIB = ImPlatform_CreateIndexBuffer( indices, &ib );
	if ( !g_VertexInputsVB || !g_VertexInputsIB || !ImPlatform_CreateVertexInputLayout( g_VertexInputsVB, g_VertexInputsProgram ) )
		return false;
	return QueueRenderPassScene( g_VertexInputsTest, 64, DrawVertexInputsTest );
#else
	return false;
#endif
}

static int CheckVertexInputsTest()
{
	static unsigned char px[ 64 * 64 * 4 ];
	if ( !DemoReadTexture( g_VertexInputsTest.Target, 0, 0, 64, 64, px, sizeof( px ) ) )
	{
		printf( "SKIP: no readback of the pass target\n" );
		return 2;
	}
	const RenderPassProbe probes[] = { { 16, 16, { 0, 255, 0, 255 } }, { 48, 16, { 0, 0, 255, 255 } }, { 16, 48, { 0, 0, 0, 0 } } };
	const int bad = CheckRenderPassProbes( "vertex inputs", px, 64, probes, 3, 4 );
	if ( !bad )
		printf( "PASS\n" );
	return bad ? 1 : 0;
}

static void DestroyVertexInputsTest()
{
	if ( g_VertexInputsProgram )
		ImPlatform_DestroyShaderProgram( g_VertexInputsProgram );
	for ( ImPlatform_Shader shader : g_VertexInputsShaders )
		if ( shader )
			ImPlatform_DestroyShader( shader );
#if IMPLATFORM_GFX_SUPPORT_VERTEX_BUFFERS
	if ( g_VertexInputsVB )
		ImPlatform_DestroyVertexBuffer( g_VertexInputsVB );
	if ( g_VertexInputsIB )
		ImPlatform_DestroyIndexBuffer( g_VertexInputsIB );
#endif
}

static void DestroyTexture3DTest()
{
	for ( ImPlatform_ShaderProgram program : g_Texture3DPrograms )
		if ( program )
			ImPlatform_DestroyShaderProgram( program );
	for ( ImPlatform_Shader shader : g_Texture3DShaders )
		if ( shader )
			ImPlatform_DestroyShader( shader );
	if ( g_Texture3D )
		ImPlatform_DestroyTexture( g_Texture3D );
}

// --direct-draw-test: from a draw callback inside a render pass, the same gradient drawn by ImPlatform_DrawQuadNow into a
// pushed render target, its colors from ImPlatform_PushShaderConstants, copied by ImPlatform_CopyCurrentTarget, then both
// read back: the direct draws a backdrop effect makes (copy, push a target, draw a quad, pop).
static RenderPassScene g_DirectDrawPass;
static ImTextureID g_DirectDrawTarget = 0;
static ImTextureID g_DirectDrawCopy = 0;
static bool g_DirectDrawPushed = false;
static bool g_DirectDrawCopied = false;
static void DirectDrawCallback( const ImDrawList*, const ImDrawCmd* )
{
#if IMPLATFORM_GFX_SUPPORT_RENDER_TARGETS && IMPLATFORM_GFX_SUPPORT_DIRECT_DRAW
	if ( !ImPlatform_PushRenderTarget( g_DirectDrawTarget, true ) )
		return;
	g_DirectDrawPushed = true;
	const ImVec4 colors[ 2 ] = { ImVec4( 1, 0, 0, 1 ), ImVec4( 0, 0, 1, 1 ) };	// gradient.hlsl's ColorStart, ColorEnd
	void* constants = ImPlatform_PushShaderConstants( colors, sizeof( colors ) );
	ImPlatform_DrawQuadNow( g_ShaderTestProgram, ImVec4( 0, 0, 64, 64 ), ImVec4( 0, 0, 1, 1 ) );
	ImPlatform_PopShaderConstants( constants );
	g_DirectDrawCopied = ImPlatform_CopyCurrentTarget( g_DirectDrawCopy, 0, 0, 64, 64, 0, 0 );	// The backdrop copy of a blur
	ImPlatform_PopRenderTarget();
#endif
}

static void DrawDirectDrawTest( ImDrawList* dl )
{
	dl->AddCallback( DirectDrawCallback, nullptr );
	dl->AddCallback( ImGui::GetPlatformIO().DrawCallback_ResetRenderState, nullptr );
	dl->AddRectFilled( ImVec2( 0, 0 ), ImVec2( 1, 1 ), IM_COL32_WHITE );	// Backends may skip draw data without vertices
}

static int CheckDirectDrawTest()
{
	static unsigned char px[ 64 * 64 * 4 ];
	if ( !g_DirectDrawPushed )
	{
		printf( "SKIP: no render target stack in draw callbacks on this backend\n" );
		return 2;
	}
	if ( !DemoReadTexture( g_DirectDrawTarget, 0, 0, 64, 64, px, sizeof( px ) ) )
	{
		printf( "SKIP: no readback of the pushed target\n" );
		return 2;
	}
	const RenderPassProbe probes[] = { { 32, 0, { 253, 0, 2, 255 } }, { 32, 32, { 125, 0, 130, 255 } }, { 32, 63, { 2, 0, 253, 255 } } };
	int bad = CheckRenderPassProbes( "direct draw", px, 64, probes, 3, 4 );
	// Every backend with a render target stack copies the current target too
	if ( !g_DirectDrawCopied || !DemoReadTexture( g_DirectDrawCopy, 0, 0, 64, 64, px, sizeof( px ) ) )
	{
		printf( "FAIL: CopyCurrentTarget from the pushed target\n" );
		++bad;
	}
	else
		bad += CheckRenderPassProbes( "current target copy", px, 64, probes, 3, 4 );
	if ( !bad )
		printf( "PASS\n" );
	return bad ? 1 : 0;
}
#endif
#endif // IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER

#if IMPLATFORM_GFX_SUPPORT_COMPUTE
#if IM_CURRENT_GFX == IM_GFX_OPENGL3
#include "shaders/compute_test.glsl.h"
#elif IM_CURRENT_GFX == IM_GFX_VULKAN
#include "shaders/compute_test.spv.h"
#else
#include "shaders/compute_test.hlsl.h"
#endif
#endif
// --compute-test: storage texture + compute dispatch + readback, the same check on every backend.
// Reads an input texture (t0), writes a storage texture (u0) with a constant from b0, reads it back.
// Exit code: 0 pass, 1 fail, 2 unsupported here.
static int ImPlatformDemo_ComputeTest()
{
	printf( "compute test: readback=%d compute=%d\n", (int)ImPlatform_SupportsReadback(), (int)ImPlatform_SupportsCompute() );
	if ( !ImPlatform_SupportsReadback() )
	{
		printf( "SKIP: not supported on this backend\n" );
		return 2;
	}
	const unsigned int N = 64;
	static unsigned char in_px[ 64 * 64 * 4 ], out_px[ 64 * 64 * 4 ];
	for ( unsigned int y = 0; y < N; ++y )
		for ( unsigned int x = 0; x < N; ++x )
		{
			unsigned char* q = in_px + ( y * N + x ) * 4;
			q[ 0 ] = (unsigned char)( x * 3 ); q[ 1 ] = (unsigned char)( y * 3 ); q[ 2 ] = 7; q[ 3 ] = 255;
		}
	ImPlatform_TextureDesc desc = ImPlatform_TextureDesc_Default( N, N );
	ImTextureID input  = ImPlatform_CreateTexture( in_px, &desc );
	if ( !ImPlatform_SupportsCompute() )
	{
		// Readback only: a sub-rectangle of the input comes back unchanged
		const bool read = input && DemoReadTexture( input, 8, 16, 32, 16, out_px, sizeof( out_px ) );
		int bad = read ? 0 : 1;
		for ( unsigned int y = 0; read && y < 16; ++y )
			bad += memcmp( out_px + y * 32 * 4, in_px + ( ( y + 16 ) * N + 8 ) * 4, 32 * 4 ) != 0;
		printf( bad ? "FAIL (readback only, %d bad rows)\n" : "PASS (readback only)\n", bad );
		return bad ? 1 : 0;
	}
#if IMPLATFORM_GFX_SUPPORT_COMPUTE
	ImTextureID output = ImPlatform_CreateStorageTexture( NULL, &desc );

	ImPlatform_ShaderDesc sd = {};
	sd.stage = ImPlatform_ShaderStage_Compute;
	// shaders/compute_test.hlsl, compiled by shaders/compile_shaders: HLSL on Direct3D, SPIR-V on Vulkan, GLSL 430 on OpenGL.
#if IM_CURRENT_GFX == IM_GFX_OPENGL3
	sd.format = ImPlatform_ShaderFormat_GLSL;
	sd.entry_point = "main";
	sd.source_code = compute_test_cs_glsl;
#elif IM_CURRENT_GFX == IM_GFX_VULKAN
	sd.format = ImPlatform_ShaderFormat_SPIRV;
	sd.entry_point = "main";
	sd.bytecode = compute_test_cs_spv;
	sd.bytecode_size = sizeof( compute_test_cs_spv );
#else
	sd.format = ImPlatform_ShaderFormat_HLSL;
	sd.entry_point = "cs_main";
	sd.source_code = compute_test_hlsl;
#endif
	ImPlatform_Shader cs = ImPlatform_CreateShader( &sd );
	const float params[ 4 ] = { 0.0f, 0.0f, 0.5f, 0.0f };
	const bool dispatched = cs && input && output
		&& ImPlatform_DispatchCompute( cs, &input, 1, &output, 1, params, sizeof( params ), N / 8, N / 8, 1 );
	const bool read = dispatched && DemoReadTexture( output, 0, 0, N, N, out_px, sizeof( out_px ) );
	printf( "shader=%d input=%d output=%d dispatched=%d read=%d\n", cs != NULL, input != (ImTextureID)0, output != (ImTextureID)0, (int)dispatched, (int)read );
	int bad = read ? 0 : 1;
	for ( unsigned int i = 0; read && i < N * N; ++i )
	{
		const unsigned char* a = in_px + i * 4;
		const unsigned char* b = out_px + i * 4;
		const bool ok = b[ 0 ] == a[ 0 ] && b[ 1 ] == a[ 1 ] && ( b[ 2 ] == 127 || b[ 2 ] == 128 ) && b[ 3 ] == 255;
		if ( !ok && bad++ < 4 )
			printf( "  pixel (%u,%u): got %u %u %u %u, want %u %u 128 255\n", i % N, i / N, b[ 0 ], b[ 1 ], b[ 2 ], b[ 3 ], a[ 0 ], a[ 1 ] );
	}
	printf( bad ? "FAIL (%d bad pixels)\n" : "PASS\n", bad );
	return bad ? 1 : 0;
#else
	return 2;	// Not reached: ImPlatform_SupportsCompute is false
#endif
}

int main( int argc, char** argv )
{
	const bool computeTest = argc > 1 && strcmp( argv[ 1 ], "--compute-test" ) == 0;
	const bool renderPassTest = argc > 1 && strcmp( argv[ 1 ], "--render-pass-test" ) == 0;
	const bool shaderTest = argc > 1 && strcmp( argv[ 1 ], "--shader-test" ) == 0;
	const bool directDrawTest = argc > 1 && strcmp( argv[ 1 ], "--direct-draw-test" ) == 0;
	const bool partsTest = argc > 1 && strcmp( argv[ 1 ], "--shader-parts-test" ) == 0;
	const bool texture3DTest = argc > 1 && strcmp( argv[ 1 ], "--texture3d-test" ) == 0;
	const bool vertexInputsTest = argc > 1 && strcmp( argv[ 1 ], "--vertex-inputs-test" ) == 0;
	const bool passTest = renderPassTest || shaderTest || directDrawTest || partsTest || texture3DTest || vertexInputsTest;	// Tests read back a render pass
	int exitCode = 0;
#ifndef __EMSCRIPTEN__
	int renderPassFrame = 0;
#endif
	// Configure borderless window with custom titlebar
	ImPlatform_EnableCustomTitleBar();
	{
		ImPlatform_BorderlessParams params = ImPlatform_BorderlessParams_Default();
		params.resizeBorderSize = 6;
		params.minWidth = 400;
		params.minHeight = 300;
		params.allowResize = true;
		params.allowMove = true;
		ImPlatform_SetBorderlessParams(&params);
	}

	// Setup for an image
	int width;
	int height;
	int channel;
#if IM_HAS_STBI
	stbi_uc* data = stbi_load( "MyImage01.jpg", &width, &height, &channel, STBI_rgb_alpha );
#else
	// If we don't have stb_image we create an image manually instead of loading the file.
	width = height = 256;
	channel = 4;
	unsigned char* data = ( unsigned char* )IM_ALLOC( width * height * channel );
	unsigned char* white_data = ( unsigned char* )IM_ALLOC( width * height * channel );
	for ( int j = 0; j < height; ++j )
	{
		for ( int i = 0; i < width; ++i )
		{
			float x = floorf( ( float )i / 16.0f );
			float y = floorf( ( float )j / 16.0f );
			float PatternMask = fmodf( x + fmodf( y, 2.0f ), 2.0f );
			unsigned char val = PatternMask ? 255 : 0;
			data[ channel * ( j * height + i ) + 0 ] = val;
			data[ channel * ( j * height + i ) + 1 ] = val;
			data[ channel * ( j * height + i ) + 2 ] = val;
			data[ channel * ( j * height + i ) + 3 ] = 0xFFu;
			white_data[ channel * ( j * height + i ) + 0 ] = 0xFFu;
			white_data[ channel * ( j * height + i ) + 1 ] = 0xFFu;
			white_data[ channel * ( j * height + i ) + 2 ] = 0xFFu;
			white_data[ channel * ( j * height + i ) + 3 ] = 0xFFu;
		}
	}
#endif

	float t = 0.0f;

	// Using the new C API
	bool bGood;

	bGood = ImPlatform_CreateWindow( "ImPlatform Demo", ImVec2( 100, 100 ), 1024, 764 );
	if ( !bGood )
	{
		fprintf( stderr, "ImPlatform: Cannot create window.\n" );
		return 1;
	}

	bGood = ImPlatform_InitGfxAPI();
	if ( !bGood )
	{
		fprintf( stderr, "ImPlatform: Cannot initialize the Graphics API.\n" );
		return 1;
	}

	bGood = ImPlatform_ShowWindow();
	if ( !bGood )
	{
		fprintf( stderr, "ImPlatform: Cannot show the window.\n" );
		return 1;
	}

	IMGUI_CHECKVERSION();
	bGood = ImGui::CreateContext() != nullptr;
	if ( !bGood )
	{
		fprintf( stderr, "ImGui: Cannot create context.\n" );
		return 1;
	}

	// Setup Dear ImGui context
	ImGuiIO& io = ImGui::GetIO(); ( void )io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;	// Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;	// Enable Gamepad Controls
#ifdef IMGUI_HAS_DOCK
	//io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;		// Enable Docking
#endif
#ifdef IMGUI_HAS_VIEWPORT
	//io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;		// Enable Multi-Viewport / Platform Windows
	////io.ConfigViewportsNoAutoMerge = true;
	////io.ConfigViewportsNoTaskBarIcon = true;
#endif

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	//io.Fonts->AddFontFromFileTTF( "../extern/FiraCode/distr/ttf/FiraCode-Medium.ttf", 16.0f );

	// Setup DPI scaling (cross-platform)
	ImGuiStyle& style = ImGui::GetStyle();
	float dpi_scale = ImPlatform_GetDpiScale();
	style.ScaleAllSizes(dpi_scale);
	style.FontScaleDpi = dpi_scale;
#ifdef IMGUI_HAS_DOCK
	io.ConfigDpiScaleFonts = true;
#endif
#ifdef IMGUI_HAS_VIEWPORT
	io.ConfigDpiScaleViewports = true;
#endif

	// Register DPI change callback for runtime monitor changes
	ImPlatform_SetDpiChangeCallback(OnDpiChanged, nullptr);

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
#ifdef IMGUI_HAS_VIEWPORT
	if ( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
	{
		style.WindowRounding = 0.0f;
		style.Colors[ ImGuiCol_WindowBg ].w = 1.0f;
	}
#endif

	bGood = ImPlatform_InitPlatform();
	if ( !bGood )
	{
		fprintf( stderr, "ImPlatform: Cannot initialize platform.\n" );
		return 1;
	}
	bGood = ImPlatform_InitGfx();
	if ( !bGood )
	{
		fprintf( stderr, "ImPlatform: Cannot initialize graphics.\n" );
		return 1;
	}
	if ( computeTest )
		return ImPlatformDemo_ComputeTest();

	// Create test textures using new Texture Creation API
#ifdef IMGUI_HAS_TEXTURES
	ImPlatform_TextureDesc tex_desc = ImPlatform_TextureDesc_Default(width, height);
	ImTextureID img = ImPlatform_CreateTexture(data, &tex_desc);
	ImTextureID img_white = ImPlatform_CreateTexture(white_data, &tex_desc);
#else
	ImTextureID img = NULL;
	ImTextureID img_white = NULL;
#endif

#if IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER
	// Custom shaders: an arrow drawn from a signed distance field, and a gradient between two adjustable colors
	ImVec4 gradient_color_start = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
	ImVec4 gradient_color_end = ImVec4(0.2f, 0.2f, 1.0f, 1.0f);
	ImPlatform_Shader arrow_shaders[2] = {};
	ImPlatform_Shader gradient_shaders[2] = {};
	ImPlatform_ShaderProgram arrow_program = DEMO_SHADER_PROGRAM(arrow_sdf, arrow_shaders);
	ImPlatform_ShaderProgram gradient_program = DEMO_SHADER_PROGRAM(gradient, gradient_shaders);
#endif // IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER

#ifdef IMGUI_HAS_TEXTURES
	if (!img || !img_white)
	{
		fprintf(stderr, "ImPlatform: Failed to create textures.\n");
	}
#endif

#if IM_HAS_STBI
	stbi_image_free( data );
#else
	IM_FREE( data );
	IM_FREE( white_data );
#endif

	ImVec4 clear_color = ImVec4( 0.461f, 0.461f, 0.461f, 1.0f );
#ifdef __EMSCRIPTEN__
	io.IniFilename = nullptr;
	EMSCRIPTEN_MAINLOOP_BEGIN
#else
	while ( ImPlatform_PlatformContinue() )
#endif
	{
		if ( !ImPlatform_PlatformEvents() )
		{
			continue;
		}

		// The self-tests read back what they render: they keep rendering when the window is occluded (screen off or
		// locked), where presenting is skipped.
		if ( !ImPlatform_GfxCheck() && !passTest )
		{
			continue;
		}

		ImPlatform_GfxAPINewFrame();
		ImPlatform_PlatformNewFrame();

		ImGui::NewFrame();
#if defined( IMGUI_HAS_TEXTURES ) && !defined( __EMSCRIPTEN__ )
		if ( renderPassTest && renderPassFrame == 0 && !QueueRenderPassScene( g_RenderPassTest, 64, DrawRenderPassTest ) )
		{
			printf( "SKIP: render passes not supported on this backend\n" );
			exitCode = 2;
			break;
		}
		if ( renderPassTest && renderPassFrame == 1 )
			QueueRenderPassScene( g_RenderPassComposite, 128, DrawRenderPassComposite );
		if ( shaderTest && renderPassFrame == 0 )
		{
#if IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER
			g_ShaderTestProgram = gradient_program;
			const ImVec4 red( 1, 0, 0, 1 ), blue( 0, 0, 1, 1 );
			ImPlatform_BeginUniformBlock( gradient_program );
			ImPlatform_SetUniform( "ColorStart", &red, sizeof( red ) );
			ImPlatform_SetUniform( "ColorEnd", &blue, sizeof( blue ) );
			ImPlatform_EndUniformBlock( gradient_program );
			if ( !gradient_program || !QueueRenderPassScene( g_ShaderTest, 64, DrawShaderTest ) )
#endif
			{
				printf( "SKIP: no custom shader or render pass on this backend\n" );
				exitCode = 2;
				break;
			}
		}
		if ( vertexInputsTest && renderPassFrame == 0 && !CreateVertexInputsTest() )
		{
			printf( "SKIP: no shader parts with vertex buffers on this backend\n" );
			exitCode = 2;
			break;
		}
		if ( texture3DTest && renderPassFrame == 0 && !CreateTexture3DTest() )
		{
			printf( "SKIP: no 3D texture, custom shader or render pass on this backend\n" );
			exitCode = 2;
			break;
		}
		if ( partsTest && renderPassFrame == 0 && !CreatePartsTest() )
		{
			printf( "SKIP: no shader parts on this backend\n" );
			exitCode = 2;
			break;
		}
		if ( directDrawTest && renderPassFrame == 0 )
		{
#if IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER
			g_ShaderTestProgram = gradient_program;
			ImPlatform_TextureDesc desc = ImPlatform_TextureDesc_Default( 64, 64 );
			desc.format = ImPlatform_GetRenderPassFormat();
			g_DirectDrawTarget = ImPlatform_CreateRenderTexture( &desc );
			g_DirectDrawCopy = ImPlatform_CreateRenderTexture( &desc );
			if ( !gradient_program || !g_DirectDrawTarget || !QueueRenderPassScene( g_DirectDrawPass, 64, DrawDirectDrawTest ) )
#endif
			{
				printf( "SKIP: no custom shader or render pass on this backend\n" );
				exitCode = 2;
				break;
			}
		}
#endif

		// Custom Title Bar (using Em-based sizing for DPI independence)
#if IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR
		if ( ImPlatform_CustomTitleBarEnabled() )
		{
			float titleBarHeight = ImPlatform_EmSize(2.0f);
			if ( ImPlatform_BeginCustomTitleBar( titleBarHeight ) )
			{
				ImGui::Text("ImPlatform Demo");
				ImVec2 buttonSize = ImPlatform_EmToVec2(2.0f, 1.5f);
				float buttonsWidth = buttonSize.x * 3.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;
				ImGui::SameLine(ImGui::GetWindowWidth() - buttonsWidth);
				if (ImGui::Button("_", buttonSize))
					ImPlatform_MinimizeApp();
				ImGui::SameLine();
				if (ImGui::Button("[]", buttonSize))
					ImPlatform_MaximizeApp();
				ImGui::SameLine();
				if (ImGui::Button("X", buttonSize))
					ImPlatform_CloseApp();
			}
			ImPlatform_EndCustomTitleBar();
		}
#endif

		// Calculate 2x2 grid layout to fill the display
		ImVec2 display_size = io.DisplaySize;
		float half_width = display_size.x * 0.5f;
		float half_height = display_size.y * 0.5f;

		// Bottom-right: Dear ImGui Demo
		ImGui::SetNextWindowPos(ImVec2(half_width, half_height), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(half_width, half_height), ImGuiCond_Always);
		bool show = true;
		ImGui::ShowDemoWindow( &show );

		// Top-right: Texture API Demo Window
#ifdef IMGUI_HAS_TEXTURES
		if (img && img_white)
		{
			ImGui::SetNextWindowPos(ImVec2(half_width, 0), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(half_width, half_height), ImGuiCond_Always);
			ImGui::Begin("ImPlatform Texture API Demo", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
			ImGui::Text("Checkerboard texture created with ImPlatform_CreateTexture:");
			ImGui::Image(img, ImPlatform_EmToVec2(16.0f, 16.0f));

			ImGui::Separator();
			ImGui::Text("White texture:");
			ImGui::Image(img_white, ImPlatform_EmToVec2(8.0f, 8.0f));

			ImGui::Separator();
			if ( !passTest )		// One pass per frame: the test's.
				RenderPassDemo();

			ImGui::Separator();
			ImGui::Text("This demonstrates the new Texture Creation API:");
			ImGui::BulletText("ImPlatform_TextureDesc_Default()");
			ImGui::BulletText("ImPlatform_CreateTexture()");
			ImGui::BulletText("Works with OpenGL3, DX11, and other backends");
			ImGui::End();
		}
#endif

#if IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER
		// Custom Shader Demo Window 1: Arrow SDF Shape (Top-left)
		if (arrow_program)
		{
			ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(half_width, half_height), ImGuiCond_Always);
			ImGui::Begin("Custom Shader: Arrow SDF", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
			ImGui::Text("This demonstrates a custom shader with SDF rendering.");
			ImGui::Text("Arrow shape rendered using Signed Distance Field.");

			ImGui::Separator();
			ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Shader API Used:");
			ImGui::BulletText("shaders/arrow_sdf.hlsl, compiled for this backend");
			ImGui::BulletText("Custom pixel shader with SDF function");
			ImGui::BulletText("No custom vertex buffer (uses ImGui's default)");

			ImGui::Separator();

			// Render the arrow using custom shader (using helper function)
			ImPlatform_DrawCustomShaderQuad(arrow_program);

			ImGui::End();
		}

		// Custom Shader Demo Window 2: Linear Gradient (Bottom-left)
		if (gradient_program && !shaderTest && !directDrawTest)		// The tests set their own constants
		{
			ImGui::SetNextWindowPos(ImVec2(0, half_height), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(half_width, half_height), ImGuiCond_Always);
			ImGui::Begin("Custom Shader: Linear Gradient", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
			ImGui::Text("This demonstrates a custom shader with adjustable colors.");
			ImGui::Text("Linear gradient interpolating between two colors.");

			ImGui::Separator();
			ImGui::ColorEdit4("Start Color", (float*)&gradient_color_start);
			ImGui::ColorEdit4("End Color", (float*)&gradient_color_end);

			ImGui::Separator();
			ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Shader API Used:");
			ImGui::BulletText("shaders/gradient.hlsl, compiled for this backend");
			ImGui::BulletText("Uniforms: ColorStart, ColorEnd");
			ImGui::BulletText("Using ImPlatform Uniform Block API");
			ImGui::BulletText("Pixel shader interpolates between two colors");

			ImGui::Separator();

			// Update shader uniforms using uniform block API
			ImPlatform_BeginUniformBlock(gradient_program);
			ImPlatform_SetUniform("ColorStart", &gradient_color_start, sizeof(ImVec4));
			ImPlatform_SetUniform("ColorEnd", &gradient_color_end, sizeof(ImVec4));
			ImPlatform_EndUniformBlock(gradient_program);

			// Render the gradient using custom shader (using helper function)
			ImPlatform_DrawCustomShaderQuad(gradient_program);

			ImGui::End();
		}
#else
		// Show a message when custom shaders are not supported
		// Top-left and bottom-left combined
		{
			ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(half_width, display_size.y), ImGuiCond_Always);
			ImGui::Begin("Custom Shader Support", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
			ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Custom Shaders Not Supported");
			ImGui::Separator();
			ImGui::Text("The current graphics backend does not support custom shaders.");
#if (IM_CURRENT_GFX == IM_GFX_DIRECTX9)
			ImGui::Text("DirectX 9: Shader API requires D3DX which is deprecated.");
#endif
			ImGui::Text("Supported backends:");
			ImGui::BulletText("OpenGL 3");
			ImGui::BulletText("DirectX 10");
			ImGui::BulletText("DirectX 11");
			ImGui::BulletText("DirectX 12 (stub)");
			ImGui::BulletText("Vulkan (stub)");
			ImGui::BulletText("Metal (stub)");
			ImGui::BulletText("WebGPU");
			ImGui::End();
		}
#endif // IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER

		ImGui::Render();

		ImPlatform_GfxAPIClear( clear_color );
		ImPlatform_GfxAPIRender( clear_color );

		// Update and Render additional Platform Windows
#ifdef IMGUI_HAS_VIEWPORT
		ImPlatform_GfxViewportPre();
		ImPlatform_GfxViewportPost();
#endif

		t += ImGui::GetIO().DeltaTime;

		ImPlatform_GfxAPISwapBuffer();
#if defined( IMGUI_HAS_TEXTURES ) && !defined( __EMSCRIPTEN__ )
		if ( passTest && ++renderPassFrame == 3 )
		{
#if IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER
			exitCode = shaderTest ? CheckShaderTest() : directDrawTest ? CheckDirectDrawTest() : partsTest ? CheckPartsTest()
					 : texture3DTest ? CheckTexture3DTest() : vertexInputsTest ? CheckVertexInputsTest() : CheckRenderPassTest();
#else
			exitCode = CheckRenderPassTest();
#endif
			break;
		}
#endif
	}
#ifdef __EMSCRIPTEN__
	EMSCRIPTEN_MAINLOOP_END;
#endif

	// Cleanup textures
#ifdef IMGUI_HAS_TEXTURES
	if (img)
		ImPlatform_DestroyTexture(img);
	if (img_white)
		ImPlatform_DestroyTexture(img_white);
#endif

#if IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER
	// Cleanup shaders
	if (arrow_program)
		ImPlatform_DestroyShaderProgram(arrow_program);
	for (ImPlatform_Shader shader : arrow_shaders)
		if (shader)
			ImPlatform_DestroyShader(shader);

	if (gradient_program)
		ImPlatform_DestroyShaderProgram(gradient_program);
	for (ImPlatform_Shader shader : gradient_shaders)
		if (shader)
			ImPlatform_DestroyShader(shader);
#endif

#ifdef IMGUI_HAS_TEXTURES
	DestroyRenderPassScene( g_RenderPassDemo );
#ifndef __EMSCRIPTEN__
	DestroyRenderPassScene( g_RenderPassTest );
	DestroyRenderPassScene( g_RenderPassComposite );
#if IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER
	DestroyRenderPassScene( g_ShaderTest );
	DestroyRenderPassScene( g_PartsTest );
	DestroyPartsTest();
	DestroyRenderPassScene( g_Texture3DTest );
	DestroyTexture3DTest();
	DestroyRenderPassScene( g_VertexInputsTest );
	DestroyVertexInputsTest();
	DestroyRenderPassScene( g_DirectDrawPass );
	if ( g_DirectDrawTarget )
		ImPlatform_DestroyTexture( g_DirectDrawTarget );
	if ( g_DirectDrawCopy )
		ImPlatform_DestroyTexture( g_DirectDrawCopy );
#endif
#endif
#endif
	ImPlatform_ShutdownGfxAPI();
	ImPlatform_ShutdownWindow();
	ImPlatform_ShutdownPostGfxAPI();

	ImGui::DestroyContext();

	ImPlatform_DestroyWindow();

	return exitCode;
}
