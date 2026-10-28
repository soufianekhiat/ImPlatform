
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

// Define platform and graphics API from project configuration
#define IM_CURRENT_PLATFORM IM_CONFIG_PLATFORM
#define IM_CURRENT_GFX IM_CONFIG_GFX

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

// --render-pass-test: an opaque red square, a half transparent blue square, transparent elsewhere, read back.
// Checks the clear, the orientation (top-left origin) and premultiplied colors. Exit code: 0 pass, 1 fail, 2 unsupported.
static RenderPassScene g_RenderPassTest;
static void DrawRenderPassTest( ImDrawList* dl )
{
	dl->AddRectFilled( ImVec2( 0, 0 ), ImVec2( 32, 32 ), IM_COL32( 255, 0, 0, 255 ) );
	dl->AddRectFilled( ImVec2( 32, 0 ), ImVec2( 64, 32 ), IM_COL32( 0, 0, 255, 128 ) );
}

// Next frame, a second pass draws gray then the first target with premultiplied alpha (the composite).
static RenderPassScene g_RenderPassComposite;
static void DrawRenderPassComposite( ImDrawList* dl )
{
	dl->AddRectFilled( ImVec2( 0, 0 ), ImVec2( 64, 64 ), IM_COL32( 100, 100, 100, 255 ) );
	ImPlatform_PushPremultipliedAlpha( dl );
	dl->AddImage( g_RenderPassTest.Target, ImVec2( 0, 0 ), ImVec2( 64, 64 ) );
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
	unsigned char px[ 64 * 64 * 4 ];
	if ( !g_RenderPassTest.Target || !ImPlatform_ReadTexture( g_RenderPassTest.Target, 0, 0, 64, 64, px, sizeof( px ) ) )
	{
		printf( "SKIP: no readback of the pass target\n" );
		return 2;
	}
	// Pass target: the clear, the orientation and premultiplied colors.
	const RenderPassProbe target[] = { { 8, 8, { 255, 0, 0, 255 } }, { 48, 8, { 0, 0, 128, 128 } }, { 8, 48, { 0, 0, 0, 0 } }, { 48, 48, { 0, 0, 0, 0 } } };
	int bad = CheckRenderPassProbes( "target", px, 64, target, 4, 4 );
	// Composite over gray (100) with premultiplied alpha: blue at half coverage is 128 + 100 * 127 / 255.
	if ( ImPlatform_ReadTexture( g_RenderPassComposite.Target, 0, 0, 64, 64, px, sizeof( px ) ) )
	{
		const RenderPassProbe composite[] = { { 8, 8, { 255, 0, 0, 255 } }, { 48, 8, { 50, 50, 178, 255 } }, { 8, 48, { 100, 100, 100, 255 } } };
		bad += CheckRenderPassProbes( "composite", px, 64, composite, 3, 4 );
	}
	else
		bad++;
	if ( !bad )
		printf( "PASS\n" );
	return bad ? 1 : 0;
}
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
	sd.entry_point = "main";
#if IM_CURRENT_GFX == IM_GFX_OPENGL3
	sd.format = ImPlatform_ShaderFormat_GLSL;
	sd.source_code =
		"#version 430\n"
		"layout(local_size_x = 8, local_size_y = 8) in;\n"
		"layout(binding = 0) uniform sampler2D inTex;\n"
		"layout(rgba8, binding = 0) uniform writeonly image2D outTex;\n"
		"layout(std140, binding = 0) uniform Params { vec4 p; };\n"
		"void main() {\n"
		"    ivec2 q = ivec2(gl_GlobalInvocationID.xy);\n"
		"    vec4 c = texelFetch(inTex, q, 0);\n"
		"    imageStore(outTex, q, vec4(c.r, c.g, p.z, 1.0));\n"
		"}\n";
