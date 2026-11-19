
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
	if ( !g_RenderPassTest.Target || !ImPlatform_ReadTexture( g_RenderPassTest.Target, 0, 0, 64, 64, px, sizeof( px ) ) )
	{
		printf( "SKIP: no readback of the pass target\n" );
		return 2;
	}
	// Pass target: the clear, the orientation and premultiplied colors.
	const RenderPassProbe target[] = { { 8, 8, { 255, 0, 0, 255 } }, { 48, 8, { 0, 0, 128, 128 } }, { 8, 48, { 0, 0, 0, 0 } }, { 48, 48, { 0, 0, 0, 0 } } };
	int bad = CheckRenderPassProbes( "target", px, 64, target, 4, 4 );
	// Composite over gray (100) with premultiplied alpha: blue at half coverage is 128 + 100 * 127 / 255.
	// At half opacity: red 128 + 100 * 127 / 255 = 178 (green, blue 50); blue at a quarter, 64 + 100 * 191 / 255 = 139.
	if ( ImPlatform_ReadTexture( g_RenderPassComposite.Target, 0, 0, 128, 128, px, sizeof( px ) ) )
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
// backend by shaders/compile_shaders: Direct3D compiles the HLSL at run time, Vulkan takes SPIR-V, OpenGL GLSL 330 and
// WebGPU WGSL.
#if (IM_CURRENT_GFX == IM_GFX_VULKAN)
#include "shaders/arrow_sdf.spv.h"
#include "shaders/gradient.spv.h"
#define DEMO_SHADER_PROGRAM( name, shaders ) CreateDemoProgram( ImPlatform_ShaderFormat_SPIRV, name##_vs_spv, sizeof( name##_vs_spv ), name##_ps_spv, sizeof( name##_ps_spv ), "main", "main", shaders )
#elif (IM_CURRENT_GFX == IM_GFX_OPENGL3)
#include "shaders/arrow_sdf.glsl.h"
#include "shaders/gradient.glsl.h"
#define DEMO_SHADER_PROGRAM( name, shaders ) CreateDemoProgram( ImPlatform_ShaderFormat_GLSL, name##_vs_glsl, 0, name##_ps_glsl, 0, "main", "main", shaders )
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10) || (IM_CURRENT_GFX == IM_GFX_DIRECTX11) || (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
#include "shaders/arrow_sdf.hlsl.h"
#include "shaders/gradient.hlsl.h"
#define DEMO_SHADER_PROGRAM( name, shaders ) CreateDemoProgram( ImPlatform_ShaderFormat_HLSL, name##_hlsl, 0, name##_hlsl, 0, "vs_main", "ps_main", shaders )
#elif (IM_CURRENT_GFX == IM_GFX_WGPU)
#include "shaders/arrow_sdf.wgsl.h"
#include "shaders/gradient.wgsl.h"
#define DEMO_SHADER_PROGRAM( name, shaders ) CreateDemoProgram( ImPlatform_ShaderFormat_WGSL, name##_vs_wgsl, 0, name##_ps_wgsl, 0, "vs_main", "ps_main", shaders )
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
// --shader-test: the gradient shader (shaders/gradient.hlsl, red to blue) drawn into a render pass target, read back.
// Checks the custom shader path of each backend: compilation, b0 projection, b1 parameters. Exit code as above.
static RenderPassScene g_ShaderTest;
static ImPlatform_ShaderProgram g_ShaderTestProgram = nullptr;
static void DrawShaderTest( ImDrawList* dl )
{
	ImPlatform_BeginCustomShader( dl, g_ShaderTestProgram );
	dl->AddImage( ImGui::GetIO().Fonts->TexRef, ImVec2( 0, 0 ), ImVec2( 64, 64 ) );
	ImPlatform_EndCustomShader( dl );
}

static int CheckShaderTest()
{
	static unsigned char px[ 64 * 64 * 4 ];
	if ( !ImPlatform_ReadTexture( g_ShaderTest.Target, 0, 0, 64, 64, px, sizeof( px ) ) )
	{
		printf( "SKIP: no readback of the pass target\n" );
		return 2;
	}
	// uv.y at the centers of rows 0, 32 and 63: 0.008, 0.508, 0.992
	const RenderPassProbe probes[] = { { 32, 0, { 253, 0, 2, 255 } }, { 32, 32, { 125, 0, 130, 255 } }, { 32, 63, { 2, 0, 253, 255 } } };
	const int bad = CheckRenderPassProbes( "gradient", px, 64, probes, 3, 4 );
	if ( !bad )
		printf( "PASS\n" );
	return bad ? 1 : 0;
}

// --direct-draw-test: from a draw callback inside a render pass, the same gradient drawn by ImPlatform_DrawQuadNow into a
// pushed render target, its colors from ImPlatform_PushShaderConstants, then read back: the direct draws a backdrop
// effect makes (copy, push a target, draw a quad, pop).
static RenderPassScene g_DirectDrawPass;
static ImTextureID g_DirectDrawTarget = 0;
static bool g_DirectDrawPushed = false;
static void DirectDrawCallback( const ImDrawList*, const ImDrawCmd* )
{
	if ( !ImPlatform_PushRenderTarget( g_DirectDrawTarget, true ) )
		return;
	g_DirectDrawPushed = true;
	const ImVec4 colors[ 2 ] = { ImVec4( 1, 0, 0, 1 ), ImVec4( 0, 0, 1, 1 ) };	// gradient.hlsl's ColorStart, ColorEnd
	void* constants = ImPlatform_PushShaderConstants( colors, sizeof( colors ) );
	ImPlatform_DrawQuadNow( g_ShaderTestProgram, ImVec4( 0, 0, 64, 64 ), ImVec4( 0, 0, 1, 1 ) );
	ImPlatform_PopShaderConstants( constants );
	ImPlatform_PopRenderTarget();
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
	if ( !ImPlatform_ReadTexture( g_DirectDrawTarget, 0, 0, 64, 64, px, sizeof( px ) ) )
	{
		printf( "SKIP: no readback of the pushed target\n" );
		return 2;
	}
	const RenderPassProbe probes[] = { { 32, 0, { 253, 0, 2, 255 } }, { 32, 32, { 125, 0, 130, 255 } }, { 32, 63, { 2, 0, 253, 255 } } };
	const int bad = CheckRenderPassProbes( "direct draw", px, 64, probes, 3, 4 );
	if ( !bad )
		printf( "PASS\n" );
	return bad ? 1 : 0;
}
#endif
#endif // IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER

#if IM_CURRENT_GFX == IM_GFX_OPENGL3
#include "shaders/compute_test.glsl.h"
#elif IM_CURRENT_GFX == IM_GFX_VULKAN
#include "shaders/compute_test.spv.h"
#else
#include "shaders/compute_test.hlsl.h"
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
		const bool read = input && ImPlatform_ReadTexture( input, 8, 16, 32, 16, out_px, sizeof( out_px ) );
		int bad = read ? 0 : 1;
		for ( unsigned int y = 0; read && y < 16; ++y )
			bad += memcmp( out_px + y * 32 * 4, in_px + ( ( y + 16 ) * N + 8 ) * 4, 32 * 4 ) != 0;
		printf( bad ? "FAIL (readback only, %d bad rows)\n" : "PASS (readback only)\n", bad );
		return bad ? 1 : 0;
	}
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
	const bool read = dispatched && ImPlatform_ReadTexture( output, 0, 0, N, N, out_px, sizeof( out_px ) );
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
}

