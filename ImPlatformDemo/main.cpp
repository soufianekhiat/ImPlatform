
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

#include <stdio.h>
#include <math.h>

#define IM_HAS_STBI 0

#if IM_HAS_STBI
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#endif

int main()
{
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
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;		// Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;		// Enable Multi-Viewport / Platform Windows
	////io.ConfigViewportsNoAutoMerge = true;
	////io.ConfigViewportsNoTaskBarIcon = true;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	//io.Fonts->AddFontFromFileTTF( "../extern/FiraCode/distr/ttf/FiraCode-Medium.ttf", 16.0f );

	// Setup DPI scaling (Win32 only)
	ImGuiStyle& style = ImGui::GetStyle();
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
	float dpi_scale = ImPlatform_App_GetDpiScale_Win32();
	style.ScaleAllSizes(dpi_scale);
	style.FontScaleDpi = dpi_scale;
	io.ConfigDpiScaleFonts = true;
	io.ConfigDpiScaleViewports = true;
#endif

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	if ( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
	{
		style.WindowRounding = 0.0f;
		style.Colors[ ImGuiCol_WindowBg ].w = 1.0f;
	}

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

	// Create test textures using new Texture Creation API
	ImPlatform_TextureDesc tex_desc = ImPlatform_TextureDesc_Default(width, height);
	ImTextureID img = ImPlatform_CreateTexture(data, &tex_desc);
	ImTextureID img_white = ImPlatform_CreateTexture(white_data, &tex_desc);

#if IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER
	// ========================================================================
	// Custom Shader Demo: Arrow SDF Shape
	// ========================================================================

	// Define shader source code based on graphics API
	const char* arrow_vs_source = nullptr;
	const char* arrow_ps_source = nullptr;
	ImPlatform_ShaderFormat arrow_format = ImPlatform_ShaderFormat_GLSL;

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
		"    p.x = abs(p.x);\n"
		"    float d1 = max(p.y - 0.3, -(p.y + 0.3));\n"
		"    float d2 = max(p.x + p.y - 0.3, p.x - p.y - 0.3);\n"
		"    return max(d1, d2);\n"
		"}\n"
		"void main() {\n"
		"    vec2 uv = Frag_UV * 2.0 - 1.0;\n"
		"    float d = sdArrow(uv);\n"
		"    float alpha = 1.0 - smoothstep(0.0, 0.02, d);\n"
		"    Out_Color = vec4(1.0, 0.5, 0.2, 1.0) * alpha;\n"
		"}\n";

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10) || (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
	// HLSL for DirectX 10/11
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
		"    p.x = abs(p.x);\n"
		"    float d1 = max(p.y - 0.3, -(p.y + 0.3));\n"
		"    float d2 = max(p.x + p.y - 0.3, p.x - p.y - 0.3);\n"
		"    return max(d1, d2);\n"
		"}\n"
		"float4 main(PS_INPUT input) : SV_TARGET {\n"
		"    // Arrow SDF shader - renders arrow shape\n"
		"    float2 uv = input.uv * 2.0 - 1.0;\n"
		"    float d = sdArrow(uv);\n"
		"    float alpha = 1.0 - smoothstep(0.0, 0.02, d);\n"
		"    return float4(1.0, 0.5, 0.2, 1.0) * alpha;\n"
		"}\n";
#endif

	// ========================================================================
	// Custom Shader Demo: Linear Gradient with Custom Vertex Buffer
	// ========================================================================

	const char* gradient_vs_source = nullptr;
	const char* gradient_ps_source = nullptr;
	ImPlatform_ShaderFormat gradient_format = ImPlatform_ShaderFormat_GLSL;

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

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10) || (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
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
		"cbuffer pixelBuffer : register(b0) {\n"
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
#endif

	// Create shaders and programs
	ImPlatform_Shader arrow_vs = nullptr;
	ImPlatform_Shader arrow_ps = nullptr;
	ImPlatform_ShaderProgram arrow_program = nullptr;

	ImPlatform_Shader gradient_vs = nullptr;
	ImPlatform_Shader gradient_ps = nullptr;
	ImPlatform_ShaderProgram gradient_program = nullptr;

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
#endif // IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER

	if (!img || !img_white)
	{
		fprintf(stderr, "ImPlatform: Failed to create textures.\n");
	}

#if IM_HAS_STBI
	stbi_image_free( data );
#else
	IM_FREE( data );
	IM_FREE( white_data );
#endif

	ImVec4 clear_color = ImVec4( 0.461f, 0.461f, 0.461f, 1.0f );
	while ( ImPlatform_PlatformContinue() )
	{
		ImPlatform_PlatformEvents();

		if ( !ImPlatform_GfxCheck() )
		{
			continue;
		}

		ImPlatform_GfxAPINewFrame();
		ImPlatform_PlatformNewFrame();

		ImGui::NewFrame();

		// ImGui Demo Code
		bool show = true;
		ImGui::ShowDemoWindow( &show );

		// Texture API Demo Window
		if (img && img_white)
		{
			ImGui::Begin("ImPlatform Texture API Demo");
			ImGui::Text("Checkerboard texture created with ImPlatform_CreateTexture:");
			ImGui::Image(img, ImVec2(256, 256));

			ImGui::Separator();
			ImGui::Text("White texture:");
			ImGui::Image(img_white, ImVec2(128, 128));

			ImGui::Separator();
			ImGui::Text("This demonstrates the new Texture Creation API:");
			ImGui::BulletText("ImPlatform_TextureDesc_Default()");
			ImGui::BulletText("ImPlatform_CreateTexture()");
			ImGui::BulletText("Works with OpenGL3, DX11, and other backends");
			ImGui::End();
		}

#if IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER
		// Custom Shader Demo Window 1: Arrow SDF Shape
		if (arrow_program)
		{
			ImGui::Begin("Custom Shader: Arrow SDF");
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
#endif
			ImGui::BulletText("Custom pixel shader with SDF function");
			ImGui::BulletText("No custom vertex buffer (uses ImGui's default)");

			ImGui::Separator();

			// Render the arrow using custom shader (using helper function)
			ImPlatform_DrawCustomShaderQuad(arrow_program);

			ImGui::End();
		}

		// Custom Shader Demo Window 2: Linear Gradient
		if (gradient_program)
		{
			ImGui::Begin("Custom Shader: Linear Gradient");
			ImGui::Text("This demonstrates a custom shader with adjustable colors.");
			ImGui::Text("Linear gradient interpolating between two colors.");

			ImGui::Separator();
			ImGui::ColorEdit4("Start Color", (float*)&gradient_color_start);
			ImGui::ColorEdit4("End Color", (float*)&gradient_color_end);

			ImGui::Separator();
			ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Shader API Used:");
#if (IM_CURRENT_GFX == IM_GFX_OPENGL3)
			ImGui::BulletText("GLSL #version 130");
			ImGui::BulletText("Uniforms: ColorStart, ColorEnd");
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10) || (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
			ImGui::BulletText("HLSL Shader Model 4.0");
			ImGui::BulletText("Note: Constant buffers not yet implemented");
#endif
			ImGui::BulletText("Pixel shader interpolates between two colors");

			ImGui::Separator();

			// Update shader uniforms
#if (IM_CURRENT_GFX == IM_GFX_OPENGL3)
			ImPlatform_SetShaderUniform(gradient_program, "ColorStart", &gradient_color_start, sizeof(ImVec4));
			ImPlatform_SetShaderUniform(gradient_program, "ColorEnd", &gradient_color_end, sizeof(ImVec4));
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10) || (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
			// For DirectX, pack both colors into a single constant buffer
			struct GradientColors {
				ImVec4 ColorStart;
				ImVec4 ColorEnd;
			} colors;
			colors.ColorStart = gradient_color_start;
			colors.ColorEnd = gradient_color_end;
			ImPlatform_SetShaderUniform(gradient_program, "pixelBuffer", &colors, sizeof(colors));
#endif

			// Render the gradient using custom shader (using helper function)
			ImPlatform_DrawCustomShaderQuad(gradient_program);

			ImGui::End();
		}
#else
		// Show a message when custom shaders are not supported
		{
			ImGui::Begin("Custom Shader Support");
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
			ImGui::BulletText("WebGPU (stub)");
			ImGui::End();
		}
#endif // IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER

		ImGui::Render();

		ImPlatform_GfxAPIClear( clear_color );
		ImPlatform_GfxAPIRender( clear_color );

		// Update and Render additional Platform Windows
		ImPlatform_GfxViewportPre();
		ImPlatform_GfxViewportPost();

		t += ImGui::GetIO().DeltaTime;

		ImPlatform_GfxAPISwapBuffer();
	}

	// Cleanup textures
	if (img)
		ImPlatform_DestroyTexture(img);
	if (img_white)
		ImPlatform_DestroyTexture(img_white);

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

	ImPlatform_ShutdownGfxAPI();
	ImPlatform_ShutdownWindow();
	ImPlatform_ShutdownPostGfxAPI();

	ImGui::DestroyContext();

	ImPlatform_DestroyWindow();

	return 0;
}