#elif IM_CURRENT_GFX == IM_GFX_VULKAN
	// glslangValidator -V + spirv-opt --strip-debug of (bindings: 0 constants, 1.. inputs, 9.. outputs):
	//   #version 450
	//   layout(local_size_x = 8, local_size_y = 8) in;
	//   layout(set = 0, binding = 0, std140) uniform Params { vec4 p; };
	//   layout(set = 0, binding = 1) uniform sampler2D inTex;
	//   layout(set = 0, binding = 9, rgba8) uniform writeonly image2D outTex;
	//   void main() {
	//       ivec2 q = ivec2(gl_GlobalInvocationID.xy);
	//       vec4 c = texelFetch(inTex, q, 0);
	//       imageStore(outTex, q, vec4(c.r, c.g, p.z, 1.0));
	//   }
	static const unsigned int spirv[] = {
		0x07230203, 0x00010000, 0x0008000b, 0x00000036, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
		0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
		0x0006000f, 0x00000005, 0x00000004, 0x6e69616d, 0x00000000, 0x0000000d, 0x00060010, 0x00000004,
		0x00000011, 0x00000008, 0x00000008, 0x00000001, 0x00040047, 0x0000000d, 0x0000000b, 0x0000001c,
		0x00040047, 0x00000019, 0x00000021, 0x00000001, 0x00040047, 0x00000019, 0x00000022, 0x00000000,
		0x00030047, 0x00000021, 0x00000019, 0x00040047, 0x00000021, 0x00000021, 0x00000009, 0x00040047,
		0x00000021, 0x00000022, 0x00000000, 0x00030047, 0x0000002b, 0x00000002, 0x00050048, 0x0000002b,
		0x00000000, 0x00000023, 0x00000000, 0x00040047, 0x0000002d, 0x00000021, 0x00000000, 0x00040047,
		0x0000002d, 0x00000022, 0x00000000, 0x00040047, 0x00000035, 0x0000000b, 0x00000019, 0x00020013,
		0x00000002, 0x00030021, 0x00000003, 0x00000002, 0x00040015, 0x00000006, 0x00000020, 0x00000001,
		0x00040017, 0x00000007, 0x00000006, 0x00000002, 0x00040020, 0x00000008, 0x00000007, 0x00000007,
		0x00040015, 0x0000000a, 0x00000020, 0x00000000, 0x00040017, 0x0000000b, 0x0000000a, 0x00000003,
		0x00040020, 0x0000000c, 0x00000001, 0x0000000b, 0x0004003b, 0x0000000c, 0x0000000d, 0x00000001,
		0x00040017, 0x0000000e, 0x0000000a, 0x00000002, 0x00030016, 0x00000012, 0x00000020, 0x00040017,
		0x00000013, 0x00000012, 0x00000004, 0x00040020, 0x00000014, 0x00000007, 0x00000013, 0x00090019,
		0x00000016, 0x00000012, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000001, 0x00000000,
		0x0003001b, 0x00000017, 0x00000016, 0x00040020, 0x00000018, 0x00000000, 0x00000017, 0x0004003b,
		0x00000018, 0x00000019, 0x00000000, 0x0004002b, 0x00000006, 0x0000001c, 0x00000000, 0x00090019,
		0x0000001f, 0x00000012, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000002, 0x00000004,
		0x00040020, 0x00000020, 0x00000000, 0x0000001f, 0x0004003b, 0x00000020, 0x00000021, 0x00000000,
		0x0004002b, 0x0000000a, 0x00000024, 0x00000000, 0x00040020, 0x00000025, 0x00000007, 0x00000012,
		0x0004002b, 0x0000000a, 0x00000028, 0x00000001, 0x0003001e, 0x0000002b, 0x00000013, 0x00040020,
		0x0000002c, 0x00000002, 0x0000002b, 0x0004003b, 0x0000002c, 0x0000002d, 0x00000002, 0x0004002b,
		0x0000000a, 0x0000002e, 0x00000002, 0x00040020, 0x0000002f, 0x00000002, 0x00000012, 0x0004002b,
		0x00000012, 0x00000032, 0x3f800000, 0x0004002b, 0x0000000a, 0x00000034, 0x00000008, 0x0006002c,
		0x0000000b, 0x00000035, 0x00000034, 0x00000034, 0x00000028, 0x00050036, 0x00000002, 0x00000004,
		0x00000000, 0x00000003, 0x000200f8, 0x00000005, 0x0004003b, 0x00000008, 0x00000009, 0x00000007,
		0x0004003b, 0x00000014, 0x00000015, 0x00000007, 0x0004003d, 0x0000000b, 0x0000000f, 0x0000000d,
		0x0007004f, 0x0000000e, 0x00000010, 0x0000000f, 0x0000000f, 0x00000000, 0x00000001, 0x0004007c,
		0x00000007, 0x00000011, 0x00000010, 0x0003003e, 0x00000009, 0x00000011, 0x0004003d, 0x00000017,
		0x0000001a, 0x00000019, 0x0004003d, 0x00000007, 0x0000001b, 0x00000009, 0x00040064, 0x00000016,
		0x0000001d, 0x0000001a, 0x0007005f, 0x00000013, 0x0000001e, 0x0000001d, 0x0000001b, 0x00000002,
		0x0000001c, 0x0003003e, 0x00000015, 0x0000001e, 0x0004003d, 0x0000001f, 0x00000022, 0x00000021,
		0x0004003d, 0x00000007, 0x00000023, 0x00000009, 0x00050041, 0x00000025, 0x00000026, 0x00000015,
		0x00000024, 0x0004003d, 0x00000012, 0x00000027, 0x00000026, 0x00050041, 0x00000025, 0x00000029,
		0x00000015, 0x00000028, 0x0004003d, 0x00000012, 0x0000002a, 0x00000029, 0x00060041, 0x0000002f,
		0x00000030, 0x0000002d, 0x0000001c, 0x0000002e, 0x0004003d, 0x00000012, 0x00000031, 0x00000030,
		0x00070050, 0x00000013, 0x00000033, 0x00000027, 0x0000002a, 0x00000031, 0x00000032, 0x00040063,
		0x00000022, 0x00000023, 0x00000033, 0x000100fd, 0x00010038
	};
	sd.format = ImPlatform_ShaderFormat_SPIRV;
	sd.bytecode = spirv;
	sd.bytecode_size = sizeof( spirv );