int main( int argc, char** argv )
{
	const bool computeTest = argc > 1 && strcmp( argv[ 1 ], "--compute-test" ) == 0;
	const bool renderPassTest = argc > 1 && strcmp( argv[ 1 ], "--render-pass-test" ) == 0;
	const bool shaderTest = argc > 1 && strcmp( argv[ 1 ], "--shader-test" ) == 0;
	const bool directDrawTest = argc > 1 && strcmp( argv[ 1 ], "--direct-draw-test" ) == 0;
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
		if ( !ImPlatform_GfxCheck() && !renderPassTest && !shaderTest && !directDrawTest )
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
		if ( directDrawTest && renderPassFrame == 0 )
		{
#if IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER
			g_ShaderTestProgram = gradient_program;
			ImPlatform_TextureDesc desc = ImPlatform_TextureDesc_Default( 64, 64 );
			desc.format = ImPlatform_GetRenderPassFormat();
			g_DirectDrawTarget = ImPlatform_CreateRenderTexture( &desc );
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
			if ( !renderPassTest && !shaderTest && !directDrawTest )		// One pass per frame: the test's.
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
		if ( ( renderPassTest || shaderTest || directDrawTest ) && ++renderPassFrame == 3 )
		{
#if IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER
			exitCode = shaderTest ? CheckShaderTest() : directDrawTest ? CheckDirectDrawTest() : CheckRenderPassTest();
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
	DestroyRenderPassScene( g_DirectDrawPass );
	if ( g_DirectDrawTarget )
		ImPlatform_DestroyTexture( g_DirectDrawTarget );
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
