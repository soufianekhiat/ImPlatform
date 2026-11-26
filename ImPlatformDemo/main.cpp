
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
	style.FrameRounding = style.GrabRounding = 3.0f;	// As at startup
	style.ScaleAllSizes(new_scale);
	style.FontScaleDpi = new_scale;
#ifdef IMGUI_HAS_VIEWPORT
	if ( ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
	{
		style.WindowRounding = 0.0f;
		style.Colors[ ImGuiCol_WindowBg ].w = 1.0f;
	}
#endif
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
			if ( abs( got[ c ] - p.rgba[ c ] ) > 4 )	// 4: WARP's Direct3D 9 filters a weight of 0.27 as 0.25 (64 for 68)
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
#define DEMO_SHADER_ARGS( name ) ImPlatform_ShaderFormat_SPIRV, name##_vs_spv, sizeof( name##_vs_spv ), name##_ps_spv, sizeof( name##_ps_spv ), "main", "main"
#elif (IM_CURRENT_GFX == IM_GFX_OPENGL3)
#include "shaders/arrow_sdf.glsl.h"
#include "shaders/gradient.glsl.h"
#include "shaders/volume_test.glsl.h"
#define DEMO_SHADER_ARGS( name ) ImPlatform_ShaderFormat_GLSL, name##_vs_glsl, 0, name##_ps_glsl, 0, "main", "main"
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX9) || (IM_CURRENT_GFX == IM_GFX_DIRECTX10) || (IM_CURRENT_GFX == IM_GFX_DIRECTX11) || (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
#include "shaders/arrow_sdf.hlsl.h"
#include "shaders/gradient.hlsl.h"
#include "shaders/volume_test.hlsl.h"
#define DEMO_SHADER_ARGS( name ) ImPlatform_ShaderFormat_HLSL, name##_hlsl, 0, name##_hlsl, 0, "vs_main", "ps_main"
#elif (IM_CURRENT_GFX == IM_GFX_WGPU)
#include "shaders/arrow_sdf.wgsl.h"
#include "shaders/gradient.wgsl.h"
#include "shaders/volume_test.wgsl.h"
#define DEMO_SHADER_ARGS( name ) ImPlatform_ShaderFormat_WGSL, name##_vs_wgsl, 0, name##_ps_wgsl, 0, "vs_main", "ps_main"
#elif (IM_CURRENT_GFX == IM_GFX_METAL)
#include "shaders/arrow_sdf.metal.h"
#include "shaders/gradient.metal.h"
#include "shaders/volume_test.metal.h"
#define DEMO_SHADER_ARGS( name ) ImPlatform_ShaderFormat_MSL, name##_vs_metal, 0, name##_ps_metal, 0, "vs_main", "ps_main"
#else
#define DEMO_SHADER_NONE	// No demo shader for this backend
#define DEMO_SHADER_PROGRAM( name, shaders ) nullptr
#define DEMO_SHADER_JOB( name ) nullptr
#endif
#ifndef DEMO_SHADER_NONE
#define DEMO_SHADER_PROGRAM( name, shaders ) CreateDemoProgram( DEMO_SHADER_ARGS( name ), shaders )
#define DEMO_SHADER_JOB( name ) CreateDemoJob( DEMO_SHADER_ARGS( name ) )
#endif

#ifndef DEMO_SHADER_NONE
// The descriptor of a stage; size 0: code is source text.
static ImPlatform_ShaderDesc DemoShaderDesc( ImPlatform_ShaderStage stage, ImPlatform_ShaderFormat format, const void* code, size_t size, const char* entry )
{
	ImPlatform_ShaderDesc desc = {};
	desc.stage = stage;
	desc.format = format;
	desc.entry_point = entry;
	if ( size )
	{
		desc.bytecode = code;
		desc.bytecode_size = (unsigned int)size;
	}
	else
		desc.source_code = (const char*)code;
	return desc;
}

// Creates both stages (kept in shaders[] to be destroyed) and the program.
static ImPlatform_ShaderProgram CreateDemoProgram( ImPlatform_ShaderFormat format, const void* vs, size_t vs_size, const void* ps, size_t ps_size,
												   const char* vs_entry, const char* ps_entry, ImPlatform_Shader shaders[ 2 ] )
{
	const ImPlatform_ShaderDesc vs_desc = DemoShaderDesc( ImPlatform_ShaderStage_Vertex, format, vs, vs_size, vs_entry );
	const ImPlatform_ShaderDesc ps_desc = DemoShaderDesc( ImPlatform_ShaderStage_Fragment, format, ps, ps_size, ps_entry );
	shaders[ 0 ] = ImPlatform_CreateShader( &vs_desc );
	shaders[ 1 ] = ImPlatform_CreateShader( &ps_desc );
	return shaders[ 0 ] && shaders[ 1 ] ? ImPlatform_CreateShaderProgram( shaders[ 0 ], shaders[ 1 ] ) : nullptr;
}

#ifndef __EMSCRIPTEN__
// The same program compiled without blocking (ImPlatform_GetShaderJobStatus tells when it is ready); --shader-async-test.
static ImPlatform_ShaderJob CreateDemoJob( ImPlatform_ShaderFormat format, const void* vs, size_t vs_size, const void* ps, size_t ps_size,
										   const char* vs_entry, const char* ps_entry )
{
	const ImPlatform_ShaderDesc vs_desc = DemoShaderDesc( ImPlatform_ShaderStage_Vertex, format, vs, vs_size, vs_entry );
	const ImPlatform_ShaderDesc ps_desc = DemoShaderDesc( ImPlatform_ShaderStage_Fragment, format, ps, ps_size, ps_entry );
	return ImPlatform_CreateShaderProgramAsync( &vs_desc, &ps_desc );
}
#endif
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

// --sampler-test: a texture of two texels, black and white, stretched over rows of a render pass target and read back.
// With the nearest sampler pushed on the pass's draw list (ImPlatform_PushSamplerEx) a row has a step, without it a
// ramp. A custom shader drawn inside the sampler scope, then inside a premultiplied alpha scope, leaves both in place
// for the draws after ImPlatform_EndCustomShader (the last row: half transparent blue over gray). Exit code as above.
static RenderPassScene g_SamplerTest;
static ImTextureID g_SamplerTestTexels = 0, g_SamplerTestBlue = 0;
static void DrawSamplerTestShader( ImDrawList* dl )
{
	static const ImVec4 colors[ 2 ] = { ImVec4( 1, 0, 0, 1 ), ImVec4( 0, 0, 1, 1 ) };
	dl->AddCallback( SetShaderTestColors, (void*)colors );
	ImPlatform_BeginCustomShader( dl, g_ShaderTestProgram );
	dl->AddImage( ImGui::GetIO().Fonts->TexRef, ImVec2( 0, 48 ), ImVec2( 8, 64 ) );
	ImPlatform_EndCustomShader( dl );
}
static void DrawSamplerTest( ImDrawList* dl )
{
	ImPlatform_PushSamplerEx( dl, ImPlatform_TextureFilter_Nearest, ImPlatform_TextureWrap_Clamp );
	dl->AddImage( g_SamplerTestTexels, ImVec2( 0, 0 ), ImVec2( 64, 16 ) );
	DrawSamplerTestShader( dl );
	dl->AddImage( g_SamplerTestTexels, ImVec2( 0, 32 ), ImVec2( 64, 48 ) );
	ImPlatform_PopSamplerEx( dl );
	dl->AddImage( g_SamplerTestTexels, ImVec2( 0, 16 ), ImVec2( 64, 32 ) );
	dl->AddRectFilled( ImVec2( 16, 48 ), ImVec2( 64, 64 ), IM_COL32( 100, 100, 100, 255 ) );
	ImPlatform_PushPremultipliedAlpha( dl );
	DrawSamplerTestShader( dl );
	dl->AddImage( g_SamplerTestBlue, ImVec2( 16, 48 ), ImVec2( 64, 64 ) );
	ImPlatform_PopPremultipliedAlpha( dl );
}

static bool CreateSamplerTest()
{
	const unsigned char texels[ 2 * 4 ] = { 0, 0, 0, 255, 255, 255, 255, 255 };
	const unsigned char blue[ 4 ] = { 0, 0, 128, 128 };	// Premultiplied
	ImPlatform_TextureDesc desc = ImPlatform_TextureDesc_Default( 2, 1 );
	g_SamplerTestTexels = ImPlatform_CreateTexture( texels, &desc );
	desc = ImPlatform_TextureDesc_Default( 1, 1 );
	g_SamplerTestBlue = ImPlatform_CreateTexture( blue, &desc );
	return g_ShaderTestProgram && g_SamplerTestTexels && g_SamplerTestBlue && QueueRenderPassScene( g_SamplerTest, 64, DrawSamplerTest );
}

static void DestroySamplerTest()
{
	if ( g_SamplerTestTexels )
		ImPlatform_DestroyTexture( g_SamplerTestTexels );
	if ( g_SamplerTestBlue )
		ImPlatform_DestroyTexture( g_SamplerTestBlue );
	g_SamplerTestTexels = g_SamplerTestBlue = 0;
}

static int CheckSamplerTest()
{
	static unsigned char px[ 64 * 64 * 4 ];
	if ( !DemoReadTexture( g_SamplerTest.Target, 0, 0, 64, 64, px, sizeof( px ) ) )
	{
		printf( "SKIP: no readback of the pass target\n" );
		return 2;
	}
	// The texel centers are at a quarter and three quarters of the row: columns 24 and 40 are 0.27 and 0.77 of the way
	// between them. Blue at half coverage over gray (100) with premultiplied alpha is 128 + 100 * 127 / 255.
	const RenderPassProbe probes[] = { { 24, 8, { 0, 0, 0, 255 } }, { 40, 8, { 255, 255, 255, 255 } },			// Nearest
									   { 24, 24, { 68, 68, 68, 255 } }, { 40, 24, { 195, 195, 195, 255 } },		// Linear, after the pop
									   { 24, 40, { 0, 0, 0, 255 } }, { 40, 40, { 255, 255, 255, 255 } },		// Nearest, after a custom shader
									   { 40, 56, { 50, 50, 178, 255 } } };										// Premultiplied, after a custom shader
	const int bad = CheckRenderPassProbes( "sampler", px, 64, probes, 7, 4 );
	if ( !bad )
		printf( "PASS\n" );
	return bad ? 1 : 0;
}

// --buffer-update-test: a Dynamic and a Stream vertex buffer of one quad each (the vertex layout of shaders/gradient.hlsl)
// drawn by ImPlatform_DrawIndexed from a draw callback into a render pass, three frames in a row. In a frame each buffer
// is written (the quad without height, then its lower vertices as a partial update, which must keep the upper ones),
// drawn on the left, written again (the Dynamic one in two parts) and drawn on the right: the left draw must keep what
// it was recorded with, and so must the frames still in flight when the next frame writes. The Stream buffer is created
// again every frame, which takes the one destroyed the frame before where the backend keeps them. Left
// quads green (uv.y 0), right ones blue (uv.y 1). Exit code as above.
static RenderPassScene g_BufferUpdateTest;
#if IMPLATFORM_GFX_SUPPORT_VERTEX_BUFFERS
static ImPlatform_VertexBuffer g_BufferUpdateVB[ 2 ] = {};	// Dynamic, Stream
static ImPlatform_IndexBuffer g_BufferUpdateIB = nullptr;
static bool g_BufferUpdateOk = true;
struct BufferUpdateVertex { float pos[ 2 ]; float col[ 4 ]; float uv[ 2 ]; };
static void FillBufferUpdateQuad( BufferUpdateVertex v[ 4 ], float x, float y, float height, float end )
{
	const float xs[ 4 ] = { x, x + 32, x + 32, x }, ys[ 4 ] = { y, y, y + height, y + height };
	for ( int i = 0; i < 4; ++i )
		v[ i ] = { { xs[ i ], ys[ i ] }, { 1, 1, 1, 1 }, { 0, end } };
}
static void WriteBufferUpdate( int buffer, const BufferUpdateVertex* v, unsigned int offset, unsigned int count )
{
	if ( !ImPlatform_UpdateVertexBuffer( g_BufferUpdateVB[ buffer ], v, offset, count ) )
		g_BufferUpdateOk = false;
}
static void DrawBufferUpdateQuads( const ImDrawList*, const ImDrawCmd* )
{
	for ( int b = 0; b < 2; ++b )
	{
		const float y = 32.0f * b;
		BufferUpdateVertex v[ 4 ], lower[ 4 ];
		FillBufferUpdateQuad( v, 0, y, 0, 0 );
		FillBufferUpdateQuad( lower, 0, y, 32, 0 );
		WriteBufferUpdate( b, v, 0, 4 );
		WriteBufferUpdate( b, lower + 2, 2, 2 );
		ImPlatform_BindBuffers( g_BufferUpdateVB[ b ], g_BufferUpdateIB );
		ImPlatform_DrawIndexed( 0, 6, 0 );
		FillBufferUpdateQuad( v, 32, y, 32, 1 );
		if ( b == 0 )
		{
			// The Dynamic buffer in two parts, after its draw: each part keeps the rest.
			WriteBufferUpdate( b, v, 0, 2 );
			WriteBufferUpdate( b, v + 2, 2, 2 );
		}
		else
			WriteBufferUpdate( b, v, 0, 4 );	// A Stream buffer that was drawn takes the whole buffer only
		ImPlatform_BindBuffers( g_BufferUpdateVB[ b ], g_BufferUpdateIB );
		ImPlatform_DrawIndexed( 0, 6, 0 );
	}
}
static void DrawBufferUpdateTest( ImDrawList* dl )
{
	static const ImVec4 colors[ 2 ] = { ImVec4( 0, 1, 0, 1 ), ImVec4( 0, 0, 1, 1 ) };
	dl->AddCallback( SetShaderTestColors, (void*)colors );
	ImPlatform_BeginCustomShader( dl, g_ShaderTestProgram );
	dl->AddCallback( DrawBufferUpdateQuads, nullptr );
	ImPlatform_EndCustomShader( dl );
}
#endif

// Called each frame of the test.
static bool QueueBufferUpdateTest()
{
#if IMPLATFORM_GFX_SUPPORT_VERTEX_BUFFERS
	static const ImPlatform_VertexAttribute attributes[ 3 ] = {
		{ ImPlatform_VertexFormat_Float2, 0, "POSITION" }, { ImPlatform_VertexFormat_Float4, 8, "COLOR" }, { ImPlatform_VertexFormat_Float2, 24, "TEXCOORD" } };
	BufferUpdateVertex v[ 4 ];
	FillBufferUpdateQuad( v, 0, 0, 32, 0 );
	ImPlatform_VertexBufferDesc vb = {};
	vb.vertex_count = 4;
	vb.vertex_stride = sizeof( BufferUpdateVertex );
	vb.attributes = attributes;
	vb.attribute_count = 3;
	if ( !g_BufferUpdateIB )
	{
		const unsigned short indices[ 6 ] = { 0, 1, 2, 0, 2, 3 };
		ImPlatform_IndexBufferDesc ib = {};
		ib.index_count = 6;
		ib.format = ImPlatform_IndexFormat_UInt16;
		ib.usage = ImPlatform_BufferUsage_Static;
		g_BufferUpdateIB = ImPlatform_CreateIndexBuffer( indices, &ib );
		vb.usage = ImPlatform_BufferUsage_Dynamic;
		g_BufferUpdateVB[ 0 ] = ImPlatform_CreateVertexBuffer( v, &vb );
	}
	// A Stream buffer holds for a frame: the previous frame's has its draws recorded and goes.
	if ( g_BufferUpdateVB[ 1 ] )
		ImPlatform_DestroyVertexBuffer( g_BufferUpdateVB[ 1 ] );
	vb.usage = ImPlatform_BufferUsage_Stream;
	g_BufferUpdateVB[ 1 ] = ImPlatform_CreateVertexBuffer( v, &vb );
	if ( !g_BufferUpdateIB || !g_BufferUpdateVB[ 0 ] || !g_BufferUpdateVB[ 1 ]
		 || !ImPlatform_CreateVertexInputLayout( g_BufferUpdateVB[ 0 ], g_ShaderTestProgram )
		 || !ImPlatform_CreateVertexInputLayout( g_BufferUpdateVB[ 1 ], g_ShaderTestProgram ) )
		return false;
	return QueueRenderPassScene( g_BufferUpdateTest, 64, DrawBufferUpdateTest );
#else
	return false;
#endif
}

static int CheckBufferUpdateTest()
{
	static unsigned char px[ 64 * 64 * 4 ];
	if ( !DemoReadTexture( g_BufferUpdateTest.Target, 0, 0, 64, 64, px, sizeof( px ) ) )
	{
		printf( "SKIP: no readback of the pass target\n" );
		return 2;
	}
	const RenderPassProbe probes[] = { { 16, 16, { 0, 255, 0, 255 } }, { 48, 16, { 0, 0, 255, 255 } },
									   { 16, 48, { 0, 255, 0, 255 } }, { 48, 48, { 0, 0, 255, 255 } } };
	int bad = CheckRenderPassProbes( "buffer update", px, 64, probes, 4, 4 );
#if IMPLATFORM_GFX_SUPPORT_VERTEX_BUFFERS
	if ( !g_BufferUpdateOk )
	{
		printf( "FAIL: an update of a Dynamic or Stream vertex buffer returned false\n" );
		++bad;
	}
#endif
	if ( !bad )
		printf( "PASS\n" );
	return bad ? 1 : 0;
}

static void DestroyBufferUpdateTest()
{
#if IMPLATFORM_GFX_SUPPORT_VERTEX_BUFFERS
	for ( ImPlatform_VertexBuffer buffer : g_BufferUpdateVB )
		if ( buffer )
			ImPlatform_DestroyVertexBuffer( buffer );
	if ( g_BufferUpdateIB )
		ImPlatform_DestroyIndexBuffer( g_BufferUpdateIB );
#endif
}

// --mesh-stream-test (DirectX 11 and 12, the backends with ImPlatform_DrawMesh): three frames in a row, into a render
// texture with depth, a Stream vertex buffer is created, drawn (left half, green) and destroyed, then a Dynamic one is
// written and drawn twice (upper right red, lower right blue): each draw keeps the content it was recorded with, and
// the frames in flight keep theirs. Exit code as above.
static ImTextureID g_MeshStreamTarget = 0;
static ImPlatform_ShaderProgram g_MeshStreamProgram = nullptr;
static ImPlatform_Shader g_MeshStreamShaders[ 2 ] = {};
#if IMPLATFORM_GFX_SUPPORT_DEPTH_DRAW && IMPLATFORM_GFX_SUPPORT_VERTEX_BUFFERS
static ImPlatform_VertexBuffer g_MeshStreamDynamic = nullptr;
static ImPlatform_IndexBuffer g_MeshStreamIB = nullptr;
struct MeshStreamVertex { float pos[ 2 ]; float col[ 4 ]; };
static void FillMeshStreamQuad( MeshStreamVertex v[ 4 ], float x0, float y0, float x1, float y1, float r, float g, float b )
{
	const float xs[ 4 ] = { x0, x1, x1, x0 }, ys[ 4 ] = { y0, y0, y1, y1 };
	for ( int i = 0; i < 4; ++i )
		v[ i ] = { { xs[ i ], ys[ i ] }, { r, g, b, 1 } };
}
#endif

// One frame of the test. 0: drawn, 1: a call failed, 2: not supported.
static int RunMeshStreamFrame()
{
#if IMPLATFORM_GFX_SUPPORT_DEPTH_DRAW && IMPLATFORM_GFX_SUPPORT_VERTEX_BUFFERS
	// Positions are clip space
	static const char* hlsl =
		"struct VS_INPUT { float2 pos : POSITION; float4 col : COLOR0; };\n"
		"struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR0; };\n"
		"PS_INPUT vs_main(VS_INPUT input) { PS_INPUT output; output.pos = float4(input.pos, 0.5, 1.0); output.col = input.col; return output; }\n"
		"float4 ps_main(PS_INPUT input) : SV_Target { return input.col; }\n";
	static const ImPlatform_VertexAttribute attributes[ 2 ] = {
		{ ImPlatform_VertexFormat_Float2, 0, "POSITION" }, { ImPlatform_VertexFormat_Float4, 8, "COLOR" } };
	MeshStreamVertex v[ 4 ];
	FillMeshStreamQuad( v, -1, -1, 0, 1, 0, 1, 0 );
	ImPlatform_VertexBufferDesc vb = {};
	vb.vertex_count = 4;
	vb.vertex_stride = sizeof( MeshStreamVertex );
	vb.attributes = attributes;
	vb.attribute_count = 2;
	if ( !g_MeshStreamTarget )
	{
		const ImPlatform_ShaderDesc vs = DemoShaderDesc( ImPlatform_ShaderStage_Vertex, ImPlatform_ShaderFormat_HLSL, hlsl, 0, "vs_main" );
		const ImPlatform_ShaderDesc ps = DemoShaderDesc( ImPlatform_ShaderStage_Fragment, ImPlatform_ShaderFormat_HLSL, hlsl, 0, "ps_main" );
		g_MeshStreamShaders[ 0 ] = ImPlatform_CreateShader( &vs );
		g_MeshStreamShaders[ 1 ] = ImPlatform_CreateShader( &ps );
		if ( g_MeshStreamShaders[ 0 ] && g_MeshStreamShaders[ 1 ] )
			g_MeshStreamProgram = ImPlatform_CreateShaderProgram( g_MeshStreamShaders[ 0 ], g_MeshStreamShaders[ 1 ] );
		const unsigned short indices[ 6 ] = { 0, 1, 2, 0, 2, 3 };
		ImPlatform_IndexBufferDesc ib = {};
		ib.index_count = 6;
		ib.format = ImPlatform_IndexFormat_UInt16;
		ib.usage = ImPlatform_BufferUsage_Static;
		g_MeshStreamIB = ImPlatform_CreateIndexBuffer( indices, &ib );
		vb.usage = ImPlatform_BufferUsage_Dynamic;
		g_MeshStreamDynamic = ImPlatform_CreateVertexBuffer( v, &vb );
		const ImPlatform_TextureDesc desc = ImPlatform_TextureDesc_Default( 64, 64 );
		g_MeshStreamTarget = ImPlatform_CreateRenderTextureWithDepth( &desc );
		if ( !g_MeshStreamProgram || !g_MeshStreamIB || !g_MeshStreamDynamic || !g_MeshStreamTarget
			 || !ImPlatform_CreateVertexInputLayout( g_MeshStreamDynamic, g_MeshStreamProgram ) )
			return 2;
	}
	vb.usage = ImPlatform_BufferUsage_Stream;
	ImPlatform_VertexBuffer stream = ImPlatform_CreateVertexBuffer( v, &vb );
	bool ok = stream && ImPlatform_CreateVertexInputLayout( stream, g_MeshStreamProgram ) && ImPlatform_BeginRenderToTexture( g_MeshStreamTarget );
	if ( ok )
	{
		ImPlatform_DrawMeshDesc mesh = {};
		mesh.program = g_MeshStreamProgram;
		mesh.indices = g_MeshStreamIB;
		mesh.index_count = 6;
		mesh.vertices = stream;
		ok = ImPlatform_DrawMesh( &mesh );
		mesh.vertices = g_MeshStreamDynamic;
		FillMeshStreamQuad( v, 0, 0, 1, 1, 1, 0, 0 );
		ok = ImPlatform_UpdateVertexBuffer( g_MeshStreamDynamic, v, 0, 4 ) && ImPlatform_DrawMesh( &mesh ) && ok;
		FillMeshStreamQuad( v, 0, -1, 1, 0, 0, 0, 1 );
		ok = ImPlatform_UpdateVertexBuffer( g_MeshStreamDynamic, v, 0, 4 ) && ImPlatform_DrawMesh( &mesh ) && ok;
		ImPlatform_EndRenderToTexture();
	}
	if ( stream )
		ImPlatform_DestroyVertexBuffer( stream );	// Its draw is recorded: the frame keeps the memory, the handle goes.
	return ok ? 0 : 1;
#else
	return 2;
#endif
}

static int CheckMeshStreamTest()
{
	static unsigned char px[ 64 * 64 * 4 ];
	if ( !DemoReadTexture( g_MeshStreamTarget, 0, 0, 64, 64, px, sizeof( px ) ) )
	{
		printf( "SKIP: no readback of the render texture\n" );
		return 2;
	}
	const RenderPassProbe probes[] = { { 16, 32, { 0, 255, 0, 255 } }, { 48, 16, { 255, 0, 0, 255 } }, { 48, 48, { 0, 0, 255, 255 } } };
	const int bad = CheckRenderPassProbes( "mesh stream", px, 64, probes, 3, 4 );
	if ( !bad )
		printf( "PASS\n" );
	return bad ? 1 : 0;
}

static void DestroyMeshStreamTest()
{
	if ( g_MeshStreamProgram )
		ImPlatform_DestroyShaderProgram( g_MeshStreamProgram );
	for ( ImPlatform_Shader shader : g_MeshStreamShaders )
		if ( shader )
			ImPlatform_DestroyShader( shader );
#if IMPLATFORM_GFX_SUPPORT_DEPTH_DRAW && IMPLATFORM_GFX_SUPPORT_VERTEX_BUFFERS
	if ( g_MeshStreamDynamic )
		ImPlatform_DestroyVertexBuffer( g_MeshStreamDynamic );
	if ( g_MeshStreamIB )
		ImPlatform_DestroyIndexBuffer( g_MeshStreamIB );
#endif
	if ( g_MeshStreamTarget )
		ImPlatform_DestroyTexture( g_MeshStreamTarget );
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

// The demo's shader parts, shared by the tests and --dump-msl.
static bool GetPartsTestParts( ImPlatform_ShaderParts* parts )
{
	// The parts as one text, as a shader file would hold them (ImPlatform_ParseShaderParts splits it in place, once).
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
	static ImPlatform_ShaderParts parsed = {};
	static const bool ok = ImPlatform_ParseShaderParts( text, &parsed );
	*parts = parsed;
#if IM_CURRENT_GFX == IM_GFX_DIRECTX9
	parts->defines = "TINT_SCALE=1.0";	// Shader model 3 has no bit operations: no BITS
#endif
	return ok;
}

static void GetTexture3DParts( ImPlatform_ShaderParts* parts )
{
	*parts = {};
	parts->textures[ 1 ] = "volume";
	parts->textures_3d = 1u << 1;
	parts->ps_body = "float3 uvw = v.uv.x < 0.5 ? float3(0.25, 0.75, 0.75) : float3(0.75, 0.25, 0.25);\n"
					 "col_out = IMP_SAMPLE_LEVEL(volume, uvw, 0.0);\n";
}

static void GetVertexInputsParts( ImPlatform_ShaderParts* parts )
{
	*parts = {};
	parts->vertex_inputs = "float4 pos : POSITION; float4 data : TEXCOORD0;";
#if IM_CURRENT_GFX == IM_GFX_DIRECTX9
	parts->varyings = "float4 id;";	// Shader model 3: no integer or flat varyings
#else
	parts->varyings = "nointerpolation int4 id;";
#endif
	parts->vs_body = "float4 p = float4(in_pos.xy, 0.0, 1.0);\n"
					 "position = float4(dot(IMP_MUL(float4(1.0, 0.0, 0.0, 0.0), ProjMtx), p), dot(IMP_MUL(float4(0.0, 1.0, 0.0, 0.0), ProjMtx), p),\n"
					 "                  dot(IMP_MUL(float4(0.0, 0.0, 1.0, 0.0), ProjMtx), p), dot(IMP_MUL(float4(0.0, 0.0, 0.0, 1.0), ProjMtx), p));\n"
					 "v.id = int4(IMP_CAST(int, in_data.x + 0.5), 0, 0, 0);\n";
	parts->ps_body = "col_out = v.id.x == 1 ? float4(0.0, 1.0, 0.0, 1.0) : v.id.x == 2 ? float4(0.0, 0.0, 1.0, 1.0) : float4(1.0, 0.0, 0.0, 1.0);\n";
}

// --dump-msl <shader_parts|texture3d|vertex_inputs> <vs|ps>: prints the MSL generated from those parts, without a window
// or device, for the offline Metal compiler (CI: the hosted macOS runners have no GPU to run the Metal tests).
static int DumpMSL( const char* name, const char* stage )
{
	ImPlatform_ShaderParts parts = {};
	if ( strcmp( name, "shader_parts" ) == 0 )
	{
		if ( !GetPartsTestParts( &parts ) )
			return 1;
	}
	else if ( strcmp( name, "texture3d" ) == 0 )
		GetTexture3DParts( &parts );
	else if ( strcmp( name, "vertex_inputs" ) == 0 )
		GetVertexInputsParts( &parts );
	else
		return 1;
	static char source[ 1 << 16 ];
	const ImPlatform_ShaderStage st = strcmp( stage, "vs" ) == 0 ? ImPlatform_ShaderStage_Vertex : ImPlatform_ShaderStage_Fragment;
	const int len = ImPlatform_GenerateShaderSource( &parts, st, ImPlatform_ShaderFormat_MSL, source, (int)sizeof( source ) );
	if ( len < 0 || len >= (int)sizeof( source ) )
		return 1;
	fwrite( source, 1, (size_t)len, stdout );
	return 0;
}

static bool CreatePartsTest()
{
#if IMPLATFORM_GFX_SUPPORT_SHADER_PARTS && IMPLATFORM_GFX_SUPPORT_SHADER_TEXTURES
	ImPlatform_ShaderParts parts = {};
	if ( !GetPartsTestParts( &parts ) )
		return false;
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
	ImPlatform_ShaderParts parts;
	GetTexture3DParts( &parts );
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
// applied row by row (IMP_MUL(float4(1, 0, 0, 0), ProjMtx)...), so a wrong row shows. Below, the left quarter stays
// clear and the right one is a regular rect drawn after the callback (it must not use the buffers DrawIndexed bound).
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
	// Only callbacks in this list: BeginCustomShader makes the backend set the scissor they run with.
	ImPlatform_BeginCustomShader( dl, g_VertexInputsProgram );
	dl->AddCallback( DrawVertexInputsQuads, nullptr );
	ImPlatform_EndCustomShader( dl );
	// A regular draw after the callback: it must use Dear ImGui's buffers again, not the ones DrawIndexed bound.
	dl->AddRectFilled( ImVec2( 32, 32 ), ImVec2( 64, 64 ), IM_COL32( 255, 255, 255, 255 ) );
}
#endif

static bool CreateVertexInputsTest()
{
#if IMPLATFORM_GFX_SUPPORT_SHADER_PARTS && IMPLATFORM_GFX_SUPPORT_VERTEX_BUFFERS
	ImPlatform_ShaderParts parts;
	GetVertexInputsParts( &parts );
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
	const RenderPassProbe probes[] = { { 16, 16, { 0, 255, 0, 255 } }, { 48, 16, { 0, 0, 255, 255 } }, { 16, 48, { 0, 0, 0, 0 } },
									   { 48, 48, { 255, 255, 255, 255 } } };
	const int bad = CheckRenderPassProbes( "vertex inputs", px, 64, probes, 4, 4 );
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

// --blend-test: the gradient program drawn in six 10 pixel columns over opaque gray (64) with a constant color
// (0.5, 0.25, 0, 1), its blending set by name from a draw callback before each column
// (ImPlatform_SetShaderProgramBlendPreset is read when the program is bound): additive, subtractive, multiplicative,
// screen, darken, lighten. All keep the target's alpha. Exit code as above.
static RenderPassScene g_BlendTest;
static const ImPlatform_BlendPreset g_BlendTestPresets[ 6 ] = { ImPlatform_BlendPreset_Additive, ImPlatform_BlendPreset_Subtractive,
																 ImPlatform_BlendPreset_Multiplicative, ImPlatform_BlendPreset_Screen,
																 ImPlatform_BlendPreset_Darken, ImPlatform_BlendPreset_Lighten };
static void SetBlendTestPreset( const ImDrawList*, const ImDrawCmd* cmd )
{
	static const ImVec4 colors[ 2 ] = { ImVec4( 0.5f, 0.25f, 0, 1 ), ImVec4( 0.5f, 0.25f, 0, 1 ) };
	ImPlatform_SetShaderProgramBlendPreset( g_ShaderTestProgram, *(const ImPlatform_BlendPreset*)cmd->UserCallbackData );
	ImPlatform_SetShaderUniform( g_ShaderTestProgram, "pixelBuffer", colors, sizeof( colors ) );
}
static void DrawBlendTest( ImDrawList* dl )
{
	dl->AddRectFilled( ImVec2( 0, 0 ), ImVec2( 64, 64 ), IM_COL32( 64, 64, 64, 255 ) );
	for ( int i = 0; i < 6; i++ )
	{
		dl->AddCallback( SetBlendTestPreset, (void*)&g_BlendTestPresets[ i ] );
		ImPlatform_BeginCustomShader( dl, g_ShaderTestProgram );
		dl->AddImage( ImGui::GetIO().Fonts->TexRef, ImVec2( 10.0f * i, 0 ), ImVec2( 10.0f * i + 10.0f, 64 ) );
		ImPlatform_EndCustomShader( dl );
	}
}

static int CheckBlendTest()
{
	static unsigned char px[ 64 * 64 * 4 ];
	if ( !DemoReadTexture( g_BlendTest.Target, 0, 0, 64, 64, px, sizeof( px ) ) )
	{
		printf( "SKIP: no readback of the pass target\n" );
		return 2;
	}
	// Target (64, 64, 64), source (128, 64, 0): sum, difference, product, screen 128 + 64 * (1 - 0.5), minimum, maximum
	const RenderPassProbe probes[] = { { 5, 32, { 192, 128, 64, 255 } }, { 15, 32, { 0, 0, 64, 255 } }, { 25, 32, { 32, 16, 0, 255 } },
									   { 35, 32, { 160, 112, 64, 255 } }, { 45, 32, { 64, 64, 0, 255 } }, { 55, 32, { 128, 64, 64, 255 } },
									   { 62, 32, { 64, 64, 64, 255 } } };
	const int bad = CheckRenderPassProbes( "blend", px, 64, probes, 7, 4 );
	if ( !bad )
		printf( "PASS\n" );
	return bad ? 1 : 0;
}

// --shader-async-test: programs compiled with ImPlatform_CreateShaderProgramAsync, drawn into a render pass once ready
// and read back. Job 0 is the gradient program (every backend), drawn blue on the left. With shader parts, job 1 is a
// permutation selected by ImPlatform_ShaderDesc::defines (MODE=2: green on the right, red otherwise) and job 2 a pixel
// shader that does not compile: it must end Failed with the compiler's messages in its log, as ImPlatform_GetShaderLog
// has them for the same shader compiled synchronously. Exit code as above.
static RenderPassScene g_AsyncTest;
static ImPlatform_ShaderJob g_AsyncJobs[ 3 ] = {};
static ImPlatform_ShaderProgram g_AsyncPrograms[ 2 ] = {};
static int g_AsyncFrames = 0;			// Frames drawn while a job was compiling
static bool g_AsyncSyncLog = false;		// The synchronous compile of job 2's shader failed with a log
static void DrawAsyncTest( ImDrawList* dl )
{
	for ( int i = 0; i < 2; i++ )
		if ( g_AsyncPrograms[ i ] )
		{
			ImPlatform_BeginCustomShader( dl, g_AsyncPrograms[ i ] );
			dl->AddImage( ImGui::GetIO().Fonts->TexRef, ImVec2( 32.0f * i, 0 ), ImVec2( 32.0f * i + 32.0f, 64 ) );
			ImPlatform_EndCustomShader( dl );
		}
}

// Called each frame: negative while a job is compiling, 0 once the scene is queued, else the exit code.
static int UpdateAsyncTest()
{
	if ( !g_AsyncJobs[ 0 ] )
	{
		g_AsyncJobs[ 0 ] = DEMO_SHADER_JOB( gradient );
		if ( !g_AsyncJobs[ 0 ] )
		{
			printf( "SKIP: no custom shader on this backend\n" );
			return 2;
		}
#if IMPLATFORM_GFX_SUPPORT_SHADER_PARTS
#if IM_CURRENT_GFX == IM_GFX_OPENGL3
		const ImPlatform_ShaderFormat format = ImPlatform_ShaderFormat_GLSL;
#elif IM_CURRENT_GFX == IM_GFX_METAL
		const ImPlatform_ShaderFormat format = ImPlatform_ShaderFormat_MSL;
#else
		const ImPlatform_ShaderFormat format = ImPlatform_ShaderFormat_HLSL;
#endif
		ImPlatform_ShaderParts parts = {};
		parts.ps_body = "#if MODE == 2\ncol_out = float4(0.0, 1.0, 0.0, 1.0);\n#else\ncol_out = float4(1.0, 0.0, 0.0, 1.0);\n#endif\n";
		static char source[ 2 ][ 1 << 14 ];
		ImPlatform_ShaderDesc desc[ 2 ] = {};
		for ( int i = 0; i < 2; i++ )
		{
			desc[ i ].stage = i == 0 ? ImPlatform_ShaderStage_Vertex : ImPlatform_ShaderStage_Fragment;
			desc[ i ].format = format;
			const int len = ImPlatform_GenerateShaderSource( &parts, desc[ i ].stage, format, source[ i ], (int)sizeof( source[ i ] ) );
			if ( len < 0 || len >= (int)sizeof( source[ i ] ) )
			{
				printf( "FAIL: shader source generation\n" );
				return 1;
			}
			desc[ i ].source_code = source[ i ];
			desc[ i ].entry_point = format == ImPlatform_ShaderFormat_GLSL ? "main" : i == 0 ? "vs_main" : "ps_main";
#if IM_CURRENT_GFX == IM_GFX_DIRECTX9
			desc[ i ].defines = "MODE=2;IMP_SM3=1;nointerpolation";	// As ImPlatform_CreateShaderFromParts defines them for shader model 3
#else
			desc[ i ].defines = "UNUSED;MODE=2";
#endif
		}
		g_AsyncJobs[ 1 ] = ImPlatform_CreateShaderProgramAsync( &desc[ 0 ], &desc[ 1 ] );
		parts.ps_body = "col_out = undeclared_value;\n";
		g_AsyncJobs[ 2 ] = ImPlatform_CreateShaderProgramFromPartsAsync( &parts );
		g_AsyncSyncLog = !ImPlatform_CreateShaderFromParts( &parts, ImPlatform_ShaderStage_Fragment ) && strstr( ImPlatform_GetShaderLog(), "undeclared_value" );
		if ( !g_AsyncJobs[ 1 ] || !g_AsyncJobs[ 2 ] )
		{
			printf( "FAIL: job creation\n" );
			return 1;
		}
#endif
		printf( "after creation: %s\n", ImPlatform_GetShaderJobStatus( g_AsyncJobs[ 0 ], nullptr ) == ImPlatform_ShaderStatus_Compiling ? "compiling" : "compiled" );
	}
	bool compiling = false;
	for ( int i = 0; i < 3; i++ )
		if ( g_AsyncJobs[ i ] && ImPlatform_GetShaderJobStatus( g_AsyncJobs[ i ], i < 2 ? &g_AsyncPrograms[ i ] : nullptr ) == ImPlatform_ShaderStatus_Compiling )
			compiling = true;
	if ( compiling )
	{
		if ( ++g_AsyncFrames < 3000 )
			return -1;
		printf( "FAIL: still compiling after %d frames\n", g_AsyncFrames );
		return 1;
	}
	for ( int i = 0; i < 2; i++ )
		if ( g_AsyncJobs[ i ] && !g_AsyncPrograms[ i ] )
		{
			printf( "FAIL: job %d: %s\n", i, ImPlatform_GetShaderJobLog( g_AsyncJobs[ i ] ) );
			return 1;
		}
	const ImVec4 blue[ 2 ] = { ImVec4( 0, 0, 1, 1 ), ImVec4( 0, 0, 1, 1 ) };
	ImPlatform_SetShaderUniform( g_AsyncPrograms[ 0 ], "pixelBuffer", blue, sizeof( blue ) );
	if ( !QueueRenderPassScene( g_AsyncTest, 64, DrawAsyncTest ) )
	{
		printf( "SKIP: render passes not supported on this backend\n" );
		return 2;
	}
	return 0;
}

static int CheckAsyncTest()
{
	static unsigned char px[ 64 * 64 * 4 ];
	if ( !DemoReadTexture( g_AsyncTest.Target, 0, 0, 64, 64, px, sizeof( px ) ) )
	{
		printf( "SKIP: no readback of the pass target\n" );
		return 2;
	}
	const RenderPassProbe probes[] = { { 16, 32, { 0, 0, 255, 255 } }, { 48, 32, { 0, 255, 0, 255 } } };
	int bad = CheckRenderPassProbes( "async program", px, 64, probes, g_AsyncJobs[ 1 ] ? 2 : 1, 4 );
	if ( g_AsyncJobs[ 2 ] )
	{
		const char* log = ImPlatform_GetShaderJobLog( g_AsyncJobs[ 2 ] );
		if ( ImPlatform_GetShaderJobStatus( g_AsyncJobs[ 2 ], nullptr ) != ImPlatform_ShaderStatus_Failed || !strstr( log, "undeclared_value" ) || !g_AsyncSyncLog )
		{
			printf( "FAIL: the shader that does not compile: status, job log \"%s\" or ImPlatform_GetShaderLog\n", log );
			++bad;
		}
	}
	if ( !bad )
		printf( "PASS (%d frames while compiling%s)\n", g_AsyncFrames, g_AsyncJobs[ 1 ] ? "" : "; no shader parts here: defines and logs not tested" );
	return bad ? 1 : 0;
}

static void DestroyAsyncTest()
{
	for ( ImPlatform_ShaderJob job : g_AsyncJobs )
		ImPlatform_DestroyShaderJob( job );
}
#endif
#endif // IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER

#if defined( IMGUI_HAS_TEXTURES ) && !defined( __EMSCRIPTEN__ )
// --screenshot-test: a red and a blue square drawn on top of the main viewport, the viewport captured
// (ImPlatform_CaptureViewport) and read back (ImPlatform_ReadCapture): the capture has the size of the back buffer and
// both squares at their place, as RGBA. Exit code as above.
// --screenshot-viewport-test: the same with multi-viewports, for a window placed outside the main one (a secondary
// platform window). Exit 2 where the platform has no multi-viewports or the backend captures the main viewport only.
static ImTextureID g_Screenshot = 0;
static unsigned int g_ScreenshotW = 0, g_ScreenshotH = 0;
static ImVec2 g_ScreenshotScale;
static bool g_ScreenshotMain = true;
static bool QueueScreenshotTest( ImGuiViewport* viewport )
{
#if IMPLATFORM_GFX_SUPPORT_RENDER_TARGETS && IMPLATFORM_GFX_SUPPORT_READBACK
	g_ScreenshotMain = viewport == ImGui::GetMainViewport();
	ImDrawList* dl = ImGui::GetForegroundDrawList( viewport );
	const ImVec2 o = viewport->Pos;
	dl->AddRectFilled( ImVec2( o.x + 16, o.y + 16 ), ImVec2( o.x + 48, o.y + 48 ), IM_COL32( 255, 0, 0, 255 ) );
	dl->AddRectFilled( ImVec2( o.x + 64, o.y + 16 ), ImVec2( o.x + 96, o.y + 48 ), IM_COL32( 0, 0, 255, 255 ) );
	g_ScreenshotScale = viewport->FramebufferScale;
	g_Screenshot = ImPlatform_CaptureViewport( viewport, &g_ScreenshotW, &g_ScreenshotH );
	return g_Screenshot != 0;
#else
	(void)viewport;
	return false;
#endif
}

static int CheckScreenshotTest()
{
#if IMPLATFORM_GFX_SUPPORT_RENDER_TARGETS && IMPLATFORM_GFX_SUPPORT_READBACK
	unsigned int bw = 0, bh = 0;
	ImPlatform_GetBackbufferSize( &bw, &bh );
	ImVector<unsigned char> px;
	px.resize( (int)( g_ScreenshotW * g_ScreenshotH * 4 ) );
	int bad = 0;
	if ( !ImPlatform_ReadCapture( g_Screenshot, g_ScreenshotW, g_ScreenshotH, px.Data, (size_t)px.Size ) )
	{
		printf( "FAIL: ImPlatform_ReadCapture\n" );
		return 1;
	}
	if ( g_ScreenshotMain && ( g_ScreenshotW != bw || g_ScreenshotH != bh ) )
	{
		printf( "FAIL: capture %ux%u, back buffer %ux%u\n", g_ScreenshotW, g_ScreenshotH, bw, bh );
		++bad;
	}
	const struct { float x, y; int rgba[ 4 ]; } probes[] = { { 32, 32, { 255, 0, 0, 255 } }, { 80, 32, { 0, 0, 255, 255 } } };
	for ( const auto& p : probes )
	{
		const unsigned int x = (unsigned int)( p.x * g_ScreenshotScale.x ), y = (unsigned int)( p.y * g_ScreenshotScale.y );
		if ( x >= g_ScreenshotW || y >= g_ScreenshotH )
		{
			printf( "FAIL: probe (%u, %u) outside the %ux%u capture\n", x, y, g_ScreenshotW, g_ScreenshotH );
			++bad;
			continue;
		}
		const unsigned char* q = px.Data + ( (size_t)y * g_ScreenshotW + x ) * 4;
		for ( int c = 0; c < 4; ++c )
			if ( abs( (int)q[ c ] - p.rgba[ c ] ) > 3 )
			{
				printf( "FAIL at (%u, %u): got %d %d %d %d, expected %d %d %d %d\n", x, y, q[ 0 ], q[ 1 ], q[ 2 ], q[ 3 ], p.rgba[ 0 ], p.rgba[ 1 ], p.rgba[ 2 ], p.rgba[ 3 ] );
				++bad;
				break;
			}
	}
	if ( !bad )
		printf( "PASS (%ux%u)\n", g_ScreenshotW, g_ScreenshotH );
	return bad ? 1 : 0;
#else
	return 2;
#endif
}
#endif

#if IMPLATFORM_GFX_SUPPORT_COMPUTE
#if IM_CURRENT_GFX == IM_GFX_OPENGL3
#include "shaders/compute_test.glsl.h"
#elif IM_CURRENT_GFX == IM_GFX_VULKAN
#include "shaders/compute_test.spv.h"
#else
#include "shaders/compute_test.hlsl.h"
#endif
#endif
#if IMPLATFORM_GFX_SUPPORT_ASYNC_COMPUTE
#include <chrono>
#include <thread>
#endif
// --compute-test: storage texture + compute dispatch + readback, the same check on every backend.
// Reads an input texture (t0), writes a storage texture (u0) with a constant from b0, reads it back.
// --compute-async-test (async): the same with the shader from ImPlatform_CreateComputeShaderAsync and the dispatch from
// ImPlatform_DispatchComputeAsync, each polled until it is ready (no frame is rendered meanwhile: polling alone must
// make them complete).
// Exit code: 0 pass, 1 fail, 2 unsupported here.
static int ImPlatformDemo_ComputeTest( bool async )
{
	printf( "compute test: readback=%d compute=%d\n", (int)ImPlatform_SupportsReadback(), (int)ImPlatform_SupportsCompute() );
	bool supported = ImPlatform_SupportsReadback();
#if IMPLATFORM_GFX_SUPPORT_ASYNC_COMPUTE
	supported = supported && ( !async || ImPlatform_SupportsCompute() );
#else
	supported = supported && !async;
#endif
	if ( !supported )
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
	const float params[ 4 ] = { 0.0f, 0.0f, 0.5f, 0.0f };
	ImPlatform_Shader cs = nullptr;
	bool dispatched = false;
#if IMPLATFORM_GFX_SUPPORT_ASYNC_COMPUTE
	ImPlatform_ShaderJob job = nullptr;
	if ( async )
	{
		// Up to 10 seconds each, a millisecond between polls
		int compile_polls = 0, dispatch_polls = 0, state = 0;
		job = ImPlatform_CreateComputeShaderAsync( &sd );
		while ( ImPlatform_GetShaderJobStatus( job, nullptr ) == ImPlatform_ShaderStatus_Compiling && ++compile_polls < 10000 )
			std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
		cs = ImPlatform_GetShaderJobShader( job );
		ImPlatform_ComputeDispatch dispatch = cs && input && output
			? ImPlatform_DispatchComputeAsync( cs, &input, 1, &output, 1, params, sizeof( params ), N / 8, N / 8, 1 ) : nullptr;
		while ( ( state = ImPlatform_PollComputeDispatch( dispatch ) ) == 0 && ++dispatch_polls < 10000 )
			std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
		dispatched = state == 1;
		ImPlatform_ReleaseComputeDispatch( dispatch );
		printf( "async: %d polls while compiling (log \"%s\"), %d polls while dispatching, state %d\n", compile_polls,
				ImPlatform_GetShaderJobLog( job ), dispatch_polls, state );
	}
	else
#endif
	{
		cs = ImPlatform_CreateShader( &sd );
		dispatched = cs && input && output
			&& ImPlatform_DispatchCompute( cs, &input, 1, &output, 1, params, sizeof( params ), N / 8, N / 8, 1 );
	}
	const bool read = dispatched && DemoReadTexture( output, 0, 0, N, N, out_px, sizeof( out_px ) );
#if IMPLATFORM_GFX_SUPPORT_ASYNC_COMPUTE
	ImPlatform_DestroyShaderJob( job );		// With its compute shader
#endif
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

#if IMPLATFORM_GFX_SUPPORT_DEVICE_CONTROL
#include <chrono>
#endif
// --device-lost-test: removes the device on purpose after a few frames. ImPlatform_GfxCheck() then skips the frames
// without stalling the loop, and the teardown that follows the test ends on the lost device. Exit code 0 / 1, 2 where no
// device can be removed.
static int ImPlatformDemo_DeviceLostTest()
{
#if IMPLATFORM_GFX_SUPPORT_DEVICE_CONTROL
	if ( ImPlatform_GfxDebugRemoveDevice() )
	{
		const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
		bool skipped = ImPlatform_GfxDeviceLost() && !ImPlatform_GfxDeviceReady();
		for ( int i = 0; i < 10; i++ )
			skipped = !ImPlatform_GfxCheck() && skipped;
		const double seconds = std::chrono::duration<double>( std::chrono::steady_clock::now() - start ).count();
		const bool pass = skipped && seconds < 1.0;
		printf( "%s: device lost test (lost and frames skipped %d, ten checks in %.2f s)\n", pass ? "PASS" : "FAIL", (int)skipped, seconds );
		return pass ? 0 : 1;
	}
#endif
	printf( "SKIP: no device removal on this backend\n" );
	return 2;
}

// The second half of --device-lost-test, once everything created on the lost device is destroyed: the device is
// recreated, and a frame of the new one is captured and read back (the squares of --screenshot-test).
static int ImPlatformDemo_DeviceRecreateTest( ImVec4 clear_color )
{
#if IMPLATFORM_GFX_SUPPORT_DEVICE_CONTROL
	if ( !ImPlatform_GfxRecreateDevice() || !ImPlatform_GfxDeviceReady() || !ImPlatform_GfxCheck() )
	{
		printf( "FAIL: ImPlatform_GfxRecreateDevice\n" );
		return 1;
	}
	bool captured = true;
	for ( int frame = 0; frame < 3; frame++ )
	{
		ImPlatform_GfxAPINewFrame();
		ImPlatform_PlatformNewFrame();
		ImGui::NewFrame();
		if ( frame == 1 )
			captured = QueueScreenshotTest( ImGui::GetMainViewport() );
		ImGui::Render();
		ImPlatform_GfxAPIClear( clear_color );
		ImPlatform_GfxAPIRender( clear_color );
#ifdef IMGUI_HAS_VIEWPORT
		ImPlatform_GfxViewportPre();
		ImPlatform_GfxViewportPost();
#endif
		ImPlatform_GfxAPISwapBuffer();
	}
	if ( !captured )
	{
		printf( "FAIL: no capture on the recreated device\n" );
		return 1;
	}
	const int result = CheckScreenshotTest();
	ImPlatform_DestroyTexture( g_Screenshot );
	g_Screenshot = 0;
	return result;
#else
	(void)clear_color;
	return 2;
#endif
}

// --drag-rect-test: the arithmetic of the borderless GLFW window's software move and resize (ImPlatform_DragRect), without
// a window: a move, edges that grow, and an edge dragged past the minimum size (the opposite edge stays). Exit code 0 / 1.
static int ImPlatformDemo_DragRectTest()
{
	const int start[ 4 ] = { 100, 200, 400, 300 };	// x, y, width, height
	const struct { int edges, dx, dy, want[ 4 ]; } cases[] = {
		{ 16, 30, -20, { 130, 180, 400, 300 } },		// moved
		{ 2 | 8, 50, 40, { 100, 200, 450, 340 } },		// right and bottom grow
		{ 1 | 4, -50, -40, { 50, 160, 450, 340 } },		// left and top grow, the right and bottom edges stay
		{ 1, 1000, 0, { 420, 200, 80, 300 } },			// left past the minimum width (80): the right edge stays at 500
		{ 8, 0, -1000, { 100, 200, 400, 60 } },			// bottom past the minimum height (60)
	};
	int bad = 0;
	for ( const auto& c : cases )
	{
		int out[ 4 ];
		ImPlatform_DragRect( c.edges, start, c.dx, c.dy, 80, 60, out );
		if ( memcmp( out, c.want, sizeof( out ) ) != 0 )
		{
			printf( "FAIL edges %d by (%d, %d): got %d %d %d %d\n", c.edges, c.dx, c.dy, out[ 0 ], out[ 1 ], out[ 2 ], out[ 3 ] );
			++bad;
		}
	}
	if ( !bad )
		printf( "PASS\n" );
	return bad ? 1 : 0;
}

// --pen-test: (SDL3) pen events pushed on SDL's queue come out of ImPlatform_ConsumePenSamples as samples: the axes
// SDL reports one by one accumulate, the rotation is turned to [0, 360), contact follows the pen state, and leaving
// proximity ends ImPlatform_GetPenState. Exit code: 0 pass, 1 fail, 2 where pen events cannot be synthesized.
static int ImPlatformDemo_PenTest()
{
#if IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3
	const SDL_WindowID window = SDL_GetWindowID( ImPlatform_App_GetSDL3Window() );
	SDL_Event e;
	SDL_zero( e );
	e.type = SDL_EVENT_PEN_PROXIMITY_IN;
	e.pproximity.windowID = window;
	SDL_PushEvent( &e );
	const struct { SDL_PenAxis axis; float value; } axes[ 3 ] = { { SDL_PEN_AXIS_PRESSURE, 0.5f }, { SDL_PEN_AXIS_XTILT, 30.0f }, { SDL_PEN_AXIS_ROTATION, -90.0f } };
	for ( const auto& a : axes )
	{
		SDL_zero( e );
		e.type = SDL_EVENT_PEN_AXIS;
		e.paxis.windowID = window;
		e.paxis.x = 10.0f;
		e.paxis.y = 20.0f;
		e.paxis.axis = a.axis;
		e.paxis.value = a.value;
		SDL_PushEvent( &e );
	}
	SDL_zero( e );
	e.type = SDL_EVENT_PEN_DOWN;
	e.ptouch.windowID = window;
	e.ptouch.x = 10.0f;
	e.ptouch.y = 20.0f;
	e.ptouch.pen_state = SDL_PEN_INPUT_DOWN | SDL_PEN_INPUT_ERASER_TIP;
	e.ptouch.down = true;
	SDL_PushEvent( &e );
	SDL_zero( e );
	e.type = SDL_EVENT_PEN_MOTION;
	e.pmotion.windowID = window;
	e.pmotion.x = 11.0f;
	e.pmotion.y = 21.0f;
	e.pmotion.pen_state = SDL_PEN_INPUT_DOWN;
	SDL_PushEvent( &e );
	SDL_zero( e );
	e.type = SDL_EVENT_PEN_UP;
	e.ptouch.windowID = window;
	e.ptouch.x = 11.0f;
	e.ptouch.y = 21.0f;
	SDL_PushEvent( &e );
	ImPlatform_PlatformEvents();

	ImPlatform_PenSample s[ 16 ], state = {};
	const int n = ImPlatform_ConsumePenSamples( s, 16 );
	const bool in_range = ImPlatform_GetPenState( &state );
	bool ok = ImPlatform_SupportsPen() && n == 6 && in_range;
	ok = ok && s[ 0 ].pressure == 0.5f && !s[ 0 ].has_tilt && !s[ 0 ].has_rotation && !s[ 0 ].in_contact;
	ok = ok && s[ 2 ].pressure == 0.5f && s[ 2 ].has_tilt && s[ 2 ].tilt_x == 30.0f && s[ 2 ].has_rotation && s[ 2 ].rotation == 270.0f;
	ok = ok && s[ 3 ].in_contact && s[ 3 ].eraser && s[ 3 ].x == 10.0f && s[ 3 ].y == 20.0f;
	ok = ok && s[ 4 ].in_contact && !s[ 4 ].eraser && s[ 4 ].x == 11.0f && s[ 4 ].y == 21.0f && s[ 4 ].time > 0.0;
	ok = ok && !s[ 5 ].in_contact && !state.in_contact && state.pressure == 0.5f;
	SDL_zero( e );
	e.type = SDL_EVENT_PEN_PROXIMITY_OUT;
	e.pproximity.windowID = window;
	SDL_PushEvent( &e );
	ImPlatform_PlatformEvents();
	ok = ok && !ImPlatform_GetPenState( &state );
	printf( ok ? "PASS\n" : "FAIL (%d samples, in range %d)\n", n, (int)in_range );
	return ok ? 0 : 1;
#else
	printf( "SKIP: pen events are synthesized on SDL3 only (pen supported here: %d)\n", (int)ImPlatform_SupportsPen() );
	return 2;
#endif
}

// The "Gestures" window: the pan, zoom and rotation of ImPlatform_ConsumeTouchGesture (fingers on a touch screen, a
// pinch or a turn on a trackpad) move, scale and turn a square. The mouse wheel is added up next to them: it is the
// trackpad's two-finger pan, and what a pinch arrives as when the platform does not report it as one.
static void GestureDemo()
{
	static ImVec2 pan( 0.0f, 0.0f );
	static float zoom = 1.0f, rotation = 0.0f, wheel_x = 0.0f, wheel_y = 0.0f;
	static int pinches = 0;
	ImPlatform_TouchGesture gesture = {};
	if ( ImPlatform_ConsumeTouchGesture( &gesture ) )
	{
		pan.x += gesture.pan_x;
		pan.y += gesture.pan_y;
		zoom *= gesture.zoom;
		rotation += gesture.rotation;
		if ( gesture.zoom != 1.0f || gesture.rotation != 0.0f )
			++pinches;
	}
	wheel_x += ImGui::GetIO().MouseWheelH;
	wheel_y += ImGui::GetIO().MouseWheel;

	ImGui::SetNextWindowPos( ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver, ImVec2( 0.5f, 0.5f ) );
	ImGui::SetNextWindowSize( ImPlatform_EmToVec2( 26.0f, 24.0f ), ImGuiCond_FirstUseEver );
	if ( ImGui::Begin( "Gestures" ) )
	{
		ImGui::TextWrapped( "Pinch on the trackpad, or use two fingers on a touch screen%s.", ImPlatform_SupportsTouch() ? "" : " (touch screens are not read on this platform)" );
		ImGui::Text( "Zoom %.3f   Rotation %.1f deg   Pan (%.0f, %.0f)", zoom, rotation, pan.x, pan.y );
		ImGui::Text( "Fingers %d   Frames with a pinch or a turn: %d", gesture.finger_count, pinches );
		ImGui::Text( "Mouse wheel, added up: (%.2f, %.2f)", wheel_x, wheel_y );
		if ( ImGui::Button( "Reset" ) )
		{
			pan = ImVec2( 0.0f, 0.0f );
			zoom = 1.0f;
			rotation = wheel_x = wheel_y = 0.0f;
			pinches = 0;
		}
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		ImVec2 size = ImGui::GetContentRegionAvail();
		size.x = size.x < 16.0f ? 16.0f : size.x;
		size.y = size.y < 16.0f ? 16.0f : size.y;
		ImGui::InvisibleButton( "canvas", size );
		const ImVec2 center( origin.x + size.x * 0.5f + pan.x, origin.y + size.y * 0.5f + pan.y );
		const float half = ImPlatform_EmSize( 3.0f ) * zoom, angle = rotation * 3.14159265f / 180.0f;
		const float c = cosf( angle ) * half, s = sinf( angle ) * half;	// Clockwise: y points down
		ImDrawList* draw = ImGui::GetWindowDrawList();
		draw->PushClipRect( origin, ImVec2( origin.x + size.x, origin.y + size.y ), true );
		draw->AddQuadFilled( ImVec2( center.x - c + s, center.y - s - c ), ImVec2( center.x + c + s, center.y + s - c ),
							 ImVec2( center.x + c - s, center.y + s + c ), ImVec2( center.x - c - s, center.y - s + c ), IM_COL32( 80, 160, 255, 255 ) );
		draw->AddLine( center, ImVec2( center.x + s, center.y - c ), IM_COL32_WHITE, 2.0f );	// The square's up
		draw->PopClipRect();
	}
	ImGui::End();
}

// --touch-test: two fingers that touch, move apart, turn and lift come out of ImPlatform_GetTouchPoints and
// ImPlatform_ConsumeTouchGesture. On SDL2 and SDL3 they are finger events pushed on SDL's queue; on Win32 they are
// given to the recognizer directly (a pointer message cannot be synthesized without injecting input into the desktop).
// Exit code: 0 pass, 1 fail, 2 without touch input (GLFW).
// phase 0: the finger touches, 1: it moves, 2: it lifts.
static void TouchTestFinger( int id, float x, float y, int phase )
{
#if IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3
	SDL_Window* window = ImPlatform_App_GetSDL3Window();
	int w = 1, h = 1;
	SDL_GetWindowSize( window, &w, &h );
	SDL_Event e;
	SDL_zero( e );
	e.type = phase == 0 ? SDL_EVENT_FINGER_DOWN : phase == 1 ? SDL_EVENT_FINGER_MOTION : SDL_EVENT_FINGER_UP;
	e.tfinger.windowID = SDL_GetWindowID( window );
	e.tfinger.fingerID = (SDL_FingerID)id;
	e.tfinger.x = x / (float)w;
	e.tfinger.y = y / (float)h;
	e.tfinger.pressure = 0.5f;
	SDL_PushEvent( &e );
	ImPlatform_PlatformEvents();
#elif IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2
	SDL_Window* window = ImPlatform_App_GetSDL2Window();
	int w = 1, h = 1;
	SDL_GetWindowSize( window, &w, &h );
	SDL_Event e;
	SDL_zero( e );
	e.type = phase == 0 ? SDL_FINGERDOWN : phase == 1 ? SDL_FINGERMOTION : SDL_FINGERUP;
	e.tfinger.windowID = SDL_GetWindowID( window );
	e.tfinger.fingerId = (SDL_FingerID)id;
	e.tfinger.x = x / (float)w;
	e.tfinger.y = y / (float)h;
	e.tfinger.pressure = 0.5f;
	SDL_PushEvent( &e );
	ImPlatform_PlatformEvents();
#else
	ImPlatform_Touch_Set( (unsigned long long)id, x, y, 0.5f, phase != 2 );
#endif
}

// The gesture since the previous call against the expected one (positions within 0.05 pixel: SDL carries them as
// fractions of the window).
static bool TouchTestGesture( const char* step, bool want_result, int fingers, float x, float y, float pan_x, float pan_y, float zoom, float rotation )
{
	ImPlatform_TouchGesture g = {};
	const bool result = ImPlatform_ConsumeTouchGesture( &g );
	const float got[ 6 ] = { g.x, g.y, g.pan_x, g.pan_y, g.zoom * 100.0f, g.rotation }, want[ 6 ] = { x, y, pan_x, pan_y, zoom * 100.0f, rotation };
	bool ok = result == want_result && g.finger_count == fingers;
	for ( int i = 0; i < 6; ++i )
		ok = ok && fabsf( got[ i ] - want[ i ] ) < 0.05f;
	if ( !ok )
		printf( "FAIL %s: result %d, %d fingers at (%g, %g), pan (%g, %g), zoom %g, rotation %g\n", step, (int)result, g.finger_count, g.x, g.y, g.pan_x, g.pan_y, g.zoom, g.rotation );
	return ok;
}

static int ImPlatformDemo_TouchTest()
{
	if ( !ImPlatform_SupportsTouch() )
	{
		printf( "SKIP: no touch input on this platform\n" );
		return 2;
	}
	ImPlatform_TouchGesture before;
	ImPlatform_PlatformEvents();
	ImPlatform_ConsumeTouchGesture( &before );	// What the window received while it opened is not part of the test
	ImPlatform_TouchPoint points[ 4 ];
	TouchTestFinger( 1, 100.0f, 100.0f, 0 );
	TouchTestFinger( 2, 200.0f, 100.0f, 0 );
	bool ok = ImPlatform_GetTouchPoints( points, 4 ) == 2 && points[ 0 ].id == 1 && points[ 1 ].id == 2 && points[ 1 ].pressure == 0.5f
			  && fabsf( points[ 1 ].x - 200.0f ) < 0.05f && fabsf( points[ 1 ].y - 100.0f ) < 0.05f;
	if ( !ok )
		printf( "FAIL: two fingers down are not the two touch points\n" );
	ok = TouchTestGesture( "touch", true, 2, 150.0f, 100.0f, 0.0f, 0.0f, 1.0f, 0.0f ) && ok;			// Touching is not a pan
	TouchTestFinger( 2, 300.0f, 100.0f, 1 );
	ok = TouchTestGesture( "spread", true, 2, 200.0f, 100.0f, 50.0f, 0.0f, 2.0f, 0.0f ) && ok;			// Twice as far apart
	TouchTestFinger( 2, 100.0f, 300.0f, 1 );
	ok = TouchTestGesture( "turn", true, 2, 100.0f, 200.0f, -100.0f, 100.0f, 1.0f, 90.0f ) && ok;		// A quarter turn clockwise
	TouchTestFinger( 2, 200.0f, 300.0f, 1 );
	TouchTestFinger( 2, 100.0f, 300.0f, 1 );
	ok = TouchTestGesture( "there and back", true, 2, 100.0f, 200.0f, 0.0f, 0.0f, 1.0f, 0.0f ) && ok;	// Moves between two calls add up
	TouchTestFinger( 1, 100.0f, 100.0f, 2 );
	ok = TouchTestGesture( "lift", true, 1, 100.0f, 300.0f, 0.0f, 0.0f, 1.0f, 0.0f ) && ok;			// Lifting is not a pan
	ok = ImPlatform_GetTouchPoints( points, 4 ) == 1 && points[ 0 ].id == 2 && ok;
	TouchTestFinger( 2, 110.0f, 300.0f, 1 );
	TouchTestFinger( 2, 110.0f, 300.0f, 2 );
	ok = TouchTestGesture( "one finger", true, 0, 0.0f, 0.0f, 10.0f, 0.0f, 1.0f, 0.0f ) && ok;			// Moved before it lifted
	ok = TouchTestGesture( "none", false, 0, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f ) && ok;
	ok = ImPlatform_GetTouchPoints( points, 4 ) == 0 && ok;
	// A trackpad's pinch is the gesture's zoom, without fingers
#if IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3
#if SDL_VERSION_ATLEAST( 3, 4, 0 )
	SDL_Event pinch;
	SDL_zero( pinch );
	pinch.type = SDL_EVENT_PINCH_UPDATE;
	pinch.pinch.scale = 1.5f;
	SDL_PushEvent( &pinch );
	ImPlatform_PlatformEvents();
	ok = TouchTestGesture( "pinch", true, 0, 0.0f, 0.0f, 0.0f, 0.0f, 1.5f, 0.0f ) && ok;
#endif
#elif defined( _WIN32 )
	// What a precision touchpad sends to the window, whichever library made it: a wheel notch marked Ctrl while the key
	// is up. The key is marked up for this thread first: a window that opens while Ctrl is held elsewhere starts with
	// it down.
	ImPlatform_PlatformEvents();
	BYTE keys[ 256 ];
	if ( GetKeyboardState( keys ) )
	{
		keys[ VK_CONTROL ] = 0;
		SetKeyboardState( keys );
	}
	SendMessageW( (HWND)ImPlatform_Native_MainWindow(), WM_MOUSEWHEEL, MAKEWPARAM( MK_CONTROL, WHEEL_DELTA ), 0 );
	ok = TouchTestGesture( "pinch", true, 0, 0.0f, 0.0f, 0.0f, 0.0f, 1.189207f, 0.0f ) && ok;
#endif
	if ( ok )
		printf( "PASS\n" );
	return ok ? 0 : 1;
}

// --drop-test: a drag from another application comes out of the ImPlatform_SetDropCallback callback as events, and
// each file out of the ImPlatform_SetDropFileCallback one. SDL3, and SDL2 off Windows: drop events pushed on SDL's
// queue. Windows (Win32, GLFW, SDL2): a data object holding a file and a URL handed to the window's drop target,
// entering, moving and dropping, then entering and leaving. Exit code: 0 pass, 1 fail, 2 on GLFW off Windows (nothing
// can hand it a drop there: see --drop-wait-test).
#if IMPLATFORM_APP_SUPPORT_DROP_FILE
// The events of a drag as text in a buffer of 512 characters: B(egin) P(osition) F:path T:text E(nd).
static ImVec2 g_DropTestPos;
static void DropTestCallback( const ImPlatform_DropEvent* event, void* user_data )
{
	static const char* const names[ 5 ] = { "B", "P", "F", "T", "E" };
	char* log = (char*)user_data;
	const size_t used = strlen( log );
	if ( event->type == ImPlatform_DropEvent_Position )
		g_DropTestPos = event->pos;
	if ( used + 8 >= 512 )
		return;
	if ( event->text )
		snprintf( log + used, 512 - used, "%s:%s ", names[ event->type ], event->text );
	else
		snprintf( log + used, 512 - used, "%s ", names[ event->type ] );
}
#endif
#if IMPLATFORM_APP_SUPPORT_DROP_FILE && ( defined( _WIN32 ) || IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2 || IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3 )
#define DEMO_DROP_TEST 1
static void DropTestFileCallback( const char*, ImVec2, void* user_data )
{
	++*(int*)user_data;
}
#if defined( _WIN32 ) && IM_CURRENT_PLATFORM != IM_PLATFORM_SDL3
#include <shlobj.h>
static HGLOBAL DropTestMemory( const void* bytes, size_t size )
{
	HGLOBAL memory = GlobalAlloc( GMEM_MOVEABLE, size );
	memcpy( GlobalLock( memory ), bytes, size );
	GlobalUnlock( memory );
	return memory;
}
#endif
#else
#define DEMO_DROP_TEST 0
#endif

static int ImPlatformDemo_DropTest()
{
#if DEMO_DROP_TEST
	static char log[ 512 ];
	int files = 0;
	bool at = true;		// The position reported while the drag moved, where the platform reports one
	ImPlatform_SetDropCallback( DropTestCallback, log );
	ImPlatform_SetDropFileCallback( DropTestFileCallback, &files );
#if IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3
	const char* want = "B P F:a.png T:https://example.com/i.png E ";
	const struct { Uint32 type; const char* data; } events[ 5 ] = { { SDL_EVENT_DROP_BEGIN, nullptr }, { SDL_EVENT_DROP_POSITION, nullptr },
		{ SDL_EVENT_DROP_FILE, "a.png" }, { SDL_EVENT_DROP_TEXT, "https://example.com/i.png" }, { SDL_EVENT_DROP_COMPLETE, nullptr } };
	for ( const auto& d : events )
	{
		SDL_Event e;
		SDL_zero( e );
		e.type = d.type;
		e.drop.windowID = SDL_GetWindowID( ImPlatform_App_GetSDL3Window() );
		e.drop.x = 30.0f;
		e.drop.y = 40.0f;
		e.drop.data = d.data;
		SDL_PushEvent( &e );
	}
	ImPlatform_PlatformEvents();
	at = g_DropTestPos.x == 30.0f && g_DropTestPos.y == 40.0f;
#elif !defined( _WIN32 )
	// sdl2-compat (SDL2's functions over SDL3: what Homebrew installs as sdl2) loses the string of a pushed drop event
	// and crashes reading it back. It reports a patch version of 50 or more, which SDL2 itself never reaches.
	SDL_version version;
	SDL_GetVersion( &version );
	if ( version.patch >= 50 )
	{
		printf( "SKIP: sdl2-compat does not carry pushed drop events\n" );
		return 2;
	}
	const char* want = "B F:a.png T:https://example.com/i.png E ";
	const struct { Uint32 type; const char* data; } events[ 4 ] = { { SDL_DROPBEGIN, nullptr }, { SDL_DROPFILE, "a.png" },
		{ SDL_DROPTEXT, "https://example.com/i.png" }, { SDL_DROPCOMPLETE, nullptr } };
	for ( const auto& d : events )
	{
		SDL_Event e;
		SDL_zero( e );
		e.type = d.type;
		e.drop.windowID = SDL_GetWindowID( ImPlatform_App_GetSDL2Window() );
		e.drop.file = d.data ? SDL_strdup( d.data ) : nullptr;	// The event loop frees it
		SDL_PushEvent( &e );
	}
	ImPlatform_PlatformEvents();
#else
	const char* want = "B P P F:C:\\a.png T:https://example.com/i.png E B P E ";
	IDropTarget* target = (IDropTarget*)ImPlatform_Native_GetDropTarget();
	const HWND window = (HWND)ImPlatform_Native_MainWindow();
	IDataObject* data = nullptr;
	if ( FAILED( SHCreateDataObject( nullptr, 0, nullptr, nullptr, IID_IDataObject, (void**)&data ) ) )
	{
		printf( "FAIL: no data object\n" );
		return 1;
	}
	const wchar_t url[] = L"https://example.com/i.png";
	struct { DROPFILES header; wchar_t paths[ 10 ]; } drop = {};	// The path and the list's two terminators
	drop.header.pFiles = sizeof( DROPFILES );
	drop.header.fWide = TRUE;
	memcpy( drop.paths, L"C:\\a.png", sizeof( L"C:\\a.png" ) );
	FORMATETC format = { CF_UNICODETEXT, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	STGMEDIUM medium = {};
	medium.tymed = TYMED_HGLOBAL;
	medium.hGlobal = DropTestMemory( url, sizeof( url ) );
	data->SetData( &format, &medium, TRUE );	// TRUE: the object owns the memory
	format.cfFormat = CF_HDROP;
	medium.hGlobal = DropTestMemory( &drop, sizeof( drop ) );
	data->SetData( &format, &medium, TRUE );
	POINT client = { 30, 40 };
	ClientToScreen( window, &client );
	const POINTL pt = { client.x, client.y };
	DWORD effect = DROPEFFECT_COPY;
	target->DragEnter( data, 0, pt, &effect );
	target->DragOver( 0, pt, &effect );
	const bool copy = effect == DROPEFFECT_COPY;
	target->Drop( data, 0, pt, &effect );
	target->DragEnter( data, 0, pt, &effect );
	target->DragLeave();
	data->Release();
	at = copy && g_DropTestPos.x == 30.0f && g_DropTestPos.y == 40.0f;
#endif
	ImPlatform_SetDropCallback( nullptr, nullptr );
	ImPlatform_SetDropFileCallback( nullptr, nullptr );
	const bool ok = strcmp( log, want ) == 0 && files == 1 && at;
	printf( ok ? "PASS\n" : "FAIL: events \"%s\", %d files, position %d\n", log, files, (int)at );
	return ok ? 0 : 1;
#else
	printf( "SKIP: a drop cannot be synthesized on this platform\n" );
	return 2;
#endif
}

// --drop-wait-test: waits ten seconds for another application to drag something over the window and drop it, then
// prints the events and the last position. For a drag made by hand, or by a program that speaks the system's drag
// protocol. Exit code: 0 when a drop with an item came, 1 when none did, 2 without drop events.
#include <chrono>
#include <thread>
static int ImPlatformDemo_DropWaitTest()
{
#if IMPLATFORM_APP_SUPPORT_DROP_FILE
	static char log[ 512 ];
	ImPlatform_SetDropCallback( DropTestCallback, log );
	bool dropped = false;
	for ( int i = 0; i < 1000 && !dropped; ++i )
	{
		ImPlatform_PlatformEvents();
		std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
		const size_t used = strlen( log );
		dropped = used > 2 && strcmp( log + used - 2, "E " ) == 0 && ( strstr( log, "F:" ) || strstr( log, "T:" ) );
	}
	ImPlatform_SetDropCallback( nullptr, nullptr );
	printf( "events \"%s\", last position (%g, %g)\n%s\n", log, g_DropTestPos.x, g_DropTestPos.y, dropped ? "PASS" : "FAIL: no drop came" );
	return dropped ? 0 : 1;
#else
	printf( "SKIP: no drop events on this platform\n" );
	return 2;
#endif
}

// --drag-out-test: ImPlatform_DragOut drags a file and a text out of the application. With no mouse button held the
// drag ends at once on the window under the pointer: the demo's own, whose drop callback gets them back. On Windows
// the test puts the pointer over the window itself, and back, and keeps the window above the others meanwhile.
// Elsewhere it first waits three seconds: for the pointer to be put over the window (under Xvfb it is there, in the
// middle of the screen), or for a button to be pressed, in which case the drag follows the pointer to the window of
// another application until the release. Exit code: 0 pass, 1 fail, 2 without drag out.
static int ImPlatformDemo_DragOutTest()
{
#if IMPLATFORM_APP_SUPPORT_DROP_FILE
	if ( !ImPlatform_SupportsDragOut() )
	{
		printf( "SKIP: nothing can be dragged out on this platform\n" );
		return 2;
	}
	static char log[ 512 ];
	ImPlatform_SetDropCallback( DropTestCallback, log );
#ifdef _WIN32
	const char* path = "C:\\out.png";
	const HWND window = (HWND)ImPlatform_Native_MainWindow();
	POINT cursor = {};
	RECT rect = {};
	GetCursorPos( &cursor );
	ImPlatform_SetAlwaysOnTop( true );
	ImPlatform_PlatformEvents();
	GetWindowRect( window, &rect );
	SetCursorPos( ( rect.left + rect.right ) / 2, ( rect.top + rect.bottom ) / 2 );
#else
	const char* path = "/tmp/out put.png";
	for ( int i = 0; i < 300; ++i )
	{
		ImPlatform_PlatformEvents();
		std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
	}
#endif
	const bool taken = ImPlatform_DragOut( &path, 1, "dragged out" );
#ifdef _WIN32
	SetCursorPos( cursor.x, cursor.y );
	ImPlatform_SetAlwaysOnTop( false );
#endif
	for ( int i = 0; i < 20; ++i )	// What the window library still holds of the drop
	{
		ImPlatform_PlatformEvents();
		std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
	}
	ImPlatform_SetDropCallback( nullptr, nullptr );
	char file[ 128 ];
	snprintf( file, sizeof( file ), "F:%s ", path );
	const size_t used = strlen( log );
	// ImPlatform's OLE target reads the files and the text; an X11 target takes the files when there are files, and
	// SDL3's own target the text when there is text
	const bool has_file = strstr( log, file ) != nullptr, has_text = strstr( log, "T:dragged out " ) != nullptr;
	bool ok = taken && log[ 0 ] == 'B' && ( has_file || has_text ) && used > 2 && strcmp( log + used - 2, "E " ) == 0;
#if defined( _WIN32 ) && IM_CURRENT_PLATFORM != IM_PLATFORM_SDL3
	ok = ok && has_file && has_text;
#endif
	printf( "taken %d, events \"%s\"\n%s\n", (int)taken, log, ok ? "PASS" : "FAIL: the drag did not come back to the window" );
	return ok ? 0 : 1;
#else
	printf( "SKIP: nothing can be dragged out on this platform\n" );
	return 2;
#endif
}

// --window-test: the window goes fullscreen (larger than it was, ImPlatform_IsFullscreen) and comes back to its size;
// always on top and the icon are set, and the system theme is printed. Exit code: 0 pass, 1 fail, 2 without the calls.
#if IMPLATFORM_APP_SUPPORT_WINDOW_STATE
#include <chrono>
#include <thread>
static void WindowTestSize( int* w, int* h )
{
	// Some window systems apply a change later: let their events through first
	for ( int i = 0; i < 30; ++i )
	{
		ImPlatform_PlatformEvents();
		std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
	}
#if IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32
	RECT rect = {};
	GetClientRect( ImPlatform_App_GetHWND(), &rect );
	*w = rect.right;
	*h = rect.bottom;
#elif IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW
	glfwGetWindowSize( ImPlatform_App_GetGLFWWindow(), w, h );
#elif IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2
	SDL_GetWindowSize( ImPlatform_App_GetSDL2Window(), w, h );
#else
	SDL_GetWindowSize( ImPlatform_App_GetSDL3Window(), w, h );
#endif
}
#endif

static int ImPlatformDemo_WindowTest()
{
	static const char* const themes[ 3 ] = { "unknown", "light", "dark" };
	printf( "system theme: %s\n", themes[ ImPlatform_GetSystemTheme() ] );
#if IMPLATFORM_APP_SUPPORT_WINDOW_STATE
	unsigned char icon[ 16 * 16 * 4 ];
	for ( int i = 0; i < 16 * 16; ++i )
	{
		icon[ i * 4 + 0 ] = 255;
		icon[ i * 4 + 1 ] = (unsigned char)( i % 16 * 16 );
		icon[ i * 4 + 2 ] = 0;
		icon[ i * 4 + 3 ] = 255;
	}
	int w[ 3 ] = {}, h[ 3 ] = {};
	WindowTestSize( &w[ 0 ], &h[ 0 ] );
	const bool icon_set = ImPlatform_SetWindowIcon( icon, 16, 16 );
	ImPlatform_SetAlwaysOnTop( true );
#if IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32
	const bool on_top = ( GetWindowLong( ImPlatform_App_GetHWND(), GWL_EXSTYLE ) & WS_EX_TOPMOST ) != 0;
#else
	const bool on_top = true;
#endif
	ImPlatform_SetAlwaysOnTop( false );
	ImPlatform_SetFullscreen( true );
	WindowTestSize( &w[ 1 ], &h[ 1 ] );
	const bool fullscreen = ImPlatform_IsFullscreen();
	ImPlatform_SetFullscreen( false );
	WindowTestSize( &w[ 2 ], &h[ 2 ] );
	const bool ok = icon_set && on_top && fullscreen && !ImPlatform_IsFullscreen() && w[ 1 ] >= w[ 0 ] && h[ 1 ] >= h[ 0 ]
					&& ( w[ 1 ] > w[ 0 ] || h[ 1 ] > h[ 0 ] ) && w[ 2 ] == w[ 0 ] && h[ 2 ] == h[ 0 ];
	printf( ok ? "PASS\n" : "FAIL: icon %d, on top %d, fullscreen %d, %dx%d then %dx%d then %dx%d\n", (int)icon_set, (int)on_top, (int)fullscreen,
			w[ 0 ], h[ 0 ], w[ 1 ], h[ 1 ], w[ 2 ], h[ 2 ] );
	return ok ? 0 : 1;
#else
	printf( "SKIP: no window state calls on this platform\n" );
	return 2;
#endif
}

#ifndef __EMSCRIPTEN__
// --window-life-test: what a user does to the window, done with the window system's own calls while the demo renders.
// The window is resized and resized back, and the display size and the back buffer follow; with multi-viewports a
// Dear ImGui window is moved across the edge of the main window and back (a platform window made, drawn and destroyed
// at run time); then the window is closed as by its close button, and the loop ends. On the Cocoa platform
// "--window-life-test quit" picks the Quit menu item instead. Exit code: 0 pass, 1 fail.
static struct
{
	int		iteration;			// Of the loop: the window system applies a change within a few of them
	int		first_w, first_h;	// Display size at the start
	int		want_w, want_h;		// Display size asked last
	int		viewports;			// Platform windows before the window is moved out
	bool	moved;				// The "Moved" window is drawn
	bool	out;				// ... across the edge of the main window
	bool	own_viewport;		// ... in a platform window of its own, as of the last frame
	bool	closing;			// The close was requested: the loop is expected to end
} g_WindowLife;

static void WindowLifeResize( int w, int h )
{
	g_WindowLife.want_w = w;
	g_WindowLife.want_h = h;
#if IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32
	const HWND window = ImPlatform_App_GetHWND();
	RECT outer = {}, client = {};
	GetWindowRect( window, &outer );
	GetClientRect( window, &client );
	SetWindowPos( window, NULL, 0, 0, w + ( outer.right - outer.left ) - client.right, h + ( outer.bottom - outer.top ) - client.bottom,
				  SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE );
#elif IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW
	glfwSetWindowSize( ImPlatform_App_GetGLFWWindow(), w, h );
#elif IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2
	SDL_SetWindowSize( ImPlatform_App_GetSDL2Window(), w, h );
#elif IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3
	SDL_SetWindowSize( ImPlatform_App_GetSDL3Window(), w, h );
#elif IM_CURRENT_PLATFORM == IM_PLATFORM_APPLE
	[(__bridge NSWindow*)ImPlatform_App_GetNSWindow() setContentSize:NSMakeSize( w, h )];
#endif
}

static void WindowLifeClose( bool quit_menu )
{
	g_WindowLife.closing = true;
	(void)quit_menu;
#if IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32
	PostMessageW( ImPlatform_App_GetHWND(), WM_CLOSE, 0, 0 );
#elif IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW
	glfwSetWindowShouldClose( ImPlatform_App_GetGLFWWindow(), GLFW_TRUE );	// What GLFW does when the window system asks
#elif IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2
	SDL_Event e = {};
	e.type = SDL_WINDOWEVENT;
	e.window.event = SDL_WINDOWEVENT_CLOSE;
	e.window.windowID = SDL_GetWindowID( ImPlatform_App_GetSDL2Window() );
	SDL_PushEvent( &e );
#elif IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3
	SDL_Event e = {};
	e.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
	e.window.windowID = SDL_GetWindowID( ImPlatform_App_GetSDL3Window() );
	SDL_PushEvent( &e );
#elif IM_CURRENT_PLATFORM == IM_PLATFORM_APPLE
	if ( quit_menu )
		[[[NSApp mainMenu] itemAtIndex:0].submenu performActionForItemAtIndex:0];
	else
		[(__bridge NSWindow*)ImPlatform_App_GetNSWindow() performClose:nil];
#endif
}

// One iteration of the loop, before its frame. Returns -1 while the test goes on, else its exit code.
static int WindowLifeStep( bool quit_menu )
{
	const ImGuiIO& io = ImGui::GetIO();
	const int step = ++g_WindowLife.iteration;
	if ( step == 20 )
	{
		g_WindowLife.first_w = (int)io.DisplaySize.x;
		g_WindowLife.first_h = (int)io.DisplaySize.y;
		WindowLifeResize( g_WindowLife.first_w - 200, g_WindowLife.first_h - 150 );
	}
	if ( step == 80 || step == 140 )
	{
		unsigned int bw = 0, bh = 0;
		ImPlatform_GetBackbufferSize( &bw, &bh );
		const int fw = (int)( io.DisplaySize.x * io.DisplayFramebufferScale.x + 0.5f ), fh = (int)( io.DisplaySize.y * io.DisplayFramebufferScale.y + 0.5f );
		if ( abs( (int)io.DisplaySize.x - g_WindowLife.want_w ) > 1 || abs( (int)io.DisplaySize.y - g_WindowLife.want_h ) > 1
			 || abs( (int)bw - fw ) > 1 || abs( (int)bh - fh ) > 1 )
		{
			printf( "FAIL: resized to %dx%d: display size %.0fx%.0f, back buffer %ux%u for %dx%d\n", g_WindowLife.want_w, g_WindowLife.want_h,
					io.DisplaySize.x, io.DisplaySize.y, bw, bh, fw, fh );
			return 1;
		}
		if ( step == 80 )
			WindowLifeResize( g_WindowLife.first_w, g_WindowLife.first_h );
#ifdef IMGUI_HAS_VIEWPORT
		else if ( ( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable ) && ( io.BackendFlags & ImGuiBackendFlags_PlatformHasViewports )
				  && ( io.BackendFlags & ImGuiBackendFlags_RendererHasViewports ) )
		{
			g_WindowLife.viewports = ImGui::GetPlatformIO().Viewports.Size;
			g_WindowLife.moved = g_WindowLife.out = true;
		}
#endif
	}
#ifdef IMGUI_HAS_VIEWPORT
	if ( g_WindowLife.moved && ( step == 200 || step == 260 ) )
	{
		const int viewports = ImGui::GetPlatformIO().Viewports.Size;
		const bool out = step == 200;
		if ( g_WindowLife.own_viewport != out || viewports != g_WindowLife.viewports + ( out ? 1 : 0 ) )
		{
			printf( "FAIL: window moved %s: own platform window %d, %d platform windows for %d before\n", out ? "across the edge" : "back inside",
					(int)g_WindowLife.own_viewport, viewports, g_WindowLife.viewports );
			return 1;
		}
		g_WindowLife.out = false;
	}
#endif
	if ( step == 260 )
		WindowLifeClose( quit_menu );
	if ( step > 460 )
	{
		printf( "FAIL: the loop goes on after the close request\n" );
		return 1;
	}
	return -1;
}

// The window the test moves, inside a frame: across the right edge of the main window, or inside it
static void WindowLifeDraw()
{
	if ( !g_WindowLife.moved )
		return;
	const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos( main_viewport->Pos + ImVec2( g_WindowLife.out ? main_viewport->Size.x - 60.0f : 80.0f, 80.0f ) );
	ImGui::SetNextWindowSize( ImVec2( 240.0f, 160.0f ) );
	ImGui::Begin( "Moved", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings );
	ImGui::TextUnformatted( g_WindowLife.out ? "Across the edge" : "Back inside" );
#ifdef IMGUI_HAS_VIEWPORT
	g_WindowLife.own_viewport = ImGui::GetWindowViewport() != main_viewport;
#endif
	ImGui::End();
}
#endif // __EMSCRIPTEN__

// --clipboard-test: an image put on the clipboard with ImPlatform_SetClipboardImage comes back with its size and its
// pixels; the system theme is printed. It replaces what the clipboard held, and needs no window but on SDL3. Exit
// code: 0 pass, 1 fail, 2 where the clipboard takes no image.
static int ImPlatformDemo_ClipboardTest()
{
	static const char* const themes[ 3 ] = { "unknown", "light", "dark" };
	printf( "system theme: %s\n", themes[ ImPlatform_GetSystemTheme() ] );
	if ( !ImPlatform_SupportsClipboardImage() )
	{
		printf( "SKIP: no clipboard image on this system\n" );
		return 2;
	}
	const unsigned char image[ 3 * 2 * 4 ] = { 255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 10, 20, 30, 255, 40, 50, 60, 255, 70, 80, 90, 255 };
	unsigned char back[ sizeof( image ) ] = {};
	int w = 0, h = 0;
	const bool ok = ImPlatform_SetClipboardImage( image, 3, 2 ) && ImPlatform_GetClipboardImageSize( &w, &h ) && w == 3 && h == 2
					&& ImPlatform_GetClipboardImage( back, w, h ) && memcmp( image, back, sizeof( image ) ) == 0;
	printf( ok ? "PASS\n" : "FAIL: %dx%d, first pixel %d %d %d %d\n", w, h, back[ 0 ], back[ 1 ], back[ 2 ], back[ 3 ] );
	return ok ? 0 : 1;
}

int main( int argc, char** argv )
{
#ifndef __EMSCRIPTEN__	// The self-tests and --dump-msl are not built for the web
	if ( argc > 3 && strcmp( argv[ 1 ], "--dump-msl" ) == 0 )
		return DumpMSL( argv[ 2 ], argv[ 3 ] );
#endif
	if ( argc > 1 && strcmp( argv[ 1 ], "--drag-rect-test" ) == 0 )
		return ImPlatformDemo_DragRectTest();
#if IM_CURRENT_PLATFORM != IM_PLATFORM_SDL3	// SDL3 carries the clipboard itself, once its window exists
	if ( argc > 1 && strcmp( argv[ 1 ], "--clipboard-test" ) == 0 )
		return ImPlatformDemo_ClipboardTest();
#endif
	const bool computeAsyncTest = argc > 1 && strcmp( argv[ 1 ], "--compute-async-test" ) == 0;
	const bool computeTest = computeAsyncTest || ( argc > 1 && strcmp( argv[ 1 ], "--compute-test" ) == 0 );
	const bool renderPassTest = argc > 1 && strcmp( argv[ 1 ], "--render-pass-test" ) == 0;
	const bool shaderTest = argc > 1 && strcmp( argv[ 1 ], "--shader-test" ) == 0;
	const bool samplerTest = argc > 1 && strcmp( argv[ 1 ], "--sampler-test" ) == 0;
	const bool viewportsDemo = argc > 1 && strcmp( argv[ 1 ], "--viewports" ) == 0;	// Every window in a platform window of its own
	const bool windowLifeTest = argc > 1 && strcmp( argv[ 1 ], "--window-life-test" ) == 0;
#ifndef __EMSCRIPTEN__
	const bool windowLifeQuit = windowLifeTest && argc > 2 && strcmp( argv[ 2 ], "quit" ) == 0;	// Cocoa: by the Quit menu item
#endif
	const bool deviceLostTest = argc > 1 && strcmp( argv[ 1 ], "--device-lost-test" ) == 0;
	int deviceLostFrame = 0;
	const bool directDrawTest = argc > 1 && strcmp( argv[ 1 ], "--direct-draw-test" ) == 0;
	const bool partsTest = argc > 1 && strcmp( argv[ 1 ], "--shader-parts-test" ) == 0;
	const bool texture3DTest = argc > 1 && strcmp( argv[ 1 ], "--texture3d-test" ) == 0;
	const bool vertexInputsTest = argc > 1 && strcmp( argv[ 1 ], "--vertex-inputs-test" ) == 0;
	const bool bufferUpdateTest = argc > 1 && strcmp( argv[ 1 ], "--buffer-update-test" ) == 0;
	const bool meshStreamTest = argc > 1 && strcmp( argv[ 1 ], "--mesh-stream-test" ) == 0;
	const bool blendTest = argc > 1 && strcmp( argv[ 1 ], "--blend-test" ) == 0;
	const bool asyncTest = argc > 1 && strcmp( argv[ 1 ], "--shader-async-test" ) == 0;
	const bool screenshotViewportTest = argc > 1 && strcmp( argv[ 1 ], "--screenshot-viewport-test" ) == 0;
	const bool screenshotTest = screenshotViewportTest || ( argc > 1 && strcmp( argv[ 1 ], "--screenshot-test" ) == 0 );
	const bool passTest = renderPassTest || shaderTest || samplerTest || directDrawTest || partsTest || texture3DTest || vertexInputsTest || blendTest
						|| asyncTest || screenshotTest || bufferUpdateTest || meshStreamTest;	// Tests read back what a few frames rendered
	int exitCode = 0;
#ifndef __EMSCRIPTEN__
	int renderPassFrame = 0;
	bool asyncCompiling = false;	// --shader-async-test: its frames are counted once its programs are ready
#endif
#if IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR
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
#endif

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
	if ( screenshotViewportTest || viewportsDemo || windowLifeTest )
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	io.ConfigViewportsNoAutoMerge = viewportsDemo;
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
	style.FrameRounding = style.GrabRounding = 3.0f;	// A little rounding, as Dear ImGui's examples
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
		return ImPlatformDemo_ComputeTest( computeAsyncTest );
	if ( argc > 1 && strcmp( argv[ 1 ], "--pen-test" ) == 0 )
		return ImPlatformDemo_PenTest();
	if ( argc > 1 && strcmp( argv[ 1 ], "--touch-test" ) == 0 )
		return ImPlatformDemo_TouchTest();
	if ( argc > 1 && strcmp( argv[ 1 ], "--drop-test" ) == 0 )
		return ImPlatformDemo_DropTest();
	if ( argc > 1 && strcmp( argv[ 1 ], "--drop-wait-test" ) == 0 )
		return ImPlatformDemo_DropWaitTest();
	if ( argc > 1 && strcmp( argv[ 1 ], "--drag-out-test" ) == 0 )
		return ImPlatformDemo_DragOutTest();
	if ( argc > 1 && strcmp( argv[ 1 ], "--clipboard-test" ) == 0 )
		return ImPlatformDemo_ClipboardTest();
	if ( argc > 1 && strcmp( argv[ 1 ], "--window-test" ) == 0 )
		return ImPlatformDemo_WindowTest();

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

#ifndef __EMSCRIPTEN__
		if ( windowLifeTest )
		{
			const int result = WindowLifeStep( windowLifeQuit );
			if ( result >= 0 )
			{
				exitCode = result;
				break;
			}
		}
#endif
		if ( deviceLostTest && ++deviceLostFrame > 3 )
		{
			exitCode = ImPlatformDemo_DeviceLostTest();
			break;
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
		if ( samplerTest && renderPassFrame == 0 )
		{
#if IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER
			g_ShaderTestProgram = gradient_program;
			if ( !CreateSamplerTest() )
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
		if ( meshStreamTest && renderPassFrame < 3 )
		{
			const int result = RunMeshStreamFrame();
			if ( result )
			{
				printf( result == 2 ? "SKIP: no ImPlatform_DrawMesh on this backend\n" : "FAIL: a buffer or draw call failed\n" );
				exitCode = result;
				break;
			}
		}
		if ( bufferUpdateTest && renderPassFrame < 3 )
		{
#if IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER
			g_ShaderTestProgram = gradient_program;
			if ( !gradient_program || !QueueBufferUpdateTest() )
#endif
			{
				printf( "SKIP: no custom shader, vertex buffer or render pass on this backend\n" );
				exitCode = 2;
				break;
			}
		}
		if ( blendTest && renderPassFrame == 0 )
		{
#if IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER
			g_ShaderTestProgram = gradient_program;
			if ( !gradient_program || !QueueRenderPassScene( g_BlendTest, 64, DrawBlendTest ) )
#endif
			{
				printf( "SKIP: no custom shader or render pass on this backend\n" );
				exitCode = 2;
				break;
			}
		}
		if ( asyncTest && renderPassFrame == 0 )
		{
#if IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER
			const int state = UpdateAsyncTest();
#else
			const int state = 2;
#endif
			if ( state > 0 )
			{
				exitCode = state;
				break;
			}
			asyncCompiling = state < 0;
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
		float grid_top = 0.0f;	// The window grid starts below the titlebar, which it would cover
#if IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR
		if ( ImPlatform_CustomTitleBarEnabled() )
		{
			float titleBarHeight = ImPlatform_EmSize(2.0f);
			grid_top = titleBarHeight;
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
		float half_height = ( display_size.y - grid_top ) * 0.5f;
		const ImVec2 origin = ImGui::GetMainViewport()->Pos;	// Where the main window is on the screen with multi-viewports; zero without

		// Bottom-right: Dear ImGui Demo
		ImGui::SetNextWindowPos(origin + ImVec2(half_width, grid_top + half_height), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(half_width, half_height), ImGuiCond_Always);
		bool show = true;
		ImGui::ShowDemoWindow( &show );

		// Top-right: Texture API Demo Window
#ifdef IMGUI_HAS_TEXTURES
		if (img && img_white)
		{
			ImGui::SetNextWindowPos(origin + ImVec2(half_width, grid_top), ImGuiCond_Always);
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
			ImGui::SetNextWindowPos(origin + ImVec2(0, grid_top), ImGuiCond_Always);
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
		if (gradient_program && !shaderTest && !samplerTest && !directDrawTest && !bufferUpdateTest)		// The tests set their own constants
		{
			ImGui::SetNextWindowPos(origin + ImVec2(0, grid_top + half_height), ImGuiCond_Always);
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
			ImGui::SetNextWindowPos(origin + ImVec2(0, grid_top), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(half_width, display_size.y - grid_top), ImGuiCond_Always);
			ImGui::Begin("Custom Shader Support", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
			ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Custom Shaders Not Supported");
			ImGui::Separator();
			ImGui::Text("The current graphics backend does not support custom shaders.");	// Every built-in backend does
			ImGui::End();
		}
#endif // IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER

		if ( !passTest )		// The tests read the frame back
			GestureDemo();
#ifndef __EMSCRIPTEN__
		if ( windowLifeTest )
			WindowLifeDraw();
#endif
#ifdef IMGUI_HAS_VIEWPORT
		if ( viewportsDemo )
		{
			ImGui::SetNextWindowPos( origin + ImPlatform_EmToVec2( 3.0f, 12.0f ), ImGuiCond_FirstUseEver );
			ImGui::Begin( "Viewports", nullptr, ImGuiWindowFlags_AlwaysAutoResize );
			ImGui::Text( "Platform windows: %d", ImGui::GetPlatformIO().Viewports.Size );
			ImGui::Text( "This window has its own: %s", ImGui::GetWindowViewport() != ImGui::GetMainViewport() ? "yes" : "no" );
			ImGui::End();
		}
#endif

#if defined( IMGUI_HAS_TEXTURES ) && !defined( __EMSCRIPTEN__ )
		ImGuiViewport* screenshotViewport = ImGui::GetMainViewport();
#ifdef IMGUI_HAS_VIEWPORT
		if ( screenshotViewportTest )
		{
			// Outside the main window: Dear ImGui gives it a platform window of its own.
			ImGui::SetNextWindowPos( ImVec2( screenshotViewport->Pos.x + screenshotViewport->Size.x + 32.0f, screenshotViewport->Pos.y ) );
			ImGui::SetNextWindowSize( ImVec2( 240.0f, 160.0f ) );
			ImGui::Begin( "Secondary viewport", nullptr, ImGuiWindowFlags_NoDecoration );
			screenshotViewport = ImGui::GetWindowViewport();
			ImGui::End();
		}
#endif
		if ( screenshotViewportTest && renderPassFrame == 1 && screenshotViewport == ImGui::GetMainViewport() )
		{
			printf( "SKIP: no secondary viewport on this platform\n" );
			exitCode = 2;
			break;
		}
		if ( screenshotTest && renderPassFrame == 1 && !QueueScreenshotTest( screenshotViewport ) )
		{
			printf( "SKIP: no viewport capture on this backend\n" );
			exitCode = 2;
			break;
		}
#endif
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
		if ( passTest && !asyncCompiling && ++renderPassFrame == 3 )
		{
			if ( screenshotTest )
				exitCode = CheckScreenshotTest();
			else
#if IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER
			exitCode = shaderTest ? CheckShaderTest() : samplerTest ? CheckSamplerTest() : directDrawTest ? CheckDirectDrawTest() : partsTest ? CheckPartsTest()
					 : texture3DTest ? CheckTexture3DTest() : vertexInputsTest ? CheckVertexInputsTest() : blendTest ? CheckBlendTest()
					 : asyncTest ? CheckAsyncTest() : bufferUpdateTest ? CheckBufferUpdateTest() : meshStreamTest ? CheckMeshStreamTest()
					 : CheckRenderPassTest();
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

#ifndef __EMSCRIPTEN__
	if ( windowLifeTest && exitCode == 0 )	// The loop ended by itself: by the close the test asked for, or too early
	{
		exitCode = g_WindowLife.closing ? 0 : 1;
		printf( exitCode == 0 ? "PASS\n" : "FAIL: the loop ended before the close was requested\n" );
	}
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
	if ( g_Screenshot )
		ImPlatform_DestroyTexture( g_Screenshot );
	DestroyRenderPassScene( g_RenderPassTest );
	DestroyRenderPassScene( g_RenderPassComposite );
#if IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER
	DestroyRenderPassScene( g_ShaderTest );
	DestroyRenderPassScene( g_SamplerTest );
	DestroySamplerTest();
	DestroyRenderPassScene( g_PartsTest );
	DestroyPartsTest();
	DestroyRenderPassScene( g_Texture3DTest );
	DestroyTexture3DTest();
	DestroyRenderPassScene( g_VertexInputsTest );
	DestroyVertexInputsTest();
	DestroyRenderPassScene( g_BufferUpdateTest );
	DestroyBufferUpdateTest();
	DestroyMeshStreamTest();
	DestroyRenderPassScene( g_BlendTest );
	DestroyRenderPassScene( g_AsyncTest );
	DestroyAsyncTest();
	DestroyRenderPassScene( g_DirectDrawPass );
	if ( g_DirectDrawTarget )
		ImPlatform_DestroyTexture( g_DirectDrawTarget );
	if ( g_DirectDrawCopy )
		ImPlatform_DestroyTexture( g_DirectDrawCopy );
#endif
#endif
#endif
	if ( deviceLostTest && exitCode == 0 )
		exitCode = ImPlatformDemo_DeviceRecreateTest( clear_color );
	ImPlatform_ShutdownGfxAPI();
	ImPlatform_ShutdownWindow();
	ImPlatform_ShutdownPostGfxAPI();

	ImGui::DestroyContext();

	ImPlatform_DestroyWindow();

	return exitCode;
}