#else
	sd.format = ImPlatform_ShaderFormat_HLSL;
	sd.source_code =
		"Texture2D<float4> inTex : register(t0);\n"
		"RWTexture2D<float4> outTex : register(u0);\n"
		"cbuffer Params : register(b0) { float4 p; };\n"
		"[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {\n"
		"    float4 c = inTex.Load(int3(id.xy, 0));\n"
		"    outTex[id.xy] = float4(c.r, c.g, p.z, 1.0);\n"
		"}\n";
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
	int renderPassFrame = 0, exitCode = 0;
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
	// ========================================================================
	// Custom Shader Demo: Arrow SDF Shape
	// ========================================================================

	// Define shader source code based on graphics API
#if (IM_CURRENT_GFX != IM_GFX_VULKAN)
	const char* arrow_vs_source = nullptr;
	const char* arrow_ps_source = nullptr;
	ImPlatform_ShaderFormat arrow_format = ImPlatform_ShaderFormat_GLSL;
#endif

#if (IM_CURRENT_GFX == IM_GFX_OPENGL3)
	// GLSL for OpenGL3
	arrow_format = ImPlatform_ShaderFormat_GLSL;
	arrow_vs_source =
		"#version 130\n"
		"uniform mat4 ProjMtx;\n"
		"in vec2 Position;\n"
		"in vec2 UV;\n"
		"out vec2 Frag_UV;\n"
		"void main() {\n"
		"    Frag_UV = UV;\n"
		"    gl_Position = ProjMtx * vec4(Position.xy, 0, 1);\n"
		"}\n";

	arrow_ps_source =
		"#version 130\n"
		"in vec2 Frag_UV;\n"
		"out vec4 Out_Color;\n"
		"float sdArrow(vec2 p) {\n"
		"    // Arrow head (triangle pointing right)\n"
		"    vec2 head = p - vec2(0.5, 0.0);\n"
		"    float dHead = max(abs(head.y) + head.x * 0.5 - 0.25, -head.x - 0.3);\n"
		"    \n"
		"    // Arrow shaft (rectangle)\n"
		"    vec2 shaft = p - vec2(-0.1, 0.0);\n"
		"    float dShaft = max(abs(shaft.y) - 0.1, max(shaft.x - 0.4, -shaft.x - 0.6));\n"
		"    \n"
		"    return min(dHead, dShaft);\n"
		"}\n"
		"void main() {\n"
		"    vec2 uv = Frag_UV * 2.0 - 1.0;\n"
		"    float d = sdArrow(uv);\n"
		"    float alpha = 1.0 - smoothstep(0.0, 0.02, d);\n"
		"    Out_Color = vec4(1.0, 0.5, 0.2, 1.0) * alpha;\n"
		"}\n";

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10) || (IM_CURRENT_GFX == IM_GFX_DIRECTX11) || (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
	// HLSL for DirectX 10/11/12
	arrow_format = ImPlatform_ShaderFormat_HLSL;
	arrow_vs_source =
		"cbuffer vertexBuffer : register(b0) {\n"
		"    float4x4 ProjMtx;\n"
		"};\n"
		"struct VS_INPUT {\n"
		"    float2 pos : POSITION;\n"
		"    float4 col : COLOR0;\n"
		"    float2 uv  : TEXCOORD0;\n"
		"};\n"
		"struct PS_INPUT {\n"
		"    float4 pos : SV_POSITION;\n"
		"    float4 col : COLOR0;\n"
		"    float2 uv  : TEXCOORD0;\n"
		"};\n"
		"PS_INPUT main(VS_INPUT input) {\n"
		"    PS_INPUT output;\n"
		"    output.pos = mul(ProjMtx, float4(input.pos.xy, 0.0, 1.0));\n"
		"    output.col = input.col;\n"
		"    output.uv = input.uv;\n"
		"    return output;\n"
		"}\n";

	arrow_ps_source =
		"struct PS_INPUT {\n"
		"    float4 pos : SV_POSITION;\n"
		"    float4 col : COLOR0;\n"
		"    float2 uv  : TEXCOORD0;\n"
		"};\n"
		"float sdArrow(float2 p) {\n"
		"    // Arrow head (triangle pointing right)\n"
		"    float2 head = p - float2(0.5, 0.0);\n"
		"    float dHead = max(abs(head.y) + head.x * 0.5 - 0.25, -head.x - 0.3);\n"
		"    \n"
		"    // Arrow shaft (rectangle)\n"
		"    float2 shaft = p - float2(-0.1, 0.0);\n"
		"    float dShaft = max(abs(shaft.y) - 0.1, max(shaft.x - 0.4, -shaft.x - 0.6));\n"
		"    \n"
		"    return min(dHead, dShaft);\n"
		"}\n"
		"float4 main(PS_INPUT input) : SV_TARGET {\n"
		"    // Arrow SDF shader - renders arrow shape\n"
		"    float2 uv = input.uv * 2.0 - 1.0;\n"
		"    float d = sdArrow(uv);\n"
		"    float alpha = 1.0 - smoothstep(0.0, 0.02, d);\n"
		"    return float4(1.0, 0.5, 0.2, 1.0) * alpha;\n"
		"}\n";

#elif (IM_CURRENT_GFX == IM_GFX_VULKAN)
	// Vulkan uses pre-compiled SPIR-V bytecode
	// Include generated SPIR-V headers
	#include "shaders/arrow_sdf.vert.h"
	#include "shaders/arrow_sdf.frag.h"
	// Note: For Vulkan, we'll use bytecode fields instead of source_code

#elif (IM_CURRENT_GFX == IM_GFX_WGPU)
	// WGSL for WebGPU
	arrow_format = ImPlatform_ShaderFormat_WGSL;
	arrow_vs_source =
		"struct Uniforms {\n"
		"    projMtx: mat4x4<f32>,\n"
		"};\n"
		"@group(0) @binding(0) var<uniform> uniforms: Uniforms;\n"
		"\n"
		"struct VertexInput {\n"
		"    @location(0) pos: vec2<f32>,\n"
		"    @location(1) uv: vec2<f32>,\n"
		"    @location(2) col: vec4<f32>,\n"
		"};\n"
		"\n"
		"struct VertexOutput {\n"
		"    @builtin(position) position: vec4<f32>,\n"
		"    @location(0) uv: vec2<f32>,\n"
		"};\n"
		"\n"
		"@vertex\n"
		"fn main(input: VertexInput) -> VertexOutput {\n"
		"    var output: VertexOutput;\n"
		"    output.position = uniforms.projMtx * vec4<f32>(input.pos, 0.0, 1.0);\n"
		"    output.uv = input.uv;\n"
		"    return output;\n"
		"}\n";

	arrow_ps_source =
		"struct FragInput {\n"
		"    @location(0) uv: vec2<f32>,\n"
		"};\n"
		"\n"
		"fn sdArrow(p: vec2<f32>) -> f32 {\n"
		"    let head = p - vec2<f32>(0.5, 0.0);\n"
		"    let dHead = max(abs(head.y) + head.x * 0.5 - 0.25, -head.x - 0.3);\n"
		"    let shaft = p - vec2<f32>(-0.1, 0.0);\n"
		"    let dShaft = max(abs(shaft.y) - 0.1, max(shaft.x - 0.4, -shaft.x - 0.6));\n"
		"    return min(dHead, dShaft);\n"
		"}\n"
		"\n"
		"@fragment\n"
		"fn main(input: FragInput) -> @location(0) vec4<f32> {\n"
		"    let uv = input.uv * 2.0 - 1.0;\n"
		"    let d = sdArrow(uv);\n"
		"    let alpha = 1.0 - smoothstep(0.0, 0.02, d);\n"
		"    return vec4<f32>(1.0, 0.5, 0.2, 1.0) * alpha;\n"
		"}\n";
#endif

	// ========================================================================
	// Custom Shader Demo: Linear Gradient with Custom Vertex Buffer
	// ========================================================================

#if (IM_CURRENT_GFX != IM_GFX_VULKAN)
	const char* gradient_vs_source = nullptr;
	const char* gradient_ps_source = nullptr;
	ImPlatform_ShaderFormat gradient_format = ImPlatform_ShaderFormat_GLSL;
#endif

	// Gradient colors
	ImVec4 gradient_color_start = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
	ImVec4 gradient_color_end = ImVec4(0.2f, 0.2f, 1.0f, 1.0f);

#if (IM_CURRENT_GFX == IM_GFX_OPENGL3)
	gradient_format = ImPlatform_ShaderFormat_GLSL;
	gradient_vs_source =
		"#version 130\n"
		"uniform mat4 ProjMtx;\n"
		"in vec2 Position;\n"
		"in vec2 UV;\n"
		"out vec2 Frag_UV;\n"
		"void main() {\n"
		"    Frag_UV = UV;\n"
		"    gl_Position = ProjMtx * vec4(Position.xy, 0, 1);\n"
		"}\n";

	gradient_ps_source =
		"#version 130\n"
		"uniform vec4 ColorStart;\n"
		"uniform vec4 ColorEnd;\n"
		"in vec2 Frag_UV;\n"
		"out vec4 Out_Color;\n"
		"void main() {\n"
		"    Out_Color = mix(ColorStart, ColorEnd, Frag_UV.y);\n"
		"}\n";

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10) || (IM_CURRENT_GFX == IM_GFX_DIRECTX11) || (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
	gradient_format = ImPlatform_ShaderFormat_HLSL;
	gradient_vs_source =
		"cbuffer vertexBuffer : register(b0) {\n"
		"    float4x4 ProjMtx;\n"
		"};\n"
		"struct VS_INPUT {\n"
		"    float2 pos : POSITION;\n"
		"    float4 col : COLOR0;\n"
		"    float2 uv  : TEXCOORD0;\n"
		"};\n"
		"struct PS_INPUT {\n"
		"    float4 pos : SV_POSITION;\n"
		"    float4 col : COLOR0;\n"
		"    float2 uv  : TEXCOORD0;\n"
		"};\n"
		"PS_INPUT main(VS_INPUT input) {\n"
		"    PS_INPUT output;\n"
		"    output.pos = mul(ProjMtx, float4(input.pos.xy, 0.0, 1.0));\n"
		"    output.col = input.col;\n"
		"    output.uv = input.uv;\n"
		"    return output;\n"
		"}\n";

	gradient_ps_source =
		"cbuffer pixelBuffer : register(b1) {\n"  // b1 for DX12 compatibility (b0 is projection matrix)
		"    float4 ColorStart;\n"
		"    float4 ColorEnd;\n"
		"};\n"
		"struct PS_INPUT {\n"
		"    float4 pos : SV_POSITION;\n"
		"    float4 col : COLOR0;\n"
		"    float2 uv  : TEXCOORD0;\n"
		"};\n"
		"float4 main(PS_INPUT input) : SV_Target {\n"
		"    return lerp(ColorStart, ColorEnd, input.uv.y);\n"
		"}\n";

#elif (IM_CURRENT_GFX == IM_GFX_VULKAN)
	// Vulkan uses pre-compiled SPIR-V bytecode
	// Include generated SPIR-V headers
	#include "shaders/gradient.vert.h"
	#include "shaders/gradient.frag.h"
	// Note: For Vulkan, we'll use bytecode fields instead of source_code

#elif (IM_CURRENT_GFX == IM_GFX_WGPU)
	// WGSL for WebGPU
	gradient_format = ImPlatform_ShaderFormat_WGSL;
	gradient_vs_source =
		"struct Uniforms {\n"
		"    projMtx: mat4x4<f32>,\n"
		"    colorStart: vec4<f32>,\n"
		"    colorEnd: vec4<f32>,\n"
		"};\n"
		"@group(0) @binding(0) var<uniform> uniforms: Uniforms;\n"
		"\n"
		"struct VertexInput {\n"
		"    @location(0) pos: vec2<f32>,\n"
		"    @location(1) uv: vec2<f32>,\n"
		"    @location(2) col: vec4<f32>,\n"
		"};\n"
		"\n"
		"struct VertexOutput {\n"
		"    @builtin(position) position: vec4<f32>,\n"
		"    @location(0) uv: vec2<f32>,\n"
		"};\n"
		"\n"
		"@vertex\n"
		"fn main(input: VertexInput) -> VertexOutput {\n"
		"    var output: VertexOutput;\n"
		"    output.position = uniforms.projMtx * vec4<f32>(input.pos, 0.0, 1.0);\n"
		"    output.uv = input.uv;\n"
		"    return output;\n"
		"}\n";

	gradient_ps_source =
		"struct Uniforms {\n"
		"    projMtx: mat4x4<f32>,\n"
		"    colorStart: vec4<f32>,\n"
		"    colorEnd: vec4<f32>,\n"
		"};\n"
		"@group(0) @binding(0) var<uniform> uniforms: Uniforms;\n"
		"\n"
		"struct FragInput {\n"
		"    @location(0) uv: vec2<f32>,\n"
		"};\n"
		"\n"
		"@fragment\n"
		"fn main(input: FragInput) -> @location(0) vec4<f32> {\n"
		"    return mix(uniforms.colorStart, uniforms.colorEnd, input.uv.y);\n"
		"}\n";
#endif

	// Create shaders and programs
	ImPlatform_Shader arrow_vs = nullptr;
	ImPlatform_Shader arrow_ps = nullptr;
	ImPlatform_ShaderProgram arrow_program = nullptr;

	ImPlatform_Shader gradient_vs = nullptr;
	ImPlatform_Shader gradient_ps = nullptr;
	ImPlatform_ShaderProgram gradient_program = nullptr;

	// Create arrow shaders based on graphics API
#if (IM_CURRENT_GFX == IM_GFX_VULKAN)
	// Vulkan: Use pre-compiled SPIR-V bytecode
	{
		ImPlatform_ShaderDesc vs_desc = {};
		vs_desc.stage = ImPlatform_ShaderStage_Vertex;
		vs_desc.format = ImPlatform_ShaderFormat_SPIRV;
		vs_desc.bytecode = arrow_sdf_vert_spv;
		vs_desc.bytecode_size = arrow_sdf_vert_spv_size;
		vs_desc.entry_point = "main";
		arrow_vs = ImPlatform_CreateShader(&vs_desc);

		ImPlatform_ShaderDesc ps_desc = {};
		ps_desc.stage = ImPlatform_ShaderStage_Fragment;
		ps_desc.format = ImPlatform_ShaderFormat_SPIRV;
		ps_desc.bytecode = arrow_sdf_frag_spv;
		ps_desc.bytecode_size = arrow_sdf_frag_spv_size;
		ps_desc.entry_point = "main";
		arrow_ps = ImPlatform_CreateShader(&ps_desc);

		if (arrow_vs && arrow_ps) {
			arrow_program = ImPlatform_CreateShaderProgram(arrow_vs, arrow_ps);
		}
	}
#else
	// Other APIs: Use source code
	if (arrow_vs_source && arrow_ps_source) {
		ImPlatform_ShaderDesc vs_desc = {};
		vs_desc.stage = ImPlatform_ShaderStage_Vertex;
		vs_desc.format = arrow_format;
		vs_desc.source_code = arrow_vs_source;
		vs_desc.entry_point = "main";
		arrow_vs = ImPlatform_CreateShader(&vs_desc);

		ImPlatform_ShaderDesc ps_desc = {};
		ps_desc.stage = ImPlatform_ShaderStage_Fragment;
		ps_desc.format = arrow_format;
		ps_desc.source_code = arrow_ps_source;
		ps_desc.entry_point = "main";
		arrow_ps = ImPlatform_CreateShader(&ps_desc);

		if (arrow_vs && arrow_ps) {
			arrow_program = ImPlatform_CreateShaderProgram(arrow_vs, arrow_ps);
		}
	}
#endif

	// Create gradient shaders based on graphics API
#if (IM_CURRENT_GFX == IM_GFX_VULKAN)
	// Vulkan: Use pre-compiled SPIR-V bytecode
	{
		ImPlatform_ShaderDesc vs_desc = {};
		vs_desc.stage = ImPlatform_ShaderStage_Vertex;
		vs_desc.format = ImPlatform_ShaderFormat_SPIRV;
		vs_desc.bytecode = gradient_vert_spv;
		vs_desc.bytecode_size = gradient_vert_spv_size;
		vs_desc.entry_point = "main";
		gradient_vs = ImPlatform_CreateShader(&vs_desc);

		ImPlatform_ShaderDesc ps_desc = {};
		ps_desc.stage = ImPlatform_ShaderStage_Fragment;
		ps_desc.format = ImPlatform_ShaderFormat_SPIRV;
		ps_desc.bytecode = gradient_frag_spv;
		ps_desc.bytecode_size = gradient_frag_spv_size;
		ps_desc.entry_point = "main";
		gradient_ps = ImPlatform_CreateShader(&ps_desc);

		if (gradient_vs && gradient_ps) {
			gradient_program = ImPlatform_CreateShaderProgram(gradient_vs, gradient_ps);
		}
	}
#else
	// Other APIs: Use source code
	if (gradient_vs_source && gradient_ps_source) {
		ImPlatform_ShaderDesc vs_desc = {};
		vs_desc.stage = ImPlatform_ShaderStage_Vertex;
		vs_desc.format = gradient_format;
		vs_desc.source_code = gradient_vs_source;
		vs_desc.entry_point = "main";
		gradient_vs = ImPlatform_CreateShader(&vs_desc);

		ImPlatform_ShaderDesc ps_desc = {};
		ps_desc.stage = ImPlatform_ShaderStage_Fragment;
		ps_desc.format = gradient_format;
		ps_desc.source_code = gradient_ps_source;
		ps_desc.entry_point = "main";
		gradient_ps = ImPlatform_CreateShader(&ps_desc);

		if (gradient_vs && gradient_ps) {
			gradient_program = ImPlatform_CreateShaderProgram(gradient_vs, gradient_ps);
		}
	}
#endif
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

		if ( !ImPlatform_GfxCheck() )
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
			QueueRenderPassScene( g_RenderPassComposite, 64, DrawRenderPassComposite );
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
			if ( !renderPassTest )		// One pass per frame: the test's.
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
#if (IM_CURRENT_GFX == IM_GFX_OPENGL3)
			ImGui::BulletText("GLSL #version 130");
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10)
			ImGui::BulletText("HLSL Shader Model 4.0 (DirectX 10)");
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
			ImGui::BulletText("HLSL Shader Model 4.0 (DirectX 11)");
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
			ImGui::BulletText("HLSL Shader Model 5.0 (DirectX 12)");
#elif (IM_CURRENT_GFX == IM_GFX_WGPU)
			ImGui::BulletText("WGSL (WebGPU Shading Language)");
#endif
			ImGui::BulletText("Custom pixel shader with SDF function");
			ImGui::BulletText("No custom vertex buffer (uses ImGui's default)");

			ImGui::Separator();

			// Render the arrow using custom shader (using helper function)
			ImPlatform_DrawCustomShaderQuad(arrow_program);

			ImGui::End();
		}

		// Custom Shader Demo Window 2: Linear Gradient (Bottom-left)
		if (gradient_program)
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
#if (IM_CURRENT_GFX == IM_GFX_OPENGL3)
			ImGui::BulletText("GLSL #version 130");
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10) || (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
			ImGui::BulletText("HLSL Shader Model 4.0");
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
			ImGui::BulletText("HLSL Shader Model 5.0");
#elif (IM_CURRENT_GFX == IM_GFX_WGPU)
			ImGui::BulletText("WGSL (WebGPU Shading Language)");
#endif
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
		if ( renderPassTest && ++renderPassFrame == 3 )
		{
			exitCode = CheckRenderPassTest();
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
	if (arrow_vs)
		ImPlatform_DestroyShader(arrow_vs);
	if (arrow_ps)
		ImPlatform_DestroyShader(arrow_ps);

	if (gradient_program)
		ImPlatform_DestroyShaderProgram(gradient_program);
	if (gradient_vs)
		ImPlatform_DestroyShader(gradient_vs);
	if (gradient_ps)
		ImPlatform_DestroyShader(gradient_ps);
#endif

#ifdef IMGUI_HAS_TEXTURES
	DestroyRenderPassScene( g_RenderPassDemo );
	DestroyRenderPassScene( g_RenderPassTest );
	DestroyRenderPassScene( g_RenderPassComposite );
#endif
	ImPlatform_ShutdownGfxAPI();
	ImPlatform_ShutdownWindow();
	ImPlatform_ShutdownPostGfxAPI();

	ImGui::DestroyContext();

	ImPlatform_DestroyWindow();

	return exitCode;
}
