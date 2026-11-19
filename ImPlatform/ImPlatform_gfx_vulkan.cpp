// dear imgui: Graphics API Abstraction - Vulkan Backend
// This handles Vulkan instance, device creation, rendering, and presentation

#include "ImPlatform_Internal.h"

#ifdef IM_GFX_VULKAN

#include "../imgui/imgui.h"
#include "../imgui/backends/imgui_impl_vulkan.h"
#include <stdio.h>
#include <stdlib.h>

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
    #include <vulkan/vulkan_win32.h>
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    #define GLFW_INCLUDE_NONE
    #define GLFW_INCLUDE_VULKAN
    #include <GLFW/glfw3.h>
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
    #include <SDL.h>
    #include <SDL_vulkan.h>
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
    #include <SDL3/SDL.h>
    #include <SDL3/SDL_vulkan.h>
#endif

// Global state
static ImPlatform_GfxData_Vulkan g_GfxData = {};
unsigned int g_ImPlatform_BackbufferW = 0;
unsigned int g_ImPlatform_BackbufferH = 0;
static VkAllocationCallbacks* g_Allocator = NULL;

// Global VK_EXT_pipeline_cache (core since Vulkan 1.0). Caches compiled
// pipeline state objects across runs. Loaded from disk at device creation
// time, passed to every vkCreateGraphicsPipelines / vkCreateComputePipelines
// call, and serialized back to disk on cleanup.
static VkPipelineCache g_VulkanPipelineCache = VK_NULL_HANDLE;

// Render texture tracking
struct ImPlatform_RTTracking_Vulkan {
    VkImage             image;
    VkDeviceMemory      imageMemory;
    VkImageView         imageView;
    VkDescriptorSet     descriptorSet;
    VkRenderPass        renderPass;
    VkFramebuffer       framebuffer;
    VkCommandPool       commandPool;
    VkCommandBuffer     commandBuffer;
    unsigned int        width, height;
    VkFormat            format;
    bool                rendered;       // A render pass wrote it (before that its layout is undefined).
    ImPlatform_RTTracking_Vulkan* next;
};
static ImPlatform_RTTracking_Vulkan* g_RTTrackingHead  = NULL;

// Every texture ImPlatform creates, by its ImTextureID (the descriptor set): readback and compute need
// the image behind it, which a descriptor set does not give back.
struct ImPlatform_TexReg_Vulkan {
    VkDescriptorSet         id;
    VkImage                 image;
    VkImageView             view;
    VkDeviceMemory          memory;       // null for render textures (their tracking entry owns the objects)
    VkFormat                format;
    unsigned int            width, height, bpp;
    VkImageLayout           idleLayout;   // layout between uses; storage textures stay GENERAL
    bool                    storage;
    ImPlatform_TexReg_Vulkan* next;
};
static ImPlatform_TexReg_Vulkan* g_TexRegHead = NULL;
static void ImPlatform_Vulkan_RegisterTexture(VkDescriptorSet id, VkImage image, VkImageView view, VkDeviceMemory memory, VkFormat format,
                                              unsigned int w, unsigned int h, int bpp, VkImageLayout idle, bool storage)
{
    ImPlatform_TexReg_Vulkan* e = new ImPlatform_TexReg_Vulkan();
    e->id = id; e->image = image; e->view = view; e->memory = memory; e->format = format; e->width = w; e->height = h; e->bpp = (unsigned int)bpp;
    e->idleLayout = idle; e->storage = storage; e->next = g_TexRegHead;
    g_TexRegHead = e;
}
static ImPlatform_TexReg_Vulkan* ImPlatform_Vulkan_FindTexture(ImTextureID id)
{
    for (ImPlatform_TexReg_Vulkan* e = g_TexRegHead; e; e = e->next)
        if (e->id == (VkDescriptorSet)id)
            return e;
    return NULL;
}
static void ImPlatform_Vulkan_ForgetShaderTexture(ImTextureID texture);
static ImTextureID g_WhiteTexture = 0;      // Custom shaders' t1..t8 when unset
static ImPlatform_RTTracking_Vulkan* g_ActiveRTEntry   = NULL;
static ImGui_ImplVulkanH_Window g_MainWindowData;  // Don't use = {} - let constructor run!
static bool g_SwapChainRebuild = false;
static bool g_FrameBegun = false;               // Main viewport command buffer is recording (set in GfxAPIClear)
static uint32_t g_QueueFamily = (uint32_t)-1;
static VkCommandPool g_UploadCommandPool = VK_NULL_HANDLE;     // Dedicated pool for texture uploads (never touch the frame command buffers)
static VkCommandBuffer g_UploadCommandBuffer = VK_NULL_HANDLE;
// Compute: layout shared by every compute shader (see ImPlatform_Vulkan_ComputeLayout)
static VkDescriptorSetLayout g_ComputeSetLayout      = VK_NULL_HANDLE;
static VkPipelineLayout      g_ComputePipelineLayout = VK_NULL_HANDLE;
static VkSampler             g_ComputeSampler        = VK_NULL_HANDLE;

// Uniform block API state
static ImPlatform_ShaderProgram g_CurrentUniformBlockProgram = nullptr;

// Current command buffer for custom shader rendering, and its render pass (custom pipelines are made per render pass;
// null: the main window's, which secondary viewports' render passes are compatible with)
static VkCommandBuffer g_CurrentCommandBuffer = VK_NULL_HANDLE;
static VkRenderPass g_CurrentRenderPass = VK_NULL_HANDLE;
static void ImPlatform_Vulkan_ReleaseConstantRing();

// The render passes being recorded on the frame's command buffer, innermost last: the main viewport's or a queued render
// pass's, then the targets of ImPlatform_PushRenderTarget. Draw callbacks end the innermost to copy or switch targets and
// begin it again with a LOAD pass. Empty while secondary viewports render (the ImGui backend owns their passes).
struct ImPlatform_ActivePass_Vulkan
{
    VkRenderPass  renderPass;       // The pass pipelines are created for (compatible with its LOAD variant).
    VkFramebuffer framebuffer;
    VkImage       image;
    VkFormat      format;
    uint32_t      width, height;
    VkImageLayout restLayout;       // The image's layout between passes.
};
static ImVector<ImPlatform_ActivePass_Vulkan> g_ActivePasses;
static int g_PushedTargetCount = 0;     // Entries of g_ActivePasses pushed by ImPlatform_PushRenderTarget
static void ImPlatform_Vulkan_ResumePass(VkCommandBuffer cmd);

// Current draw data for custom shader rendering (needed for multi-viewport)
static ImDrawData* g_CurrentDrawData = nullptr;

// Helper functions
static void check_vk_result(VkResult err)
{
    if (err == 0)
        return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}

#ifdef _DEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_report(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT objectType,
    uint64_t object,
    size_t location,
    int32_t messageCode,
    const char* pLayerPrefix,
    const char* pMessage,
    void* pUserData)
{
    (void)flags; (void)object; (void)location; (void)messageCode; (void)pUserData; (void)pLayerPrefix;
    fprintf(stderr, "[vulkan] Debug report from ObjectType: %i\nMessage: %s\n\n", objectType, pMessage);
    return VK_FALSE;
}
#endif

static bool IsExtensionAvailable(const VkExtensionProperties* properties, uint32_t property_count, const char* extension)
{
    for (uint32_t i = 0; i < property_count; i++)
        if (strcmp(properties[i].extensionName, extension) == 0)
            return true;
    return false;
}

// ----------------------------------------------------------------------------
// VK_EXT_pipeline_cache disk persistence helpers
// ----------------------------------------------------------------------------
// Cache file layout: opaque blob produced/consumed by the Vulkan driver.
// The driver embeds a 32-byte header that begins with a 4-byte header length,
// a 4-byte cache header version, a 4-byte vendor ID, a 4-byte device ID, and
// a 16-byte pipeline cache UUID. vkCreatePipelineCache will reject the data
// if any of those fields don't match the current device, so we don't need to
// hash source or track driver versions ourselves.

static void ImPlatform_Vulkan_BuildPipelineCachePath(char* out_path, size_t out_size)
{
    // cache_key="vulkan_global", entry="pipeline", ext="cache", hash=0
    ImPlatform_ShaderCacheBuildPath(out_path, out_size,
                                    "vulkan_global", "pipeline", "cache", 0);
}

static void ImPlatform_Vulkan_InitPipelineCache(VkDevice device)
{
    if (g_VulkanPipelineCache != VK_NULL_HANDLE)
        return;

    char cache_path[512];
    ImPlatform_Vulkan_BuildPipelineCachePath(cache_path, sizeof(cache_path));

    size_t data_size = 0;
    void* data = ImPlatform_ShaderCacheLoad(cache_path, &data_size);

    VkPipelineCacheCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    info.initialDataSize = data ? data_size : 0;
    info.pInitialData = data;

    VkResult err = vkCreatePipelineCache(device, &info, g_Allocator, &g_VulkanPipelineCache);
    if (err != VK_SUCCESS)
    {
        // Driver rejected the initial data (e.g. GPU or driver changed). Retry
        // with an empty cache -- this is not a fatal error.
        fprintf(stderr, "[ImPlatform shader cache] VK pipeline cache: rejected on-disk data (VkResult %d), starting fresh\n", err);
        info.initialDataSize = 0;
        info.pInitialData = NULL;
        err = vkCreatePipelineCache(device, &info, g_Allocator, &g_VulkanPipelineCache);
        if (err != VK_SUCCESS)
        {
            fprintf(stderr, "[ImPlatform shader cache] VK pipeline cache: vkCreatePipelineCache failed (VkResult %d)\n", err);
            g_VulkanPipelineCache = VK_NULL_HANDLE;
        }
    }
    else if (data && data_size > 0)
    {
        fprintf(stderr, "[ImPlatform shader cache] VK pipeline cache: loaded %zu bytes from disk\n", data_size);
    }
    else
    {
        fprintf(stderr, "[ImPlatform shader cache] VK pipeline cache: initialized empty (no cache file)\n");
    }

    if (data) free(data);
}

static void ImPlatform_Vulkan_SavePipelineCache(VkDevice device)
{
    if (g_VulkanPipelineCache == VK_NULL_HANDLE || device == VK_NULL_HANDLE)
        return;

    size_t data_size = 0;
    VkResult err = vkGetPipelineCacheData(device, g_VulkanPipelineCache, &data_size, NULL);
    if (err != VK_SUCCESS || data_size == 0)
        return;

    void* data = malloc(data_size);
    if (!data)
        return;

    err = vkGetPipelineCacheData(device, g_VulkanPipelineCache, &data_size, data);
    if (err != VK_SUCCESS)
    {
        free(data);
        return;
    }

    char cache_path[512];
    ImPlatform_Vulkan_BuildPipelineCachePath(cache_path, sizeof(cache_path));

    if (ImPlatform_ShaderCacheSave(cache_path, data, data_size))
    {
        fprintf(stderr, "[ImPlatform shader cache] VK pipeline cache: saved %zu bytes to disk\n", data_size);
    }
    free(data);
}

static void ImPlatform_Vulkan_DestroyPipelineCache(VkDevice device)
{
    if (g_VulkanPipelineCache == VK_NULL_HANDLE)
        return;
    vkDestroyPipelineCache(device, g_VulkanPipelineCache, g_Allocator);
    g_VulkanPipelineCache = VK_NULL_HANDLE;
}

// Internal API - Get Vulkan gfx data
ImPlatform_GfxData_Vulkan* ImPlatform_Gfx_GetData_Vulkan(void)
{
    return &g_GfxData;
}

// Setup Vulkan instance and device
static bool SetupVulkan(const char** instance_extensions, uint32_t instance_extensions_count)
{
    VkResult err;

    // Create Vulkan Instance
    {
        // Vulkan 1.1 accepts SPIR-V 1.3, the oldest version Slang emits (custom shaders are compiled with -profile spirv_1_3)
        VkApplicationInfo app_info = {};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.apiVersion = VK_API_VERSION_1_1;
        VkInstanceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &app_info;

        // Enumerate available extensions
        uint32_t properties_count;
        vkEnumerateInstanceExtensionProperties(NULL, &properties_count, NULL);
        VkExtensionProperties* properties = (VkExtensionProperties*)malloc(sizeof(VkExtensionProperties) * properties_count);
        err = vkEnumerateInstanceExtensionProperties(NULL, &properties_count, properties);
        check_vk_result(err);

        // Count total extensions needed
        uint32_t total_extension_count = instance_extensions_count;
        if (IsExtensionAvailable(properties, properties_count, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
            total_extension_count++;
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
        if (IsExtensionAvailable(properties, properties_count, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
        {
            total_extension_count++;
            create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        }
#endif

        // Build extensions array
        const char** extensions = (const char**)malloc(sizeof(const char*) * (total_extension_count + 1));
        uint32_t ext_idx = 0;
        for (uint32_t i = 0; i < instance_extensions_count; i++)
            extensions[ext_idx++] = instance_extensions[i];
        if (IsExtensionAvailable(properties, properties_count, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
            extensions[ext_idx++] = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
        if (IsExtensionAvailable(properties, properties_count, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
            extensions[ext_idx++] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
#endif

#ifdef _DEBUG
        // The validation layer ships with the Vulkan SDK: without it, run unvalidated instead of failing instance creation.
        const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
        uint32_t layer_count = 0;
        vkEnumerateInstanceLayerProperties(&layer_count, NULL);
        VkLayerProperties* layer_props = (VkLayerProperties*)malloc(sizeof(VkLayerProperties) * (layer_count + 1));
        vkEnumerateInstanceLayerProperties(&layer_count, layer_props);
        for (uint32_t i = 0; i < layer_count; i++)
            if (strcmp(layer_props[i].layerName, layers[0]) == 0)
            {
                create_info.enabledLayerCount = 1;
                create_info.ppEnabledLayerNames = layers;
                extensions[ext_idx++] = "VK_EXT_debug_report";
                break;
            }
        free(layer_props);
#endif

        create_info.enabledExtensionCount = ext_idx;
        create_info.ppEnabledExtensionNames = extensions;
        err = vkCreateInstance(&create_info, g_Allocator, &g_GfxData.instance);
        check_vk_result(err);

        free(properties);
        free(extensions);

        // Setup debug report callback
#ifdef _DEBUG
        auto f_vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(g_GfxData.instance, "vkCreateDebugReportCallbackEXT");
        if (f_vkCreateDebugReportCallbackEXT)
        {
            VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
            debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
            debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
            debug_report_ci.pfnCallback = debug_report;
            debug_report_ci.pUserData = NULL;
            err = f_vkCreateDebugReportCallbackEXT(g_GfxData.instance, &debug_report_ci, g_Allocator, &g_GfxData.debugReport);
            check_vk_result(err);
        }
#endif
    }

    // Select Physical Device (GPU)
    g_GfxData.physicalDevice = ImGui_ImplVulkanH_SelectPhysicalDevice(g_GfxData.instance);
    if (g_GfxData.physicalDevice == VK_NULL_HANDLE)
        return false;

    // Select graphics queue family
    g_QueueFamily = ImGui_ImplVulkanH_SelectQueueFamilyIndex(g_GfxData.physicalDevice);
    if (g_QueueFamily == (uint32_t)-1)
        return false;

    // Create Logical Device
    {
        const char* device_extensions[2] = { "VK_KHR_swapchain", NULL };
        uint32_t device_extensions_count = 1;

        // Check for portability subset
        uint32_t properties_count;
        vkEnumerateDeviceExtensionProperties(g_GfxData.physicalDevice, NULL, &properties_count, NULL);
        VkExtensionProperties* properties = (VkExtensionProperties*)malloc(sizeof(VkExtensionProperties) * properties_count);
        vkEnumerateDeviceExtensionProperties(g_GfxData.physicalDevice, NULL, &properties_count, properties);
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
        if (IsExtensionAvailable(properties, properties_count, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
        {
            device_extensions[device_extensions_count++] = VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME;
        }
#endif
        free(properties);

        const float queue_priority[] = { 1.0f };
        VkDeviceQueueCreateInfo queue_info = {};
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = g_QueueFamily;
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = queue_priority;

        VkDeviceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount = 1;
        create_info.pQueueCreateInfos = &queue_info;
        create_info.enabledExtensionCount = device_extensions_count;
        create_info.ppEnabledExtensionNames = device_extensions;
        err = vkCreateDevice(g_GfxData.physicalDevice, &create_info, g_Allocator, &g_GfxData.device);
        check_vk_result(err);
        vkGetDeviceQueue(g_GfxData.device, g_QueueFamily, 0, &g_GfxData.queue);
    }

    // Create Descriptor Pool
    // Since 1.92.8 the ImGui Vulkan backend uses separate VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE (one per texture)
    // and VK_DESCRIPTOR_TYPE_SAMPLER (linear + nearest) descriptors instead of combined image samplers.
    // Combined image samplers are kept available for application code sharing this pool.
    {
        VkDescriptorPoolSize pool_sizes[] =
        {
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 0;
        for (VkDescriptorPoolSize& pool_size : pool_sizes)
            pool_info.maxSets += pool_size.descriptorCount;
        pool_info.poolSizeCount = (uint32_t)IM_COUNTOF(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        err = vkCreateDescriptorPool(g_GfxData.device, &pool_info, g_Allocator, &g_GfxData.descriptorPool);
        check_vk_result(err);
    }

    // Initialize disk-backed VkPipelineCache so subsequent vkCreateGraphicsPipelines
    // calls can reuse compiled pipeline state objects from previous runs.
    ImPlatform_Vulkan_InitPipelineCache(g_GfxData.device);

    return true;
}

// Select surface format / present mode and create the main window swapchain (same as upstream SetupVulkanWindow)
static bool ImPlatform_Vulkan_SetupWindow(ImPlatform_GfxData_Vulkan* pData, int width, int height)
{
    g_MainWindowData.Surface = pData->surface;
    pData->minImageCount = 2;

    // Check for WSI support
    VkBool32 res;
    vkGetPhysicalDeviceSurfaceSupportKHR(pData->physicalDevice, g_QueueFamily, g_MainWindowData.Surface, &res);
    if (res != VK_TRUE)
    {
        fprintf(stderr, "[vulkan] Error: no WSI support on physical device\n");
        return false;
    }

    // Select Surface Format
    const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    g_MainWindowData.SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(pData->physicalDevice, g_MainWindowData.Surface, requestSurfaceImageFormat, (size_t)IM_COUNTOF(requestSurfaceImageFormat), requestSurfaceColorSpace);

    // Select Present Mode
#ifdef IMPLATFORM_VULKAN_UNLIMITED_FRAMERATE
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
#else
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR }; // FIFO is always supported (vsync enabled)
#endif
    g_MainWindowData.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(pData->physicalDevice, g_MainWindowData.Surface, &present_modes[0], IM_COUNTOF(present_modes));

    // Create SwapChain, RenderPass, Framebuffer, etc.
    IM_ASSERT(pData->minImageCount >= 2);
    ImGui_ImplVulkanH_CreateOrResizeWindow(pData->instance, pData->physicalDevice, pData->device,
        &g_MainWindowData, g_QueueFamily, g_Allocator, width, height, pData->minImageCount, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    g_ImPlatform_BackbufferW = (unsigned int)g_MainWindowData.Width;
    g_ImPlatform_BackbufferH = (unsigned int)g_MainWindowData.Height;

    return true;
}

// Platform-specific device creation implementations
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
bool ImPlatform_Gfx_CreateDevice_Vulkan(void* hWnd, ImPlatform_GfxData_Vulkan* pData)
{
    HWND hwnd = (HWND)hWnd;

    // Get required instance extensions for Win32
    const char* extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    };

    if (!SetupVulkan(extensions, 2))
        return false;

    // Create Win32 Surface
    VkWin32SurfaceCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = GetModuleHandle(NULL);
    createInfo.hwnd = hwnd;
    VkResult err = vkCreateWin32SurfaceKHR(pData->instance, &createInfo, g_Allocator, &pData->surface);
    check_vk_result(err);

    // Use the actual client size (the window is already created with its DPI-scaled size)
    RECT rect;
    ::GetClientRect(hwnd, &rect);
    int w = (int)(rect.right - rect.left);
    int h = (int)(rect.bottom - rect.top);
    if (w <= 0 || h <= 0) { w = 1280; h = 800; }

    return ImPlatform_Vulkan_SetupWindow(pData, w, h);
}
#endif

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
bool ImPlatform_Gfx_CreateDevice_Vulkan(void* pWindow, ImPlatform_GfxData_Vulkan* pData)
{
    GLFWwindow* window = (GLFWwindow*)pWindow;

    if (!glfwVulkanSupported())
    {
        fprintf(stderr, "[vulkan] Error: GLFW: Vulkan not supported\n");
        return false;
    }

    // Get required extensions from GLFW
    uint32_t extensions_count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&extensions_count);

    if (!SetupVulkan(extensions, extensions_count))
        return false;

    // Create Window Surface
    VkResult err = glfwCreateWindowSurface(pData->instance, window, g_Allocator, &pData->surface);
    check_vk_result(err);

    // Create Framebuffers
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    return ImPlatform_Vulkan_SetupWindow(pData, w, h);
}
#endif

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
bool ImPlatform_Gfx_CreateDevice_Vulkan(void* pWindow, ImPlatform_GfxData_Vulkan* pData)
{
    SDL_Window* window = (SDL_Window*)pWindow;

    // Get required extensions from SDL2
    unsigned int extensions_count = 0;
    SDL_Vulkan_GetInstanceExtensions(window, &extensions_count, NULL);
    const char** extensions = (const char**)malloc(sizeof(const char*) * extensions_count);
    SDL_Vulkan_GetInstanceExtensions(window, &extensions_count, extensions);

    if (!SetupVulkan(extensions, extensions_count))
    {
        free(extensions);
        return false;
    }
    free(extensions);

    // Create Window Surface
    if (!SDL_Vulkan_CreateSurface(window, pData->instance, &pData->surface))
        return false;

    // Create Framebuffers
    int w, h;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    return ImPlatform_Vulkan_SetupWindow(pData, w, h);
}
#endif

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
bool ImPlatform_Gfx_CreateDevice_Vulkan(void* pWindow, ImPlatform_GfxData_Vulkan* pData)
{
    SDL_Window* window = (SDL_Window*)pWindow;

    // Get required extensions from SDL3
    uint32_t extensions_count = 0;
    const char* const* extensions_sdl = SDL_Vulkan_GetInstanceExtensions(&extensions_count);
    const char** extensions = (const char**)malloc(sizeof(const char*) * extensions_count);
    for (uint32_t i = 0; i < extensions_count; i++)
        extensions[i] = extensions_sdl[i];

    if (!SetupVulkan(extensions, extensions_count))
    {
        free(extensions);
        return false;
    }
    free(extensions);

    // Create Window Surface
    if (!SDL_Vulkan_CreateSurface(window, pData->instance, g_Allocator, &pData->surface))
        return false;

    // Create Framebuffers
    int w, h;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    return ImPlatform_Vulkan_SetupWindow(pData, w, h);
}
#endif

// Internal API - Cleanup Vulkan device
void ImPlatform_Gfx_CleanupDevice_Vulkan(ImPlatform_GfxData_Vulkan* pData)
{
    // Serialize and tear down the pipeline cache before the device goes away.
    ImPlatform_Vulkan_SavePipelineCache(pData->device);
    ImPlatform_Vulkan_DestroyPipelineCache(pData->device);

    vkDestroyDescriptorPool(pData->device, pData->descriptorPool, g_Allocator);

#ifdef _DEBUG
    if (pData->debugReport != VK_NULL_HANDLE)
    {
        auto f_vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(pData->instance, "vkDestroyDebugReportCallbackEXT");
        if (f_vkDestroyDebugReportCallbackEXT)
            f_vkDestroyDebugReportCallbackEXT(pData->instance, pData->debugReport, g_Allocator);
    }
#endif

    vkDestroyDevice(pData->device, g_Allocator);
    vkDestroyInstance(pData->instance, g_Allocator);
}

// ImPlatform API - InitGfxAPI
IMPLATFORM_API bool ImPlatform_InitGfxAPI(void)
{
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
    HWND hWnd = ImPlatform_App_GetHWND();
    return ImPlatform_Gfx_CreateDevice_Vulkan(hWnd, &g_GfxData);
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    GLFWwindow* pWindow = ImPlatform_App_GetGLFWWindow();
    return ImPlatform_Gfx_CreateDevice_Vulkan(pWindow, &g_GfxData);
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
    SDL_Window* pWindow = ImPlatform_App_GetSDL2Window();
    return ImPlatform_Gfx_CreateDevice_Vulkan(pWindow, &g_GfxData);
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
    SDL_Window* pWindow = ImPlatform_App_GetSDL3Window();
    return ImPlatform_Gfx_CreateDevice_Vulkan(pWindow, &g_GfxData);
#else
    return false;
#endif
}

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
// The Win32 platform backend doesn't know about Vulkan: provide the surface creation for secondary viewports (same as upstream example_win32_vulkan)
static int ImPlatform_Win32_CreateVkSurface(ImGuiViewport* viewport, ImU64 vk_instance, const void* vk_allocator, ImU64* out_vk_surface)
{
    VkWin32SurfaceCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hwnd = (HWND)viewport->PlatformHandleRaw;
    createInfo.hinstance = ::GetModuleHandle(NULL);
    return (int)vkCreateWin32SurfaceKHR((VkInstance)vk_instance, &createInfo, (VkAllocationCallbacks*)vk_allocator, (VkSurfaceKHR*)out_vk_surface);
}
#endif

// ImPlatform API - InitGfx
IMPLATFORM_API bool ImPlatform_InitGfx(void)
{
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32) && defined(IMGUI_HAS_VIEWPORT)
    ImGui::GetPlatformIO().Platform_CreateVkSurface = ImPlatform_Win32_CreateVkSurface;
#endif

    // Setup ImGui Vulkan backend
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = g_GfxData.instance;
    init_info.PhysicalDevice = g_GfxData.physicalDevice;
    init_info.Device = g_GfxData.device;
    init_info.QueueFamily = g_QueueFamily;
    init_info.Queue = g_GfxData.queue;
    init_info.PipelineCache = g_VulkanPipelineCache;
    init_info.DescriptorPool = g_GfxData.descriptorPool;
    init_info.MinImageCount = g_GfxData.minImageCount;
    init_info.ImageCount = g_MainWindowData.ImageCount * 2;     // Its buffers turn per render: a render pass and the frame.
    init_info.Allocator = g_Allocator;
    init_info.CheckVkResultFn = check_vk_result;

    // Pipeline info for main viewport (new API since 2025/09/26)
    init_info.PipelineInfoMain.RenderPass = g_MainWindowData.RenderPass;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

#ifdef IMGUI_HAS_VIEWPORT
    // Pipeline info for secondary viewports (multi-viewport support)
    init_info.PipelineInfoForViewports = init_info.PipelineInfoMain;
#endif

    if (!ImGui_ImplVulkan_Init(&init_info))
        return false;

    return true;
}

// ImPlatform API - GfxCheck
IMPLATFORM_API bool ImPlatform_GfxCheck(void)
{
    // Resize swap chain? (polled every frame, same as upstream examples)
    int fb_width = 0, fb_height = 0;
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
    HWND hwnd = ImPlatform_App_GetHWND();
    if (!::IsIconic(hwnd))
    {
        RECT rect;
        ::GetClientRect(hwnd, &rect);
        fb_width = (int)(rect.right - rect.left);
        fb_height = (int)(rect.bottom - rect.top);
    }
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    glfwGetFramebufferSize(ImPlatform_App_GetGLFWWindow(), &fb_width, &fb_height);
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
    SDL_GetWindowSizeInPixels(ImPlatform_App_GetSDL2Window(), &fb_width, &fb_height);
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
    SDL_GetWindowSizeInPixels(ImPlatform_App_GetSDL3Window(), &fb_width, &fb_height);
#endif

    if (fb_width <= 0 || fb_height <= 0)
    {
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
        ::Sleep(10);
#endif
        return false; // Minimized: skip frame
    }

    if (g_SwapChainRebuild || g_MainWindowData.Width != fb_width || g_MainWindowData.Height != fb_height)
    {
        ImGui_ImplVulkan_SetMinImageCount(g_GfxData.minImageCount);
        ImGui_ImplVulkanH_CreateOrResizeWindow(g_GfxData.instance, g_GfxData.physicalDevice, g_GfxData.device,
            &g_MainWindowData, g_QueueFamily, g_Allocator, fb_width, fb_height, g_GfxData.minImageCount, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        g_MainWindowData.FrameIndex = 0;
        g_SwapChainRebuild = false;
        g_ImPlatform_BackbufferW = (unsigned int)g_MainWindowData.Width;
        g_ImPlatform_BackbufferH = (unsigned int)g_MainWindowData.Height;
    }

    return true;
}

// ImPlatform API - GfxAPINewFrame
IMPLATFORM_API void ImPlatform_GfxAPINewFrame(void)
{
    ImGui_ImplVulkan_NewFrame();
}

// ImPlatform API - GfxAPIClear
// Acquires the next swapchain image and begins recording the main viewport command buffer (upstream FrameRender, first half)
static void ImPlatform_RunRenderPass(VkCommandBuffer cmd);
static void ImPlatform_ReleaseRenderPasses();

IMPLATFORM_API bool ImPlatform_GfxAPIClear(ImVec4 const vClearColor)
{
    g_FrameBegun = false;

    ImGui::Render();
    ImDrawData* main_draw_data = ImGui::GetDrawData();
    const bool main_is_minimized = (main_draw_data->DisplaySize.x <= 0.0f || main_draw_data->DisplaySize.y <= 0.0f);
    if (main_is_minimized)
        return false;

    ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
    wd->ClearValue.color.float32[0] = vClearColor.x * vClearColor.w;
    wd->ClearValue.color.float32[1] = vClearColor.y * vClearColor.w;
    wd->ClearValue.color.float32[2] = vClearColor.z * vClearColor.w;
    wd->ClearValue.color.float32[3] = vClearColor.w;

    VkSemaphore image_acquired_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
    VkResult err = vkAcquireNextImageKHR(g_GfxData.device, wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
        g_SwapChainRebuild = true;
    if (err == VK_ERROR_OUT_OF_DATE_KHR)
        return false;
    if (err != VK_SUBOPTIMAL_KHR)
        check_vk_result(err);

    ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
    {
        err = vkWaitForFences(g_GfxData.device, 1, &fd->Fence, VK_TRUE, UINT64_MAX); // wait indefinitely instead of periodically checking
        check_vk_result(err);

        err = vkResetFences(g_GfxData.device, 1, &fd->Fence);
        check_vk_result(err);
    }
    {
        err = vkResetCommandPool(g_GfxData.device, fd->CommandPool, 0);
        check_vk_result(err);
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
        check_vk_result(err);
    }
    ImPlatform_RunRenderPass(fd->CommandBuffer);   // Queued by ImPlatform_QueueRenderPass(): outside the frame's render pass.
    {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = wd->RenderPass;
        info.framebuffer = fd->Framebuffer;
        info.renderArea.extent.width = wd->Width;
        info.renderArea.extent.height = wd->Height;
        info.clearValueCount = 1;
        info.pClearValues = &wd->ClearValue;
        vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    g_FrameBegun = true;
    return true;
}

// ImPlatform API - GfxAPIRender
// Records ImGui draw data and submits the main viewport command buffer (upstream FrameRender, second half)
IMPLATFORM_API bool ImPlatform_GfxAPIRender(ImVec4 const vClearColor)
{
    (void)vClearColor;
    if (!g_FrameBegun)
        return false;

    ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
    ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];

    // Store command buffer for custom shader callbacks
    g_CurrentCommandBuffer = fd->CommandBuffer;

    // Record dear imgui primitives into command buffer
    const ImPlatform_ActivePass_Vulkan main_pass = { wd->RenderPass, fd->Framebuffer, fd->Backbuffer, wd->SurfaceFormat.format,
                                                     (uint32_t)wd->Width, (uint32_t)wd->Height, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR };
    g_ActivePasses.push_back(main_pass);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), fd->CommandBuffer);
    while (g_PushedTargetCount > 0)     // Targets the frame's callbacks did not pop: their passes end here
    {
        vkCmdEndRenderPass(fd->CommandBuffer);
        g_ActivePasses.pop_back();
        g_PushedTargetCount--;
        ImPlatform_Vulkan_ResumePass(fd->CommandBuffer);
    }
    g_ActivePasses.pop_back();
    g_CurrentRenderPass = VK_NULL_HANDLE;

    // Clear command buffer reference
    g_CurrentCommandBuffer = VK_NULL_HANDLE;

    // Submit command buffer
    vkCmdEndRenderPass(fd->CommandBuffer);
    {
        VkSemaphore image_acquired_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
        VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &image_acquired_semaphore;
        info.pWaitDstStageMask = &wait_stage;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &fd->CommandBuffer;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores = &render_complete_semaphore;

        VkResult err = vkEndCommandBuffer(fd->CommandBuffer);
        check_vk_result(err);
        err = vkQueueSubmit(g_GfxData.queue, 1, &info, fd->Fence);
        check_vk_result(err);
    }

    return true;
}

// ImPlatform API - GfxViewportPre
#ifdef IMGUI_HAS_VIEWPORT
IMPLATFORM_API void ImPlatform_GfxViewportPre(void)
{
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
    }
}

// ImPlatform API - GfxViewportPost
IMPLATFORM_API void ImPlatform_GfxViewportPost(void)
{
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::RenderPlatformWindowsDefault();
    }
}
#endif

// ImPlatform API - GfxAPISwapBuffer
// Presents the main viewport (upstream FramePresent)
IMPLATFORM_API bool ImPlatform_GfxAPISwapBuffer(void)
{
    if (!g_FrameBegun)
        return false;
    g_FrameBegun = false;

    if (g_SwapChainRebuild)
        return false;

    ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    VkPresentInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &render_complete_semaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &wd->Swapchain;
    info.pImageIndices = &wd->FrameIndex;
    VkResult err = vkQueuePresentKHR(g_GfxData.queue, &info);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
        g_SwapChainRebuild = true;
    if (err == VK_ERROR_OUT_OF_DATE_KHR)
        return false;
    if (err != VK_SUBOPTIMAL_KHR)
        check_vk_result(err);
    wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount; // Now we can use the next set of semaphores

    return true;
}

// ImPlatform API - ShutdownGfxAPI
IMPLATFORM_API void ImPlatform_ShutdownGfxAPI(void)
{
    vkDeviceWaitIdle(g_GfxData.device);
    ImPlatform_ReleaseRenderPasses();
}

// ImPlatform API - ShutdownWindow
IMPLATFORM_API void ImPlatform_ShutdownWindow(void)
{
    ImPlatform_DestroyTexture(g_WhiteTexture);
    g_WhiteTexture = 0;
    ImPlatform_Vulkan_ReleaseConstantRing();
    if (g_UploadCommandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(g_GfxData.device, g_UploadCommandPool, g_Allocator); // Also frees g_UploadCommandBuffer
        g_UploadCommandPool = VK_NULL_HANDLE;
        g_UploadCommandBuffer = VK_NULL_HANDLE;
    }
    if (g_ComputePipelineLayout) { vkDestroyPipelineLayout(g_GfxData.device, g_ComputePipelineLayout, g_Allocator); g_ComputePipelineLayout = VK_NULL_HANDLE; }
    if (g_ComputeSetLayout)      { vkDestroyDescriptorSetLayout(g_GfxData.device, g_ComputeSetLayout, g_Allocator); g_ComputeSetLayout = VK_NULL_HANDLE; }
    if (g_ComputeSampler)        { vkDestroySampler(g_GfxData.device, g_ComputeSampler, g_Allocator); g_ComputeSampler = VK_NULL_HANDLE; }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplVulkanH_DestroyWindow(g_GfxData.instance, g_GfxData.device, &g_MainWindowData, g_Allocator);
    // Since 1.92.6 ImGui_ImplVulkanH_DestroyWindow() doesn't destroy the surface: it is owned by the caller.
    vkDestroySurfaceKHR(g_GfxData.instance, g_MainWindowData.Surface, g_Allocator);
    g_MainWindowData.Surface = VK_NULL_HANDLE;
    g_GfxData.surface = VK_NULL_HANDLE;
    ImPlatform_Gfx_CleanupDevice_Vulkan(&g_GfxData);
}

// ============================================================================
// Texture Creation API - Vulkan Implementation
// ============================================================================

// Helper to get Vulkan format from ImPlatform format
static VkFormat ImPlatform_GetVulkanFormat(ImPlatform_PixelFormat format, int* out_bytes_per_pixel)
{
    switch (format)
    {
    case ImPlatform_PixelFormat_R8:
        *out_bytes_per_pixel = 1;
        return VK_FORMAT_R8_UNORM;
    case ImPlatform_PixelFormat_RG8:
        *out_bytes_per_pixel = 2;
        return VK_FORMAT_R8G8_UNORM;
    case ImPlatform_PixelFormat_RGB8:
        // Vulkan prefers RGBA8, RGB8 has limited support
        *out_bytes_per_pixel = 4;
        return VK_FORMAT_R8G8B8A8_UNORM;
    case ImPlatform_PixelFormat_RGBA8:
        *out_bytes_per_pixel = 4;
        return VK_FORMAT_R8G8B8A8_UNORM;
    case ImPlatform_PixelFormat_R16:
        *out_bytes_per_pixel = 2;
        return VK_FORMAT_R16_UNORM;
    case ImPlatform_PixelFormat_RG16:
        *out_bytes_per_pixel = 4;
        return VK_FORMAT_R16G16_UNORM;
    case ImPlatform_PixelFormat_RGBA16:
        *out_bytes_per_pixel = 8;
        return VK_FORMAT_R16G16B16A16_UNORM;
    case ImPlatform_PixelFormat_R32F:
        *out_bytes_per_pixel = 4;
        return VK_FORMAT_R32_SFLOAT;
    case ImPlatform_PixelFormat_RG32F:
        *out_bytes_per_pixel = 8;
        return VK_FORMAT_R32G32_SFLOAT;
    case ImPlatform_PixelFormat_RGBA32F:
        *out_bytes_per_pixel = 16;
        return VK_FORMAT_R32G32B32A32_SFLOAT;
#if IMPLATFORM_GFX_SUPPORT_BGRA_FORMATS
    case ImPlatform_PixelFormat_BGRA8:
        *out_bytes_per_pixel = 4;
        return VK_FORMAT_B8G8R8A8_UNORM;
#endif
#if IMPLATFORM_GFX_SUPPORT_HALF_FLOAT_FORMATS
    case ImPlatform_PixelFormat_R16F:
        *out_bytes_per_pixel = 2;
        return VK_FORMAT_R16_SFLOAT;
    case ImPlatform_PixelFormat_RG16F:
        *out_bytes_per_pixel = 4;
        return VK_FORMAT_R16G16_SFLOAT;
    case ImPlatform_PixelFormat_RGBA16F:
        *out_bytes_per_pixel = 8;
        return VK_FORMAT_R16G16B16A16_SFLOAT;
#endif
#if IMPLATFORM_GFX_SUPPORT_RGB_EXTENDED
    case ImPlatform_PixelFormat_RGB16:
        *out_bytes_per_pixel = 6;
        return VK_FORMAT_R16G16B16_UNORM;
    case ImPlatform_PixelFormat_RGB16F:
        *out_bytes_per_pixel = 6;
        return VK_FORMAT_R16G16B16_SFLOAT;
    case ImPlatform_PixelFormat_RGB32F:
        *out_bytes_per_pixel = 12;
        return VK_FORMAT_R32G32B32_SFLOAT;
#endif
#if IMPLATFORM_GFX_SUPPORT_SRGB_FORMATS
    case ImPlatform_PixelFormat_RGB8_SRGB:
        // VK_FORMAT_R8G8B8_SRGB has limited optimal-tiling support; use RGBA8_SRGB
        *out_bytes_per_pixel = 4;
        return VK_FORMAT_R8G8B8A8_SRGB;
    case ImPlatform_PixelFormat_RGBA8_SRGB:
        *out_bytes_per_pixel = 4;
        return VK_FORMAT_R8G8B8A8_SRGB;
#endif
#if IMPLATFORM_GFX_SUPPORT_PACKED_FORMATS
    case ImPlatform_PixelFormat_RGB10A2:
        *out_bytes_per_pixel = 4;
        return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
#endif
#if IMPLATFORM_GFX_SUPPORT_DEPTH_FORMATS
    case ImPlatform_PixelFormat_D16:
        *out_bytes_per_pixel = 2;
        return VK_FORMAT_D16_UNORM;
    case ImPlatform_PixelFormat_D32F:
        *out_bytes_per_pixel = 4;
        return VK_FORMAT_D32_SFLOAT;
    case ImPlatform_PixelFormat_D24S8:
        *out_bytes_per_pixel = 4;
        return VK_FORMAT_D24_UNORM_S8_UINT;
    case ImPlatform_PixelFormat_D32FS8:
        *out_bytes_per_pixel = 8;
        return VK_FORMAT_D32_SFLOAT_S8_UINT;
#endif
#if IMPLATFORM_GFX_SUPPORT_INTEGER_FORMATS
    case ImPlatform_PixelFormat_R8UI:
        *out_bytes_per_pixel = 1;
        return VK_FORMAT_R8_UINT;
    case ImPlatform_PixelFormat_R8I:
        *out_bytes_per_pixel = 1;
        return VK_FORMAT_R8_SINT;
    case ImPlatform_PixelFormat_R16UI:
        *out_bytes_per_pixel = 2;
        return VK_FORMAT_R16_UINT;
    case ImPlatform_PixelFormat_R16I:
        *out_bytes_per_pixel = 2;
        return VK_FORMAT_R16_SINT;
    case ImPlatform_PixelFormat_R32UI:
        *out_bytes_per_pixel = 4;
        return VK_FORMAT_R32_UINT;
    case ImPlatform_PixelFormat_R32I:
        *out_bytes_per_pixel = 4;
        return VK_FORMAT_R32_SINT;
#endif
    default:
        *out_bytes_per_pixel = 4;
        return VK_FORMAT_R8G8B8A8_UNORM;
    }
}

// Helper function to find suitable memory type
static uint32_t ImPlatform_FindMemoryType(VkMemoryPropertyFlags properties, uint32_t type_bits)
{
    VkPhysicalDeviceMemoryProperties prop;
    vkGetPhysicalDeviceMemoryProperties(g_GfxData.physicalDevice, &prop);
    for (uint32_t i = 0; i < prop.memoryTypeCount; i++)
    {
        if ((prop.memoryTypes[i].propertyFlags & properties) == properties && (type_bits & (1 << i)))
            return i;
    }
    return 0xFFFFFFFF; // Unable to find memoryType
}

IMPLATFORM_API ImPlatform_TextureDesc ImPlatform_TextureDesc_Default(unsigned int width, unsigned int height)
{
    ImPlatform_TextureDesc desc;
    desc.width = width;
    desc.height = height;
    desc.format = ImPlatform_PixelFormat_RGBA8;
    desc.min_filter = ImPlatform_TextureFilter_Linear;
    desc.mag_filter = ImPlatform_TextureFilter_Linear;
    desc.wrap_u = ImPlatform_TextureWrap_Clamp;
    desc.wrap_v = ImPlatform_TextureWrap_Clamp;
    return desc;
}

// Texture3D stub for Vulkan.
IMPLATFORM_API ImPlatform_TextureDesc3D ImPlatform_TextureDesc3D_Default(unsigned int w, unsigned int h, unsigned int d)
{
    ImPlatform_TextureDesc3D x; x.width = w; x.height = h; x.depth = d;
    x.format = ImPlatform_PixelFormat_RGBA8;
    x.min_filter = ImPlatform_TextureFilter_Linear; x.mag_filter = ImPlatform_TextureFilter_Linear;
    x.wrap_u = x.wrap_v = x.wrap_w = ImPlatform_TextureWrap_Clamp;
    return x;
}
IMPLATFORM_API bool ImPlatform_SupportsTexture3D(void) { return false; }
IMPLATFORM_API ImTextureID ImPlatform_CreateTexture3D(const void*, const ImPlatform_TextureDesc3D*) { return (ImTextureID)0; }

IMPLATFORM_API ImTextureID ImPlatform_CreateTexture(const void* pixel_data, const ImPlatform_TextureDesc* desc)
{
    if (!desc || !pixel_data || !g_GfxData.device)
        return (ImTextureID)0;

    int bytes_per_pixel;
    VkFormat format = ImPlatform_GetVulkanFormat(desc->format, &bytes_per_pixel);
    VkDeviceSize upload_size = desc->width * desc->height * bytes_per_pixel;

    VkResult err;

    // Create staging buffer
    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
    {
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = upload_size;
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        err = vkCreateBuffer(g_GfxData.device, &buffer_info, g_Allocator, &staging_buffer);
        if (err != VK_SUCCESS)
            return (ImTextureID)0;

        VkMemoryRequirements mem_req;
        vkGetBufferMemoryRequirements(g_GfxData.device, staging_buffer, &mem_req);

        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_req.size;
        alloc_info.memoryTypeIndex = ImPlatform_FindMemoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, mem_req.memoryTypeBits);
        err = vkAllocateMemory(g_GfxData.device, &alloc_info, g_Allocator, &staging_memory);
        if (err != VK_SUCCESS)
        {
            vkDestroyBuffer(g_GfxData.device, staging_buffer, g_Allocator);
            return (ImTextureID)0;
        }

        err = vkBindBufferMemory(g_GfxData.device, staging_buffer, staging_memory, 0);
        if (err != VK_SUCCESS)
        {
            vkDestroyBuffer(g_GfxData.device, staging_buffer, g_Allocator);
            vkFreeMemory(g_GfxData.device, staging_memory, g_Allocator);
            return (ImTextureID)0;
        }
    }

    // Upload to staging buffer
    {
        void* map = NULL;
        err = vkMapMemory(g_GfxData.device, staging_memory, 0, upload_size, 0, &map);
        if (err != VK_SUCCESS)
        {
            vkDestroyBuffer(g_GfxData.device, staging_buffer, g_Allocator);
            vkFreeMemory(g_GfxData.device, staging_memory, g_Allocator);
            return (ImTextureID)0;
        }
        memcpy(map, pixel_data, upload_size);
        VkMappedMemoryRange range[1] = {};
        range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range[0].memory = staging_memory;
        range[0].size = upload_size;
        err = vkFlushMappedMemoryRanges(g_GfxData.device, 1, range);
        vkUnmapMemory(g_GfxData.device, staging_memory);
        if (err != VK_SUCCESS)
        {
            vkDestroyBuffer(g_GfxData.device, staging_buffer, g_Allocator);
            vkFreeMemory(g_GfxData.device, staging_memory, g_Allocator);
            return (ImTextureID)0;
        }
    }

    // Create the Image
    VkImage image;
    VkDeviceMemory image_memory;
    {
        VkImageCreateInfo image_info = {};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.format = format;
        image_info.extent.width = desc->width;
        image_info.extent.height = desc->height;
        image_info.extent.depth = 1;
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        err = vkCreateImage(g_GfxData.device, &image_info, g_Allocator, &image);
        if (err != VK_SUCCESS)
        {
            vkDestroyBuffer(g_GfxData.device, staging_buffer, g_Allocator);
            vkFreeMemory(g_GfxData.device, staging_memory, g_Allocator);
            return (ImTextureID)0;
        }

        VkMemoryRequirements mem_req;
        vkGetImageMemoryRequirements(g_GfxData.device, image, &mem_req);

        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_req.size;
        alloc_info.memoryTypeIndex = ImPlatform_FindMemoryType(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mem_req.memoryTypeBits);
        err = vkAllocateMemory(g_GfxData.device, &alloc_info, g_Allocator, &image_memory);
        if (err != VK_SUCCESS)
        {
            vkDestroyImage(g_GfxData.device, image, g_Allocator);
            vkDestroyBuffer(g_GfxData.device, staging_buffer, g_Allocator);
            vkFreeMemory(g_GfxData.device, staging_memory, g_Allocator);
            return (ImTextureID)0;
        }

        err = vkBindImageMemory(g_GfxData.device, image, image_memory, 0);
        if (err != VK_SUCCESS)
        {
            vkFreeMemory(g_GfxData.device, image_memory, g_Allocator);
            vkDestroyImage(g_GfxData.device, image, g_Allocator);
            vkDestroyBuffer(g_GfxData.device, staging_buffer, g_Allocator);
            vkFreeMemory(g_GfxData.device, staging_memory, g_Allocator);
            return (ImTextureID)0;
        }
    }

    // Create the Image View
    VkImageView image_view;
    {
        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = format;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.layerCount = 1;
        err = vkCreateImageView(g_GfxData.device, &view_info, g_Allocator, &image_view);
        if (err != VK_SUCCESS)
        {
            vkFreeMemory(g_GfxData.device, image_memory, g_Allocator);
            vkDestroyImage(g_GfxData.device, image, g_Allocator);
            vkDestroyBuffer(g_GfxData.device, staging_buffer, g_Allocator);
            vkFreeMemory(g_GfxData.device, staging_memory, g_Allocator);
            return (ImTextureID)0;
        }
    }

    // Create descriptor set
    // Since 1.92.8 textures are registered as VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: the sampler is owned by the ImGui backend
    // (linear by default, switch with ImPlatform_PushSampler()), so desc->min_filter/mag_filter/wrap_u/wrap_v are not baked in.
    VkDescriptorSet descriptor_set = ImGui_ImplVulkan_AddTexture(image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (descriptor_set == VK_NULL_HANDLE)
    {
        vkDestroyImageView(g_GfxData.device, image_view, g_Allocator);
        vkFreeMemory(g_GfxData.device, image_memory, g_Allocator);
        vkDestroyImage(g_GfxData.device, image, g_Allocator);
        vkDestroyBuffer(g_GfxData.device, staging_buffer, g_Allocator);
        vkFreeMemory(g_GfxData.device, staging_memory, g_Allocator);
        return (ImTextureID)0;
    }

    // Upload to GPU using a dedicated command buffer: the frame command buffers may be recording
    {
        if (g_UploadCommandPool == VK_NULL_HANDLE)
        {
            VkCommandPoolCreateInfo pool_info = {};
            pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            pool_info.queueFamilyIndex = g_QueueFamily;
            err = vkCreateCommandPool(g_GfxData.device, &pool_info, g_Allocator, &g_UploadCommandPool);
            check_vk_result(err);

            VkCommandBufferAllocateInfo cb_info = {};
            cb_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            cb_info.commandPool = g_UploadCommandPool;
            cb_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cb_info.commandBufferCount = 1;
            err = vkAllocateCommandBuffers(g_GfxData.device, &cb_info, &g_UploadCommandBuffer);
            check_vk_result(err);
        }
        VkCommandBuffer command_buffer = g_UploadCommandBuffer;
        vkResetCommandBuffer(command_buffer, 0);

        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(command_buffer, &begin_info);
        if (err != VK_SUCCESS)
        {
            // Cleanup and return null
            ImGui_ImplVulkan_RemoveTexture(descriptor_set);
            vkDestroyImageView(g_GfxData.device, image_view, g_Allocator);
            vkFreeMemory(g_GfxData.device, image_memory, g_Allocator);
            vkDestroyImage(g_GfxData.device, image, g_Allocator);
            vkDestroyBuffer(g_GfxData.device, staging_buffer, g_Allocator);
            vkFreeMemory(g_GfxData.device, staging_memory, g_Allocator);
            return (ImTextureID)0;
        }

        // Transition to transfer dst
        {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.layerCount = 1;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
        }

        // Copy buffer to image
        {
            VkBufferImageCopy region = {};
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.layerCount = 1;
            region.imageExtent.width = desc->width;
            region.imageExtent.height = desc->height;
            region.imageExtent.depth = 1;
            vkCmdCopyBufferToImage(command_buffer, staging_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        }

        // Transition to shader read
        {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.layerCount = 1;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
        }

        vkEndCommandBuffer(command_buffer);

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffer;
        err = vkQueueSubmit(g_GfxData.queue, 1, &submit_info, VK_NULL_HANDLE);
        vkQueueWaitIdle(g_GfxData.queue); // Wait for upload to complete
    }

    // Cleanup staging resources
    vkDestroyBuffer(g_GfxData.device, staging_buffer, g_Allocator);
    vkFreeMemory(g_GfxData.device, staging_memory, g_Allocator);

    ImPlatform_Vulkan_RegisterTexture(descriptor_set, image, image_view, image_memory, format, desc->width, desc->height, bytes_per_pixel,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false);

    return (ImTextureID)descriptor_set;
}

IMPLATFORM_API bool ImPlatform_UpdateTexture(ImTextureID /*texture_id*/, const void* /*pixel_data*/,
                                              unsigned int /*x*/, unsigned int /*y*/,
                                              unsigned int /*width*/, unsigned int /*height*/)
{
    // Vulkan texture updates require staging buffers and command lists
    // This is complex and not implemented yet
    return false;
}

// The external dependency of render texture passes: identical to Dear ImGui's window render pass, so that the passes
// are compatible with it and Dear ImGui's pipeline draws in them (render pass compatibility compares dependencies).
// ImPlatform_RunRenderPass records the rest of the synchronization as a barrier before each pass.
static VkSubpassDependency ImPlatform_PassDependency()
{
    VkSubpassDependency dep = {};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    return dep;
}

static VkCommandBuffer ImPlatform_Vulkan_BeginOneShot();
static bool ImPlatform_Vulkan_EndOneShot(VkCommandBuffer cb);
static void ImPlatform_Vulkan_ImageBarrier(VkCommandBuffer cb, VkImage image, VkImageLayout from, VkImageLayout to,
                                           VkPipelineStageFlags dst_stage, VkAccessFlags dst_access);

IMPLATFORM_API ImTextureID ImPlatform_CreateRenderTexture(const ImPlatform_TextureDesc* desc)
{
    if (!desc || !g_GfxData.device)
        return (ImTextureID)0;

    int bytes_per_pixel;
    VkFormat format = ImPlatform_GetVulkanFormat(desc->format, &bytes_per_pixel);
    VkResult err;

    // Create image
    VkImage image;
    VkDeviceMemory imageMemory;
    {
        VkImageCreateInfo image_info = {};
        image_info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType     = VK_IMAGE_TYPE_2D;
        image_info.format        = format;
        image_info.extent        = { desc->width, desc->height, 1 };
        image_info.mipLevels     = 1;
        image_info.arrayLayers   = 1;
        image_info.samples       = VK_SAMPLE_COUNT_1_BIT;
        image_info.tiling        = VK_IMAGE_TILING_OPTIMAL;
        image_info.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        image_info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        err = vkCreateImage(g_GfxData.device, &image_info, g_Allocator, &image);
        if (err != VK_SUCCESS) return (ImTextureID)0;

        VkMemoryRequirements mem_req;
        vkGetImageMemoryRequirements(g_GfxData.device, image, &mem_req);
        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize  = mem_req.size;
        alloc_info.memoryTypeIndex = ImPlatform_FindMemoryType(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mem_req.memoryTypeBits);
        err = vkAllocateMemory(g_GfxData.device, &alloc_info, g_Allocator, &imageMemory);
        if (err != VK_SUCCESS) { vkDestroyImage(g_GfxData.device, image, g_Allocator); return (ImTextureID)0; }
        vkBindImageMemory(g_GfxData.device, image, imageMemory, 0);
    }

    // Create image view
    VkImageView imageView;
    {
        VkImageViewCreateInfo view_info = {};
        view_info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image                           = image;
        view_info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format                          = format;
        view_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.levelCount     = 1;
        view_info.subresourceRange.layerCount     = 1;
        err = vkCreateImageView(g_GfxData.device, &view_info, g_Allocator, &imageView);
        if (err != VK_SUCCESS) {
            vkFreeMemory(g_GfxData.device, imageMemory, g_Allocator);
            vkDestroyImage(g_GfxData.device, image, g_Allocator);
            return (ImTextureID)0;
        }
    }

    // Create render pass
    VkRenderPass renderPass;
    {
        VkAttachmentDescription attachment = {};
        attachment.format         = format;
        attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference color_ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments    = &color_ref;

        const VkSubpassDependency dep = ImPlatform_PassDependency();

        VkRenderPassCreateInfo rp_info = {};
        rp_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rp_info.attachmentCount = 1;
        rp_info.pAttachments    = &attachment;
        rp_info.subpassCount    = 1;
        rp_info.pSubpasses      = &subpass;
        rp_info.dependencyCount = 1;
        rp_info.pDependencies   = &dep;
        err = vkCreateRenderPass(g_GfxData.device, &rp_info, g_Allocator, &renderPass);
        if (err != VK_SUCCESS) {
            vkDestroyImageView(g_GfxData.device, imageView, g_Allocator);
            vkFreeMemory(g_GfxData.device, imageMemory, g_Allocator);
            vkDestroyImage(g_GfxData.device, image, g_Allocator);
            return (ImTextureID)0;
        }
    }

    // Create framebuffer
    VkFramebuffer framebuffer;
    {
        VkFramebufferCreateInfo fb_info = {};
        fb_info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_info.renderPass      = renderPass;
        fb_info.attachmentCount = 1;
        fb_info.pAttachments    = &imageView;
        fb_info.width           = desc->width;
        fb_info.height          = desc->height;
        fb_info.layers          = 1;
        err = vkCreateFramebuffer(g_GfxData.device, &fb_info, g_Allocator, &framebuffer);
        if (err != VK_SUCCESS) {
            vkDestroyRenderPass(g_GfxData.device, renderPass, g_Allocator);
            vkDestroyImageView(g_GfxData.device, imageView, g_Allocator);
            vkFreeMemory(g_GfxData.device, imageMemory, g_Allocator);
            vkDestroyImage(g_GfxData.device, image, g_Allocator);
            return (ImTextureID)0;
        }
    }

    // Create dedicated command pool + command buffer for standalone rendering
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    {
        VkCommandPoolCreateInfo pool_info = {};
        pool_info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.queueFamilyIndex = g_QueueFamily;
        pool_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        err = vkCreateCommandPool(g_GfxData.device, &pool_info, g_Allocator, &commandPool);
        if (err != VK_SUCCESS) {
            vkDestroyFramebuffer(g_GfxData.device, framebuffer, g_Allocator);
            vkDestroyRenderPass(g_GfxData.device, renderPass, g_Allocator);
            vkDestroyImageView(g_GfxData.device, imageView, g_Allocator);
            vkFreeMemory(g_GfxData.device, imageMemory, g_Allocator);
            vkDestroyImage(g_GfxData.device, image, g_Allocator);
            return (ImTextureID)0;
        }

        VkCommandBufferAllocateInfo alloc_info = {};
        alloc_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool        = commandPool;
        alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;
        vkAllocateCommandBuffers(g_GfxData.device, &alloc_info, &commandBuffer);
    }

    // Register with ImGui for use as a shader input (sampled image, the sampler is owned by the ImGui backend)
    VkDescriptorSet descriptorSet = ImGui_ImplVulkan_AddTexture(imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    ImPlatform_RTTracking_Vulkan* entry = new ImPlatform_RTTracking_Vulkan();
    entry->image         = image;
    entry->imageMemory   = imageMemory;
    entry->imageView     = imageView;
    entry->descriptorSet = descriptorSet;
    entry->renderPass    = renderPass;
    entry->framebuffer   = framebuffer;
    entry->commandPool   = commandPool;
    entry->commandBuffer = commandBuffer;
    entry->width         = desc->width;
    entry->height        = desc->height;
    entry->format        = format;
    entry->next          = g_RTTrackingHead;
    g_RTTrackingHead     = entry;
    // In its resting layout from the start: copies into it (ImPlatform_CopyCurrentTarget) and readbacks expect it.
    if (VkCommandBuffer cb = ImPlatform_Vulkan_BeginOneShot())
    {
        ImPlatform_Vulkan_ImageBarrier(cb, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
        ImPlatform_Vulkan_EndOneShot(cb);
    }
    ImPlatform_Vulkan_RegisterTexture(descriptorSet, image, imageView, VK_NULL_HANDLE, format, desc->width, desc->height, bytes_per_pixel,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false);

    return (ImTextureID)descriptorSet;
}

// Render passes: recorded on the frame's command buffer before its render pass. Targets have the swapchain's format
// (render pass compatibility with the ImGui pipeline); the ImGui backend's buffers turn per render (ImageCount doubled
// at init), so a pass never reuses buffers the GPU may still read.
static ImTextureID  g_PassTarget = 0;
static ImDrawData*  g_PassDrawData = NULL;
static bool         g_PassClear = false;
struct ImPlatform_LoadPass_Vulkan { VkFormat format; VkImageLayout layout; VkRenderPass pass; };
static ImVector<ImPlatform_LoadPass_Vulkan> g_LoadPasses;      // See ImPlatform_PassLoadRenderPass.

static ImPlatform_RTTracking_Vulkan* ImPlatform_FindRT_Vulkan(ImTextureID texture)
{
    ImPlatform_RTTracking_Vulkan* entry = g_RTTrackingHead;
    while (entry && entry->descriptorSet != (VkDescriptorSet)texture)
        entry = entry->next;
    return entry;
}

IMPLATFORM_API ImPlatform_PixelFormat ImPlatform_GetRenderPassFormat(void)
{
    return g_MainWindowData.SurfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM ? ImPlatform_PixelFormat_BGRA8 : ImPlatform_PixelFormat_RGBA8;
}

IMPLATFORM_API bool ImPlatform_QueueRenderPass(ImTextureID target, ImDrawData* draw_data, bool clear)
{
    ImPlatform_RTTracking_Vulkan* entry = ImPlatform_FindRT_Vulkan(target);
    if (!entry || !draw_data || entry->format != g_MainWindowData.SurfaceFormat.format)
        return false;
    g_PassTarget = target;
    g_PassDrawData = draw_data;
    g_PassClear = clear;
    return true;
}

// Render passes that keep a target's content (LOAD), compatible with the passes that drew it (same format): one per format
// and layout the target rests in between passes (shader reads for render textures, presentation for the swapchain).
static VkRenderPass ImPlatform_PassLoadRenderPass(VkFormat format, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
{
    for (const ImPlatform_LoadPass_Vulkan& p : g_LoadPasses)
        if (p.format == format && p.layout == layout)
            return p.pass;
    VkAttachmentDescription attachment = {};
    attachment.format         = format;
    attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout  = layout;
    attachment.finalLayout    = layout;
    VkAttachmentReference color_ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &color_ref;
    const VkSubpassDependency dep = ImPlatform_PassDependency();
    VkRenderPassCreateInfo rp_info = {};
    rp_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.attachmentCount = 1;
    rp_info.pAttachments    = &attachment;
    rp_info.subpassCount    = 1;
    rp_info.pSubpasses      = &subpass;
    rp_info.dependencyCount = 1;
    rp_info.pDependencies   = &dep;
    ImPlatform_LoadPass_Vulkan p = { format, layout, VK_NULL_HANDLE };
    if (vkCreateRenderPass(g_GfxData.device, &rp_info, g_Allocator, &p.pass) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    g_LoadPasses.push_back(p);
    return p.pass;
}

static void ImPlatform_RunRenderPass(VkCommandBuffer cmd)
{
    ImDrawData* draw_data = g_PassDrawData;
    g_PassDrawData = NULL;
    ImPlatform_RTTracking_Vulkan* entry = draw_data ? ImPlatform_FindRT_Vulkan(g_PassTarget) : NULL;
    if (!entry)     // Destroyed since it was queued.
        return;
    const bool clear = g_PassClear || !entry->rendered;    // Undefined content: cleared the first time.
    VkRenderPass render_pass = clear ? entry->renderPass : ImPlatform_PassLoadRenderPass(entry->format);
    if (render_pass == VK_NULL_HANDLE)
        return;
    if (draw_data->OwnerViewport == NULL)
        draw_data->OwnerViewport = ImGui::GetMainViewport();     // The backend keeps its buffers per viewport.
    if (draw_data->Textures == NULL)
        draw_data->Textures = &ImGui::GetPlatformIO().Textures;     // The frame's texture updates, before they are drawn.
    VkClearValue transparent = {};
    VkRenderPassBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = render_pass;
    info.framebuffer = entry->framebuffer;
    info.renderArea.extent.width = entry->width;
    info.renderArea.extent.height = entry->height;
    info.clearValueCount = clear ? 1 : 0;
    info.pClearValues = &transparent;
    // The pass's dependency only orders it after earlier color output (see ImPlatform_PassDependency): earlier
    // sampling of the target, and earlier passes' writes that a LOAD reads or blends with, are ordered here.
    VkMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    vkCmdBeginRenderPass(cmd, &info, VK_SUBPASS_CONTENTS_INLINE);
    ImDrawData* saved_draw_data = g_CurrentDrawData;
    g_CurrentDrawData = draw_data;                      // Custom shaders project with it.
    g_CurrentCommandBuffer = cmd;
    g_CurrentRenderPass = render_pass;
    const ImPlatform_ActivePass_Vulkan active = { render_pass, entry->framebuffer, entry->image, entry->format, entry->width, entry->height,
                                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    g_ActivePasses.push_back(active);
    ImGui_ImplVulkan_RenderDrawData(draw_data, cmd);   // Its viewport and projection come from the draw data.
    g_ActivePasses.pop_back();
    g_CurrentRenderPass = VK_NULL_HANDLE;
    g_CurrentCommandBuffer = VK_NULL_HANDLE;
    g_CurrentDrawData = saved_draw_data;
    vkCmdEndRenderPass(cmd);
    entry->rendered = true;
}

// Premultiplied alpha: blending is part of the pipelines, so a custom program un-premultiplies the texels instead,
// and the straight alpha blending composites them as premultiplied (texel times the premultiplied vertex color).
// SPIR-V of this HLSL, compiled as every custom shader (see "Custom Shader System - Vulkan"), -O3:
//   cbuffer vertexBuffer : register(b0) { float4x4 ProjMtx; };
//   Texture2D texture0 : register(t0);
//   SamplerState sampler0 : register(s0);
//   struct VS_INPUT { float2 pos : POSITION; float4 col : COLOR0; float2 uv : TEXCOORD0; };
//   struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR0; float2 uv : TEXCOORD0; };
//   PS_INPUT vs_main(VS_INPUT input) { PS_INPUT o; o.pos = mul(ProjMtx, float4(input.pos.xy, 0.0, 1.0)); o.col = input.col; o.uv = input.uv; return o; }
//   float4 ps_main(PS_INPUT input) : SV_Target
//   {
//       float4 c = texture0.Sample(sampler0, input.uv) * input.col;     // Texel and vertex color premultiplied.
//       return float4(c.a > 0.0 ? c.rgb / c.a : float3(0.0, 0.0, 0.0), c.a);
//   }
static const unsigned int g_UnpremultiplyVert[] = {
    0x07230203, 0x00010300, 0x00280000, 0x00000023, 0x00000000, 0x00020011, 0x00000001, 0x0003000e,
    0x00000000, 0x00000001, 0x000b000f, 0x00000000, 0x00000001, 0x6e69616d, 0x00000000, 0x00000002,
    0x00000003, 0x00000004, 0x00000005, 0x00000006, 0x00000007, 0x00030003, 0x0000000b, 0x00000001,
    0x000d0005, 0x00000008, 0x4e414c53, 0x61505f47, 0x656d6172, 0x47726574, 0x70756f72, 0x7265765f,
    0x42786574, 0x65666675, 0x74735f72, 0x30343164, 0x00000000, 0x00050006, 0x00000008, 0x00000000,
    0x6a6f7250, 0x0078744d, 0x00060005, 0x00000009, 0x74726576, 0x75427865, 0x72656666, 0x00000000,
    0x00050005, 0x00000005, 0x75706e69, 0x6f702e74, 0x00000073, 0x00050005, 0x00000006, 0x75706e69,
    0x6f632e74, 0x0000006c, 0x00050005, 0x00000007, 0x75706e69, 0x76752e74, 0x00000000, 0x00090005,
    0x00000003, 0x72746e65, 0x696f5079, 0x6150746e, 0x5f6d6172, 0x6d5f7376, 0x2e6e6961, 0x006c6f63,
    0x00090005, 0x00000004, 0x72746e65, 0x696f5079, 0x6150746e, 0x5f6d6172, 0x6d5f7376, 0x2e6e6961,
    0x00007675, 0x00040005, 0x00000001, 0x6d5f7376, 0x006e6961, 0x00030047, 0x00000008, 0x00000002,
    0x00050048, 0x00000008, 0x00000000, 0x00000023, 0x00000000, 0x00040048, 0x00000008, 0x00000000,
    0x00000004, 0x00050048, 0x00000008, 0x00000000, 0x00000007, 0x00000010, 0x00040047, 0x00000009,
    0x00000021, 0x00000020, 0x00040047, 0x00000009, 0x00000022, 0x00000000, 0x00040047, 0x00000005,
    0x0000001e, 0x00000000, 0x00040047, 0x00000006, 0x0000001e, 0x00000001, 0x00040047, 0x00000007,
    0x0000001e, 0x00000002, 0x00040047, 0x00000002, 0x0000000b, 0x00000000, 0x00040047, 0x00000003,
    0x0000001e, 0x00000000, 0x00040047, 0x00000004, 0x0000001e, 0x00000001, 0x00020013, 0x0000000a,
    0x00030021, 0x0000000b, 0x0000000a, 0x00030016, 0x0000000c, 0x00000020, 0x00040017, 0x0000000d,
    0x0000000c, 0x00000004, 0x00040017, 0x0000000e, 0x0000000c, 0x00000002, 0x00040015, 0x0000000f,
    0x00000020, 0x00000001, 0x0004002b, 0x0000000f, 0x00000010, 0x00000000, 0x00040018, 0x00000011,
    0x0000000d, 0x00000004, 0x0003001e, 0x00000008, 0x00000011, 0x00040020, 0x00000012, 0x00000002,
    0x00000008, 0x00040020, 0x00000013, 0x00000002, 0x00000011, 0x00040020, 0x00000014, 0x00000001,
    0x0000000e, 0x0004002b, 0x0000000c, 0x00000015, 0x00000000, 0x0004002b, 0x0000000c, 0x00000016,
    0x3f800000, 0x00040020, 0x00000017, 0x00000001, 0x0000000d, 0x00040020, 0x00000018, 0x00000003,
    0x0000000d, 0x00040020, 0x00000019, 0x00000003, 0x0000000e, 0x0004003b, 0x00000012, 0x00000009,
    0x00000002, 0x0004003b, 0x00000014, 0x00000005, 0x00000001, 0x0004003b, 0x00000017, 0x00000006,
    0x00000001, 0x0004003b, 0x00000014, 0x00000007, 0x00000001, 0x0004003b, 0x00000018, 0x00000002,
    0x00000003, 0x0004003b, 0x00000018, 0x00000003, 0x00000003, 0x0004003b, 0x00000019, 0x00000004,
    0x00000003, 0x00050036, 0x0000000a, 0x00000001, 0x00000000, 0x0000000b, 0x000200f8, 0x0000001a,
    0x00050041, 0x00000013, 0x0000001b, 0x00000009, 0x00000010, 0x0004003d, 0x00000011, 0x0000001c,
    0x0000001b, 0x0004003d, 0x0000000e, 0x0000001d, 0x00000005, 0x0007004f, 0x0000000e, 0x0000001e,
    0x0000001d, 0x0000001d, 0x00000000, 0x00000001, 0x00060050, 0x0000000d, 0x0000001f, 0x0000001e,
    0x00000015, 0x00000016, 0x00050090, 0x0000000d, 0x00000020, 0x0000001f, 0x0000001c, 0x0004003d,
    0x0000000d, 0x00000021, 0x00000006, 0x0004003d, 0x0000000e, 0x00000022, 0x00000007, 0x0003003e,
    0x00000002, 0x00000020, 0x0003003e, 0x00000003, 0x00000021, 0x0003003e, 0x00000004, 0x00000022,
    0x000100fd, 0x00010038
};
static const unsigned int g_UnpremultiplyFrag[] = {
    0x07230203, 0x00010300, 0x00280000, 0x0000002a, 0x00000000, 0x00020011, 0x00000001, 0x0003000e,
    0x00000000, 0x00000001, 0x0008000f, 0x00000004, 0x00000001, 0x6e69616d, 0x00000000, 0x00000002,
    0x00000003, 0x00000004, 0x00030010, 0x00000001, 0x00000007, 0x00030003, 0x0000000b, 0x00000001,
    0x00050005, 0x00000004, 0x75706e69, 0x76752e74, 0x00000000, 0x00050005, 0x00000005, 0x74786574,
    0x30657275, 0x00000000, 0x00050005, 0x00000006, 0x706d6173, 0x3072656c, 0x00000000, 0x00050005,
    0x00000007, 0x61735f5f, 0x656c706d, 0x00000064, 0x00050005, 0x00000003, 0x75706e69, 0x6f632e74,
    0x0000006c, 0x00030005, 0x00000008, 0x00000063, 0x00080005, 0x00000002, 0x72746e65, 0x696f5079,
    0x6150746e, 0x5f6d6172, 0x6d5f7370, 0x006e6961, 0x00040005, 0x00000001, 0x6d5f7370, 0x006e6961,
    0x00040047, 0x00000004, 0x0000001e, 0x00000001, 0x00040047, 0x00000005, 0x00000021, 0x00000000,
    0x00040047, 0x00000005, 0x00000022, 0x00000000, 0x00040047, 0x00000006, 0x00000021, 0x00000010,
    0x00040047, 0x00000006, 0x00000022, 0x00000000, 0x00040047, 0x00000003, 0x0000001e, 0x00000000,
    0x00040047, 0x00000002, 0x0000001e, 0x00000000, 0x00020013, 0x00000009, 0x00030021, 0x0000000a,
    0x00000009, 0x00030016, 0x0000000b, 0x00000020, 0x00040017, 0x0000000c, 0x0000000b, 0x00000003,
    0x00040017, 0x0000000d, 0x0000000b, 0x00000002, 0x00040020, 0x0000000e, 0x00000001, 0x0000000d,
    0x00090019, 0x0000000f, 0x0000000b, 0x00000001, 0x00000002, 0x00000000, 0x00000000, 0x00000001,
    0x00000000, 0x00040020, 0x00000010, 0x00000000, 0x0000000f, 0x0002001a, 0x00000011, 0x00040020,
    0x00000012, 0x00000000, 0x00000011, 0x0003001b, 0x00000013, 0x0000000f, 0x00040017, 0x00000014,
    0x0000000b, 0x00000004, 0x00040020, 0x00000015, 0x00000001, 0x00000014, 0x00020014, 0x00000016,
    0x0004002b, 0x0000000b, 0x00000017, 0x00000000, 0x0006002c, 0x0000000c, 0x00000018, 0x00000017,
    0x00000017, 0x00000017, 0x00040020, 0x00000019, 0x00000003, 0x00000014, 0x0004003b, 0x0000000e,
    0x00000004, 0x00000001, 0x0004003b, 0x00000010, 0x00000005, 0x00000000, 0x0004003b, 0x00000012,
    0x00000006, 0x00000000, 0x0004003b, 0x00000015, 0x00000003, 0x00000001, 0x0004003b, 0x00000019,
    0x00000002, 0x00000003, 0x00050036, 0x00000009, 0x00000001, 0x00000000, 0x0000000a, 0x000200f8,
    0x0000001a, 0x0004003d, 0x0000000d, 0x0000001b, 0x00000004, 0x0004003d, 0x0000000f, 0x0000001c,
    0x00000005, 0x0004003d, 0x00000011, 0x0000001d, 0x00000006, 0x00050056, 0x00000013, 0x0000001e,
    0x0000001c, 0x0000001d, 0x00060057, 0x00000014, 0x00000007, 0x0000001e, 0x0000001b, 0x00000000,
    0x0004003d, 0x00000014, 0x0000001f, 0x00000003, 0x00050085, 0x00000014, 0x00000008, 0x00000007,
    0x0000001f, 0x00050051, 0x0000000b, 0x00000020, 0x00000008, 0x00000003, 0x000500ba, 0x00000016,
    0x00000021, 0x00000020, 0x00000017, 0x000300f7, 0x00000022, 0x00000000, 0x000400fa, 0x00000021,
    0x00000023, 0x00000024, 0x000200f8, 0x00000023, 0x0008004f, 0x0000000c, 0x00000025, 0x00000008,
    0x00000008, 0x00000000, 0x00000001, 0x00000002, 0x00060050, 0x0000000c, 0x00000026, 0x00000020,
    0x00000020, 0x00000020, 0x00050088, 0x0000000c, 0x00000027, 0x00000025, 0x00000026, 0x000200f9,
    0x00000022, 0x000200f8, 0x00000024, 0x000200f9, 0x00000022, 0x000200f8, 0x00000022, 0x000700f5,
    0x0000000c, 0x00000028, 0x00000027, 0x00000023, 0x00000018, 0x00000024, 0x00050050, 0x00000014,
    0x00000029, 0x00000028, 0x00000020, 0x0003003e, 0x00000002, 0x00000029, 0x000100fd, 0x00010038
};
static ImPlatform_ShaderProgram g_UnpremultiplyProgram = NULL;
static ImPlatform_Shader        g_UnpremultiplyShaders[2] = {};

static ImPlatform_ShaderProgram ImPlatform_UnpremultiplyProgram()
{
    if (g_UnpremultiplyProgram)
        return g_UnpremultiplyProgram;
    ImPlatform_ShaderDesc vs = {}, ps = {};
    vs.stage = ImPlatform_ShaderStage_Vertex;
    ps.stage = ImPlatform_ShaderStage_Fragment;
    vs.format = ps.format = ImPlatform_ShaderFormat_SPIRV;
    vs.entry_point = ps.entry_point = "main";
    vs.bytecode = g_UnpremultiplyVert;
    vs.bytecode_size = sizeof(g_UnpremultiplyVert);
    ps.bytecode = g_UnpremultiplyFrag;
    ps.bytecode_size = sizeof(g_UnpremultiplyFrag);
    ImPlatform_Shader v = g_UnpremultiplyShaders[0] = ImPlatform_CreateShader(&vs);
    ImPlatform_Shader f = g_UnpremultiplyShaders[1] = ImPlatform_CreateShader(&ps);
    if (v && f)
        g_UnpremultiplyProgram = ImPlatform_CreateShaderProgram(v, f);
    return g_UnpremultiplyProgram;
}

IMPLATFORM_API void ImPlatform_PushPremultipliedAlpha(ImDrawList* draw)
{
    if (ImPlatform_ShaderProgram program = ImPlatform_UnpremultiplyProgram())
        ImPlatform_BeginCustomShader(draw ? draw : ImGui::GetWindowDrawList(), program);
}

IMPLATFORM_API void ImPlatform_PopPremultipliedAlpha(ImDrawList* draw)
{
    if (g_UnpremultiplyProgram)
        ImPlatform_EndCustomShader(draw ? draw : ImGui::GetWindowDrawList());
}

static void ImPlatform_ReleaseRenderPasses()
{
    for (const ImPlatform_LoadPass_Vulkan& p : g_LoadPasses)
        vkDestroyRenderPass(g_GfxData.device, p.pass, g_Allocator);
    g_LoadPasses.clear();
    if (g_UnpremultiplyProgram)
        ImPlatform_DestroyShaderProgram(g_UnpremultiplyProgram);
    g_UnpremultiplyProgram = NULL;
    for (ImPlatform_Shader& shader : g_UnpremultiplyShaders)
    {
        if (shader)
            ImPlatform_DestroyShader(shader);
        shader = NULL;
    }
}

IMPLATFORM_API bool ImPlatform_BeginRenderToTexture(ImTextureID texture)
{
    if (!texture || !g_GfxData.device)
        return false;

    VkDescriptorSet ds = (VkDescriptorSet)texture;
    ImPlatform_RTTracking_Vulkan* entry = g_RTTrackingHead;
    while (entry) { if (entry->descriptorSet == ds) break; entry = entry->next; }
    if (!entry) return false;

    // Begin the dedicated command buffer
    vkResetCommandBuffer(entry->commandBuffer, 0);
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VkResult err = vkBeginCommandBuffer(entry->commandBuffer, &begin_info);
    if (err != VK_SUCCESS) return false;

    // Begin render pass (loadOp = CLEAR handles the clear)
    VkClearValue clear_value = {};
    VkRenderPassBeginInfo rp_info = {};
    rp_info.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_info.renderPass               = entry->renderPass;
    rp_info.framebuffer              = entry->framebuffer;
    rp_info.renderArea.extent.width  = entry->width;
    rp_info.renderArea.extent.height = entry->height;
    rp_info.clearValueCount          = 1;
    rp_info.pClearValues             = &clear_value;
    vkCmdBeginRenderPass(entry->commandBuffer, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = { 0, 0, (float)entry->width, (float)entry->height, 0.f, 1.f };
    vkCmdSetViewport(entry->commandBuffer, 0, 1, &viewport);

    VkRect2D scissor = { {0, 0}, {entry->width, entry->height} };
    vkCmdSetScissor(entry->commandBuffer, 0, 1, &scissor);

    g_ActiveRTEntry = entry;
    return true;
}

IMPLATFORM_API void ImPlatform_EndRenderToTexture(void)
{
    if (!g_ActiveRTEntry) return;

    ImPlatform_RTTracking_Vulkan* entry = g_ActiveRTEntry;
    g_ActiveRTEntry = NULL;

    vkCmdEndRenderPass(entry->commandBuffer);
    vkEndCommandBuffer(entry->commandBuffer);

    VkSubmitInfo submit_info = {};
    submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers    = &entry->commandBuffer;
    vkQueueSubmit(g_GfxData.queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(g_GfxData.queue);
}

IMPLATFORM_API bool ImPlatform_CopyBackbuffer(ImTextureID dst) { (void)dst; return false; }
IMPLATFORM_API void ImPlatform_GetBackbufferSize(unsigned int* width, unsigned int* height)
{
    if (width)  *width  = g_ImPlatform_BackbufferW;
    if (height) *height = g_ImPlatform_BackbufferH;
}

IMPLATFORM_API bool ImPlatform_CopyTexture(ImTextureID dst, ImTextureID src)
{
    // Vulkan texture copy requires proper image tracking which is not yet implemented
    // in this backend (see DestroyTexture note below).
    (void)dst; (void)src;
    return false;
}

IMPLATFORM_API void ImPlatform_DestroyTexture(ImTextureID texture_id)
{
    if (!texture_id)
        return;

    ImPlatform_Vulkan_ForgetShaderTexture(texture_id);

    // Remove from ImGui's texture registry
    VkDescriptorSet descriptor_set = (VkDescriptorSet)texture_id;
    ImGui_ImplVulkan_RemoveTexture(descriptor_set);
    for (ImPlatform_TexReg_Vulkan** e = &g_TexRegHead; *e; e = &(*e)->next)
        if ((*e)->id == descriptor_set)
        {
            ImPlatform_TexReg_Vulkan* dead = *e;
            *e = dead->next;
            if (dead->memory)
            {
                // Frames in flight may still sample it
                vkDeviceWaitIdle(g_GfxData.device);
                vkDestroyImageView(g_GfxData.device, dead->view, g_Allocator);
                vkDestroyImage(g_GfxData.device, dead->image, g_Allocator);
                vkFreeMemory(g_GfxData.device, dead->memory, g_Allocator);
            }
            delete dead;
            break;
        }

    // Render textures own all their objects (frames in flight may still use them: the device is idled first)
    for (ImPlatform_RTTracking_Vulkan** e = &g_RTTrackingHead; *e; e = &(*e)->next)
        if ((*e)->descriptorSet == descriptor_set)
        {
            ImPlatform_RTTracking_Vulkan* rt = *e;
            *e = rt->next;
            vkDeviceWaitIdle(g_GfxData.device);
            vkFreeCommandBuffers(g_GfxData.device, rt->commandPool, 1, &rt->commandBuffer);
            vkDestroyCommandPool(g_GfxData.device, rt->commandPool, g_Allocator);
            vkDestroyFramebuffer(g_GfxData.device, rt->framebuffer, g_Allocator);
            vkDestroyRenderPass(g_GfxData.device, rt->renderPass, g_Allocator);
            vkDestroyImageView(g_GfxData.device, rt->imageView, g_Allocator);
            vkDestroyImage(g_GfxData.device, rt->image, g_Allocator);
            vkFreeMemory(g_GfxData.device, rt->imageMemory, g_Allocator);
            delete rt;
            return;
        }
}

// ============================================================================
// Custom Vertex/Index Buffer Management API - Vulkan
// ============================================================================

// Vertex and index buffers drawn by ImPlatform_DrawIndexed inside draw callbacks, in host-visible memory.
// shortcut: static buffers stay in host-visible memory too; move them to device memory if drawing them gets slow.
struct ImPlatform_BufferData_Vulkan
{
    VkBuffer       buffer;
    VkDeviceMemory memory;
    char*          mapped;
    VkDeviceSize   size;
    bool           index;
    ImPlatform_VertexBufferDesc vb_desc;
    ImVector<ImPlatform_VertexAttribute> attributes;
    ImPlatform_IndexBufferDesc ib_desc;
};
static ImPlatform_BufferData_Vulkan* g_BoundVertexBuffer = NULL;
static ImPlatform_BufferData_Vulkan* g_BoundIndexBuffer = NULL;
static bool ImPlatform_Vulkan_CreateHostBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer* buf, VkDeviceMemory* mem);
static void ImPlatform_Vulkan_DestroyHostBuffer(VkBuffer buf, VkDeviceMemory mem);

static ImPlatform_BufferData_Vulkan* ImPlatform_Vulkan_CreateBuffer(const void* data, VkDeviceSize size, VkBufferUsageFlags usage)
{
    if (!data || size == 0)
        return NULL;
    ImPlatform_BufferData_Vulkan* b = IM_NEW(ImPlatform_BufferData_Vulkan)();
    void* mapped = NULL;
    if (!ImPlatform_Vulkan_CreateHostBuffer(size, usage, &b->buffer, &b->memory))
    {
        IM_DELETE(b);
        return NULL;
    }
    vkMapMemory(g_GfxData.device, b->memory, 0, VK_WHOLE_SIZE, 0, &mapped);
    b->mapped = (char*)mapped;
    b->size = size;
    if (b->mapped)
        memcpy(b->mapped, data, (size_t)size);
    return b;
}

static bool ImPlatform_Vulkan_UpdateBuffer(ImPlatform_BufferData_Vulkan* b, const void* data, VkDeviceSize at, VkDeviceSize bytes)
{
    if (!b || !b->mapped || !data || at + bytes > b->size)
        return false;
    memcpy(b->mapped + at, data, (size_t)bytes);     // Callers must not update a buffer the GPU still reads.
    return true;
}

static void ImPlatform_Vulkan_DestroyBuffer(ImPlatform_BufferData_Vulkan* b)
{
    if (!b)
        return;
    if (g_BoundVertexBuffer == b) g_BoundVertexBuffer = NULL;
    if (g_BoundIndexBuffer == b) g_BoundIndexBuffer = NULL;
    vkDeviceWaitIdle(g_GfxData.device);     // Frames in flight may still draw it
    vkUnmapMemory(g_GfxData.device, b->memory);
    ImPlatform_Vulkan_DestroyHostBuffer(b->buffer, b->memory);
    IM_DELETE(b);
}

IMPLATFORM_API ImPlatform_VertexBuffer ImPlatform_CreateVertexBuffer(const void* vertex_data, const ImPlatform_VertexBufferDesc* desc)
{
    if (!desc || !desc->attributes || !desc->attribute_count || desc->attribute_count > 16)
        return NULL;
    ImPlatform_BufferData_Vulkan* b = ImPlatform_Vulkan_CreateBuffer(vertex_data, (VkDeviceSize)desc->vertex_count * desc->vertex_stride,
                                                                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    if (!b)
        return NULL;
    b->vb_desc = *desc;
    for (unsigned int i = 0; i < desc->attribute_count; i++)
        b->attributes.push_back(desc->attributes[i]);
    b->vb_desc.attributes = b->attributes.Data;
    return (ImPlatform_VertexBuffer)b;
}

IMPLATFORM_API bool ImPlatform_UpdateVertexBuffer(ImPlatform_VertexBuffer vertex_buffer, const void* vertex_data, unsigned int vertex_count, unsigned int offset)
{
    ImPlatform_BufferData_Vulkan* b = (ImPlatform_BufferData_Vulkan*)vertex_buffer;
    const VkDeviceSize stride = b ? b->vb_desc.vertex_stride : 0;
    return b && !b->index && ImPlatform_Vulkan_UpdateBuffer(b, vertex_data, offset * stride, vertex_count * stride);
}

IMPLATFORM_API void ImPlatform_DestroyVertexBuffer(ImPlatform_VertexBuffer vertex_buffer) { ImPlatform_Vulkan_DestroyBuffer((ImPlatform_BufferData_Vulkan*)vertex_buffer); }

IMPLATFORM_API ImPlatform_IndexBuffer ImPlatform_CreateIndexBuffer(const void* index_data, const ImPlatform_IndexBufferDesc* desc)
{
    if (!desc)
        return NULL;
    const VkDeviceSize index_size = desc->format == ImPlatform_IndexFormat_UInt16 ? 2 : 4;
    ImPlatform_BufferData_Vulkan* b = ImPlatform_Vulkan_CreateBuffer(index_data, desc->index_count * index_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    if (!b)
        return NULL;
    b->index = true;
    b->ib_desc = *desc;
    return (ImPlatform_IndexBuffer)b;
}

IMPLATFORM_API bool ImPlatform_UpdateIndexBuffer(ImPlatform_IndexBuffer index_buffer, const void* index_data, unsigned int index_count, unsigned int offset)
{
    ImPlatform_BufferData_Vulkan* b = (ImPlatform_BufferData_Vulkan*)index_buffer;
    const VkDeviceSize index_size = b && b->ib_desc.format == ImPlatform_IndexFormat_UInt16 ? 2 : 4;
    return b && b->index && ImPlatform_Vulkan_UpdateBuffer(b, index_data, offset * index_size, index_count * index_size);
}

IMPLATFORM_API void ImPlatform_DestroyIndexBuffer(ImPlatform_IndexBuffer index_buffer) { ImPlatform_Vulkan_DestroyBuffer((ImPlatform_BufferData_Vulkan*)index_buffer); }
IMPLATFORM_API void ImPlatform_BindVertexBuffer(ImPlatform_VertexBuffer vertex_buffer) { g_BoundVertexBuffer = (ImPlatform_BufferData_Vulkan*)vertex_buffer; }
IMPLATFORM_API void ImPlatform_BindIndexBuffer(ImPlatform_IndexBuffer index_buffer) { g_BoundIndexBuffer = (ImPlatform_BufferData_Vulkan*)index_buffer; }
IMPLATFORM_API void ImPlatform_BindBuffers(ImPlatform_VertexBuffer vertex_buffer, ImPlatform_IndexBuffer index_buffer)
{
    ImPlatform_BindVertexBuffer(vertex_buffer);
    ImPlatform_BindIndexBuffer(index_buffer);
}

// ============================================================================
// Custom Shader System - Vulkan
// ============================================================================

// Custom shaders are plain HLSL compiled to SPIR-V 1.3 with Slang, each register class offset into its own binding range:
//   slangc -target spirv -profile spirv_1_3 -fvk-t-shift 0 0 -fvk-s-shift 16 0 -fvk-b-shift 32 0 -fvk-u-shift 48 0
// so bindings 0-15 are t0-t15, 16-31 are s0-s15 and 32-47 are b0-b15, all in set 0. ImPlatform_CreateShader moves them
// to the sets of ImGui's pipeline layout, so that ImGui keeps binding the texture and the sampler of each draw:
//   t0 -> set 0 binding 0: the texture of the draw command;
//   s0 -> set 1 binding 0: ImGui's sampler (ImPlatform_PushSampler);
//   b0 -> set 2 binding 0: the projection (float4x4, as D3D's vertex constant buffer);
//   b1 -> set 2 binding 1: the parameters (BeginUniformBlock / SetUniform / EndUniformBlock);
//   t1..t8 -> set 3 bindings 0..7: ImPlatform textures from ImPlatform_SetShaderTexture (a white texture when unset).
// Vertex inputs are matched by type: the 4-component input is the color, the first 2-component input (lowest location)
// the position and the second the UV, as in ImDrawVert.
// shortcut: no other graphics register (t9+, s1+, b2+, u) is supported; add bindings for them when a shader needs one.
struct ImPlatform_ShaderData_Vulkan
{
    VkShaderModule shaderModule;
    ImPlatform_ShaderStage stage;
    VkPipeline computePipeline;     // Compute stage: created on first dispatch
    int locPos, locUV, locCol;      // Vertex stage: input locations of ImDrawVert's members, -1 when not read
    bool extraTextures;             // Reads t1..t8 (set 3)
    bool customLayout;              // Vertex stage: inputs other than ImDrawVert's (vertex buffers, by location)
};

// Set 3 written for one combination of t1..t8 (a set the GPU may be reading cannot be rewritten)
struct ImPlatform_TextureSet_Vulkan
{
    ImTextureID     ids[8];
    VkDescriptorSet set;
};
static const int g_TextureSetMax = 32;     // Per program

struct ImPlatform_PipelineVariant_Vulkan
{
    VkRenderPass renderPass;
    ImGuiID      layout;        // 0: ImDrawVert; else a hash of the vertex buffer layout (ImPlatform_DrawIndexed)
    unsigned int topology;      // ImPlatform_DrawIndexed's primitive type
    VkPipeline   pipeline;
};

struct ImPlatform_ShaderProgramData_Vulkan
{
    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;
    int locPos, locUV, locCol;
    ImVector<ImPlatform_PipelineVariant_Vulkan> pipelines;     // One per render pass the program is drawn in
    VkPipelineLayout pipelineLayout;
    VkDescriptorSetLayout setLayouts[4];                        // ImGui's texture and sampler layouts, b0 / b1, t1..t8
    VkDescriptorPool descriptorPool;
    VkDescriptorSet constantSet;                                // Set 2: dynamic offsets into g_ConstantRing
    ImVector<char> parameters;                                  // b1
    bool extraTextures;                                         // The shaders read t1..t8
    ImTextureID textures[8];                                    // t1..t8, 0 when unset
    ImVector<ImPlatform_TextureSet_Vulkan> textureSets;         // Set 3 of each combination drawn so far
    ImTextureID texture0;                                       // t0 of the direct draws (ImPlatform_SetShaderTexture slot 0)
    bool customLayout;                                          // Draws vertex buffers of its own layout only
};
static ImVector<ImPlatform_ShaderProgramData_Vulkan*> g_ShaderPrograms;     // Live programs (DestroyTexture drops their sets)
static ImPlatform_ShaderProgramData_Vulkan* g_BoundShaderProgram = NULL;   // Bound in the command buffer being recorded

// SPIR-V opcodes, decorations and storage class used below
enum
{
    ImPlatform_SpvOpTypeVector = 23, ImPlatform_SpvOpTypePointer = 32, ImPlatform_SpvOpVariable = 59, ImPlatform_SpvOpDecorate = 71,
    ImPlatform_SpvDecorationLocation = 30, ImPlatform_SpvDecorationBinding = 33, ImPlatform_SpvDecorationDescriptorSet = 34,
    ImPlatform_SpvStorageClassInput = 1,
};

// Moves the HLSL registers to ImGui's sets and finds the vertex inputs (see above). False on an unsupported register.
static bool ImPlatform_Vulkan_PrepareSpirv(uint32_t* w, size_t count, ImPlatform_ShaderData_Vulkan* sd)
{
    if (count < 5 || w[0] != 0x07230203)
        return false;
    const uint32_t bound = w[3];
    ImVector<size_t>   binding_at, set_at;     // Word index of each id's Binding / DescriptorSet literal, 0 when none
    ImVector<int>      location;               // Each id's Location, -1 when none
    ImVector<uint32_t> vector_size, pointee, input_ptr_type;
    binding_at.resize((int)bound, 0); set_at.resize((int)bound, 0); location.resize((int)bound, -1);
    vector_size.resize((int)bound, 0); pointee.resize((int)bound, 0); input_ptr_type.resize((int)bound, 0);
    for (size_t i = 5; i < count; )
    {
        const uint32_t op = w[i] & 0xFFFF, len = w[i] >> 16;
        if (len == 0 || i + len > count)
            return false;
        if (op == ImPlatform_SpvOpDecorate && len >= 4 && w[i + 1] < bound)
        {
            if (w[i + 2] == ImPlatform_SpvDecorationBinding)       binding_at[(int)w[i + 1]] = i + 3;
            if (w[i + 2] == ImPlatform_SpvDecorationDescriptorSet) set_at[(int)w[i + 1]] = i + 3;
            if (w[i + 2] == ImPlatform_SpvDecorationLocation)      location[(int)w[i + 1]] = (int)w[i + 3];
        }
        else if (op == ImPlatform_SpvOpTypeVector && len >= 4 && w[i + 1] < bound)
            vector_size[(int)w[i + 1]] = w[i + 3];
        else if (op == ImPlatform_SpvOpTypePointer && len >= 4 && w[i + 1] < bound)
            pointee[(int)w[i + 1]] = w[i + 3];
        else if (op == ImPlatform_SpvOpVariable && len >= 4 && w[i + 2] < bound && w[i + 3] == ImPlatform_SpvStorageClassInput)
            input_ptr_type[(int)w[i + 2]] = w[i + 1];
        i += len;
    }

    for (uint32_t id = 0; id < bound; id++)
    {
        if (!binding_at[(int)id] || !set_at[(int)id])
            continue;
        const uint32_t binding = w[binding_at[(int)id]], reg = binding % 16, cls = binding / 16;   // cls: 0 t, 1 s, 2 b, 3 u
        uint32_t set;
        if (w[set_at[(int)id]] == 0 && cls == 0 && reg == 0)      set = 0;
        else if (w[set_at[(int)id]] == 0 && cls == 1 && reg == 0) set = 1;
        else if (w[set_at[(int)id]] == 0 && cls == 2 && reg <= 1) set = 2;
        else if (w[set_at[(int)id]] == 0 && cls == 0 && reg <= 8) set = 3;
        else
        {
            fprintf(stderr, "[ImPlatform] Vulkan: unsupported register %c%u (set %u, binding %u): custom shaders use t0..t8, s0, b0 and b1\n",
                    "tsbu"[cls < 4 ? cls : 3], reg, w[set_at[(int)id]], binding);
            return false;
        }
        w[set_at[(int)id]] = set;
        w[binding_at[(int)id]] = set == 2 ? reg : set == 3 ? reg - 1 : 0;
        sd->extraTextures |= set == 3;
    }

    sd->locPos = sd->locUV = sd->locCol = -1;
    if (sd->stage != ImPlatform_ShaderStage_Vertex)
        return true;
    int size_at[16] = {};      // Component count of the input at each location
    for (uint32_t id = 0; id < bound; id++)
        if (input_ptr_type[(int)id] && location[(int)id] >= 0 && location[(int)id] < 16)
        {
            const uint32_t type = pointee[(int)input_ptr_type[(int)id]];
            size_at[location[(int)id]] = type < bound && vector_size[(int)type] ? (int)vector_size[(int)type] : 1;
        }
    for (int loc = 0; loc < 16; loc++)
    {
        if (size_at[loc] == 4 && sd->locCol < 0)       sd->locCol = loc;
        else if (size_at[loc] == 2 && sd->locPos < 0)  sd->locPos = loc;
        else if (size_at[loc] == 2 && sd->locUV < 0)   sd->locUV = loc;
        else if (size_at[loc] != 0)
        {
            // Not ImDrawVert: the shader draws vertex buffers of its own layout (ImPlatform_DrawIndexed)
            sd->locPos = sd->locUV = sd->locCol = -1;
            sd->customLayout = true;
            break;
        }
    }
    return true;
}

// Per-draw constants (b0, b1) of custom shaders: a host-visible buffer split into segments, each owned by the command
// buffer recording into it. A segment restarts when its command buffer records again in a later frame: a command buffer
// is only recorded again once its previous submission completed.
struct ImPlatform_ConstantSegment_Vulkan
{
    VkCommandBuffer cmd;
    int             frame;
    uint32_t        used;
};
static const uint32_t g_ConstantBlockSize   = 256;          // Range of b0 and b1: 16 float4
static const uint32_t g_ConstantSegmentSize = 64 * 1024;
static const int      g_ConstantSegmentCount = 16;
static struct
{
    VkBuffer       buffer;
    VkDeviceMemory memory;
    char*          mapped;
    uint32_t       align;
    ImPlatform_ConstantSegment_Vulkan segments[g_ConstantSegmentCount];
} g_ConstantRing;

static bool ImPlatform_Vulkan_CreateHostBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer* buf, VkDeviceMemory* mem);
static void ImPlatform_Vulkan_DestroyHostBuffer(VkBuffer buf, VkDeviceMemory mem);

static bool ImPlatform_Vulkan_InitConstantRing()
{
    if (g_ConstantRing.buffer)
        return true;
    // Also the vertices and indices of ImPlatform_DrawQuadNow (one block each draw)
    const VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (!ImPlatform_Vulkan_CreateHostBuffer((VkDeviceSize)g_ConstantSegmentSize * g_ConstantSegmentCount, usage,
                                            &g_ConstantRing.buffer, &g_ConstantRing.memory))
        return false;
    void* mapped = NULL;
    vkMapMemory(g_GfxData.device, g_ConstantRing.memory, 0, VK_WHOLE_SIZE, 0, &mapped);
    g_ConstantRing.mapped = (char*)mapped;
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(g_GfxData.physicalDevice, &props);
    g_ConstantRing.align = (uint32_t)props.limits.minUniformBufferOffsetAlignment;
    if (g_ConstantRing.align < 16)
        g_ConstantRing.align = 16;
    memset(g_ConstantRing.segments, 0, sizeof(g_ConstantRing.segments));
    return g_ConstantRing.mapped != NULL;
}

static void ImPlatform_Vulkan_ReleaseConstantRing()
{
    if (!g_ConstantRing.buffer)
        return;
    vkUnmapMemory(g_GfxData.device, g_ConstantRing.memory);
    ImPlatform_Vulkan_DestroyHostBuffer(g_ConstantRing.buffer, g_ConstantRing.memory);
    memset(&g_ConstantRing, 0, sizeof(g_ConstantRing));
}

// Copies a constant block (zero padded to g_ConstantBlockSize) for a draw recorded in cmd; returns its buffer offset.
static bool ImPlatform_Vulkan_WriteConstants(VkCommandBuffer cmd, const void* data, uint32_t size, uint32_t* offset)
{
    const int frame = ImGui::GetFrameCount();
    int s = -1;
    for (int i = 0; i < g_ConstantSegmentCount && s < 0; i++)
        if (g_ConstantRing.segments[i].cmd == cmd)
            s = i;
    // A free segment, or one whose command buffer stopped recording long enough ago to be complete (swapchains keep
    // fewer than 8 frames in flight).
    for (int i = 0; i < g_ConstantSegmentCount && s < 0; i++)
        if (!g_ConstantRing.segments[i].cmd || g_ConstantRing.segments[i].frame < frame - 8)
        {
            s = i;
            g_ConstantRing.segments[i].cmd = cmd;
            g_ConstantRing.segments[i].frame = frame - 1;
        }
    if (s < 0)
        return false;
    ImPlatform_ConstantSegment_Vulkan& seg = g_ConstantRing.segments[s];
    if (seg.frame != frame)
    {
        seg.frame = frame;
        seg.used = 0;
    }
    const uint32_t at = (seg.used + g_ConstantRing.align - 1) / g_ConstantRing.align * g_ConstantRing.align;
    if (at + g_ConstantBlockSize > g_ConstantSegmentSize)
        return false;
    char* dst = g_ConstantRing.mapped + (size_t)s * g_ConstantSegmentSize + at;
    const uint32_t n = size < g_ConstantBlockSize ? size : g_ConstantBlockSize;
    if (n)
        memcpy(dst, data, n);
    memset(dst + n, 0, g_ConstantBlockSize - n);
    seg.used = at + g_ConstantBlockSize;
    *offset = (uint32_t)s * g_ConstantSegmentSize + at;
    return true;
}

// Custom Shader System API - Vulkan
//
// Caching strategy:
//   Vulkan only accepts SPIR-V bytecode, which is already the pre-compiled
//   form produced by Slang at build time. There is no runtime source->bytecode
//   compilation step, so the per-shader `cache_key` / `compile_flags` fields
//   of ImPlatform_ShaderDesc are silently ignored at the shader-module level.
//
//   The expensive work on Vulkan happens when pipeline state objects are
//   built. That work IS cached on this backend via a global VK_EXT_pipeline_cache
//   (VkPipelineCache) that is loaded from
//       ./shaders/bytecode_cache/vulkan_global_pipeline_0000000000000000.cache
//   at device-create time and written back on device cleanup. Every call to
//   vkCreateGraphicsPipelines in this file passes that cache. The Vulkan
//   driver itself validates cache compatibility (vendor ID / device ID /
//   UUID) via the header it embeds in the blob, so no extra hashing here.
//   See ImPlatform_Vulkan_InitPipelineCache / _SavePipelineCache above.
IMPLATFORM_API ImPlatform_Shader ImPlatform_CreateShader(const ImPlatform_ShaderDesc* desc)
{
    if (!desc || !desc->bytecode || desc->bytecode_size == 0 || desc->bytecode_size % 4 != 0)
    {
        fprintf(stderr, "[ImPlatform] Vulkan: Invalid shader description\n");
        return NULL;
    }

    // Vulkan only accepts SPIR-V bytecode
    if (desc->format != ImPlatform_ShaderFormat_SPIRV)
    {
        fprintf(stderr, "[ImPlatform] Vulkan: Only SPIR-V format is supported. Please compile your shader to SPIR-V first.\n");
        return NULL;
    }

    ImPlatform_ShaderData_Vulkan* shader_data = (ImPlatform_ShaderData_Vulkan*)malloc(sizeof(ImPlatform_ShaderData_Vulkan));
    if (!shader_data)
        return NULL;

    memset(shader_data, 0, sizeof(ImPlatform_ShaderData_Vulkan));
    shader_data->stage = desc->stage;
    shader_data->locPos = shader_data->locUV = shader_data->locCol = -1;

    ImVector<uint32_t> code;
    code.resize((int)(desc->bytecode_size / 4));
    memcpy(code.Data, desc->bytecode, desc->bytecode_size);
    if (desc->stage != ImPlatform_ShaderStage_Compute && !ImPlatform_Vulkan_PrepareSpirv(code.Data, (size_t)code.Size, shader_data))
    {
        free(shader_data);
        return NULL;
    }

    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = desc->bytecode_size;
    create_info.pCode = code.Data;

    VkResult err = vkCreateShaderModule(g_GfxData.device, &create_info, g_Allocator, &shader_data->shaderModule);
    if (err != VK_SUCCESS)
    {
        fprintf(stderr, "[ImPlatform] Vulkan: Failed to create shader module (VkResult = %d)\n", err);
        free(shader_data);
        return NULL;
    }

    return (ImPlatform_Shader)shader_data;
}

IMPLATFORM_API void ImPlatform_DestroyShader(ImPlatform_Shader shader)
{
    if (!shader)
        return;

    ImPlatform_ShaderData_Vulkan* shader_data = (ImPlatform_ShaderData_Vulkan*)shader;

    if (shader_data->computePipeline)
        vkDestroyPipeline(g_GfxData.device, shader_data->computePipeline, g_Allocator);
    if (shader_data->shaderModule)
        vkDestroyShaderModule(g_GfxData.device, shader_data->shaderModule, g_Allocator);

    free(shader_data);
}

// The program's pipeline for a render pass, created on first use (pipelines are tied to compatible render passes). Without
// 'vertices' it reads ImDrawVert (Dear ImGui's draws); with them, attribute i of the buffer feeds the shader's location i
// (Slang numbers the inputs in declaration order), as 'topology' (ImPlatform_DrawIndexed's primitive type).
static VkPipeline ImPlatform_Vulkan_ProgramPipeline(ImPlatform_ShaderProgramData_Vulkan* program_data, VkRenderPass render_pass,
                                                    const ImPlatform_VertexBufferDesc* vertices = NULL, unsigned int topology = 0)
{
    ImGuiID layout = 0;
    if (vertices)
    {
        // FNV-1a of the stride and each attribute's format and offset, never 0 (ImDrawVert's key)
        layout = (2166136261u ^ vertices->vertex_stride) * 16777619u;
        for (unsigned int i = 0; i < vertices->attribute_count; i++)
            layout = (((layout ^ (ImGuiID)vertices->attributes[i].format) * 16777619u) ^ vertices->attributes[i].offset) * 16777619u;
        layout |= 1u;
    }
    else if (program_data->customLayout)
        return VK_NULL_HANDLE;      // Its inputs are not ImDrawVert's
    for (const ImPlatform_PipelineVariant_Vulkan& v : program_data->pipelines)
        if (v.renderPass == render_pass && v.layout == layout && v.topology == topology)
            return v.pipeline;

    VkPipelineShaderStageCreateInfo stage[2] = {};
    stage[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stage[0].module = program_data->vertShaderModule;
    stage[0].pName = "main";
    stage[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stage[1].module = program_data->fragShaderModule;
    stage[1].pName = "main";

    // Vertex input: ImDrawVert, at the locations the vertex shader reads
    VkVertexInputBindingDescription binding_desc = {};
    binding_desc.stride = vertices ? vertices->vertex_stride : (uint32_t)sizeof(ImDrawVert);
    binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription attribute_desc[16] = {};
    uint32_t attribute_count = 0;
    if (vertices)
    {
        for (unsigned int i = 0; i < vertices->attribute_count && i < 16; i++)
        {
            VkFormat f = VK_FORMAT_R32G32B32A32_SFLOAT;
            switch (vertices->attributes[i].format)
            {
            case ImPlatform_VertexFormat_Float:   f = VK_FORMAT_R32_SFLOAT; break;
            case ImPlatform_VertexFormat_Float2:  f = VK_FORMAT_R32G32_SFLOAT; break;
            case ImPlatform_VertexFormat_Float3:  f = VK_FORMAT_R32G32B32_SFLOAT; break;
            case ImPlatform_VertexFormat_UByte4:  f = VK_FORMAT_R8G8B8A8_UINT; break;
            case ImPlatform_VertexFormat_UByte4N: f = VK_FORMAT_R8G8B8A8_UNORM; break;
            default: break;
            }
            attribute_desc[attribute_count].location = i;
            attribute_desc[attribute_count].format = f;
            attribute_desc[attribute_count].offset = vertices->attributes[i].offset;
            attribute_count++;
        }
    }
    else
    {
        const int locations[3] = { program_data->locPos, program_data->locUV, program_data->locCol };
        const VkFormat formats[3] = { VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R8G8B8A8_UNORM };
        const uint32_t offsets[3] = { (uint32_t)offsetof(ImDrawVert, pos), (uint32_t)offsetof(ImDrawVert, uv), (uint32_t)offsetof(ImDrawVert, col) };
        for (int i = 0; i < 3; i++)
            if (locations[i] >= 0)
            {
                attribute_desc[attribute_count].location = (uint32_t)locations[i];
                attribute_desc[attribute_count].format = formats[i];
                attribute_desc[attribute_count].offset = offsets[i];
                attribute_count++;
            }
    }

    VkPipelineVertexInputStateCreateInfo vertex_info = {};
    vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_info.vertexBindingDescriptionCount = 1;
    vertex_info.pVertexBindingDescriptions = &binding_desc;
    vertex_info.vertexAttributeDescriptionCount = attribute_count;
    vertex_info.pVertexAttributeDescriptions = attribute_desc;

    VkPipelineInputAssemblyStateCreateInfo ia_info = {};
    ia_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    const VkPrimitiveTopology topologies[3] = { VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_PRIMITIVE_TOPOLOGY_POINT_LIST };
    ia_info.topology = topologies[topology < 3 ? topology : 0];

    VkPipelineViewportStateCreateInfo viewport_info = {};
    viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_info.viewportCount = 1;
    viewport_info.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster_info = {};
    raster_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster_info.polygonMode = VK_POLYGON_MODE_FILL;
    raster_info.cullMode = VK_CULL_MODE_NONE;
    raster_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster_info.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms_info = {};
    ms_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_attachment = {};
    color_attachment.blendEnable = VK_TRUE;
    color_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    color_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineDepthStencilStateCreateInfo depth_info = {};
    depth_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    VkPipelineColorBlendStateCreateInfo blend_info = {};
    blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_info.attachmentCount = 1;
    blend_info.pAttachments = &color_attachment;

    VkDynamicState dynamic_states[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic_state = {};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates = dynamic_states;

    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = stage;
    pipeline_info.pVertexInputState = &vertex_info;
    pipeline_info.pInputAssemblyState = &ia_info;
    pipeline_info.pViewportState = &viewport_info;
    pipeline_info.pRasterizationState = &raster_info;
    pipeline_info.pMultisampleState = &ms_info;
    pipeline_info.pDepthStencilState = &depth_info;
    pipeline_info.pColorBlendState = &blend_info;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = program_data->pipelineLayout;
    pipeline_info.renderPass = render_pass;
    pipeline_info.subpass = 0;

    // The global pipeline cache lets the compiled state be reused across runs (see ImPlatform_Vulkan_InitPipelineCache).
    ImPlatform_PipelineVariant_Vulkan variant = { render_pass, layout, topology, VK_NULL_HANDLE };
    VkResult err = vkCreateGraphicsPipelines(g_GfxData.device, g_VulkanPipelineCache, 1, &pipeline_info, g_Allocator, &variant.pipeline);
    if (err != VK_SUCCESS)
    {
        fprintf(stderr, "[ImPlatform] Vulkan: Failed to create graphics pipeline (VkResult = %d)\n", err);
        return VK_NULL_HANDLE;
    }
    program_data->pipelines.push_back(variant);
    return variant.pipeline;
}


IMPLATFORM_API ImPlatform_ShaderProgram ImPlatform_CreateShaderProgram(ImPlatform_Shader vertex_shader, ImPlatform_Shader fragment_shader)
{
    if (!vertex_shader || !fragment_shader)
    {
        fprintf(stderr, "[ImPlatform] Vulkan: Both vertex and fragment shaders are required\n");
        return NULL;
    }

    ImPlatform_ShaderData_Vulkan* vs_data = (ImPlatform_ShaderData_Vulkan*)vertex_shader;
    ImPlatform_ShaderData_Vulkan* fs_data = (ImPlatform_ShaderData_Vulkan*)fragment_shader;

    if (vs_data->stage != ImPlatform_ShaderStage_Vertex || fs_data->stage != ImPlatform_ShaderStage_Fragment)
    {
        fprintf(stderr, "[ImPlatform] Vulkan: Invalid shader stages\n");
        return NULL;
    }
    if (vs_data->locPos < 0 && !vs_data->customLayout)
    {
        fprintf(stderr, "[ImPlatform] Vulkan: the vertex shader reads no position\n");
        return NULL;
    }
    if (!ImPlatform_Vulkan_InitConstantRing())
        return NULL;

    ImPlatform_ShaderProgramData_Vulkan* program_data = IM_NEW(ImPlatform_ShaderProgramData_Vulkan)();
    program_data->vertShaderModule = vs_data->shaderModule;
    program_data->fragShaderModule = fs_data->shaderModule;
    program_data->locPos = vs_data->locPos;
    program_data->locUV = vs_data->locUV;
    program_data->locCol = vs_data->locCol;
    program_data->pipelineLayout = VK_NULL_HANDLE;
    program_data->descriptorPool = VK_NULL_HANDLE;
    program_data->constantSet = VK_NULL_HANDLE;
    program_data->extraTextures = vs_data->extraTextures || fs_data->extraTextures;
    program_data->customLayout = vs_data->customLayout;
    program_data->texture0 = 0;
    memset(program_data->textures, 0, sizeof(program_data->textures));
    for (VkDescriptorSetLayout& layout : program_data->setLayouts)
        layout = VK_NULL_HANDLE;
    g_ShaderPrograms.push_back(program_data);

    // Sets 0 and 1 are defined as ImGui's (a sampled image, a sampler) and the push constant range is ImGui's too: the
    // layouts are then compatible for those sets, so ImGui's per-draw texture binds neither disturb sets 2 and 3 nor
    // need rebinding them.
    bool ok = true;
    for (int set = 0; set < 4 && ok; set++)
    {
        VkDescriptorSetLayoutBinding bindings[8] = {};
        const uint32_t binding_count = set == 2 ? 2 : set == 3 ? 8 : 1;
        for (uint32_t b = 0; b < binding_count; b++)
        {
            bindings[b].binding = b;
            bindings[b].descriptorCount = 1;
            bindings[b].descriptorType = set == 1 ? VK_DESCRIPTOR_TYPE_SAMPLER : set == 2 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            bindings[b].stageFlags = set >= 2 ? (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT) : VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        VkDescriptorSetLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = binding_count;
        layout_info.pBindings = bindings;
        ok = vkCreateDescriptorSetLayout(g_GfxData.device, &layout_info, g_Allocator, &program_data->setLayouts[set]) == VK_SUCCESS;
    }
    if (ok)
    {
        VkPushConstantRange push_constants = {};
        push_constants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        push_constants.size = sizeof(float) * 4;
        VkPipelineLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 4;
        layout_info.pSetLayouts = program_data->setLayouts;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = &push_constants;
        ok = vkCreatePipelineLayout(g_GfxData.device, &layout_info, g_Allocator, &program_data->pipelineLayout) == VK_SUCCESS;
    }
    if (ok)
    {
        const VkDescriptorPoolSize pool_sizes[2] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 2 },
                                                     { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 8 * g_TextureSetMax } };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;     // Texture sets of destroyed textures
        pool_info.poolSizeCount = program_data->extraTextures ? 2 : 1;
        pool_info.pPoolSizes = pool_sizes;
        pool_info.maxSets = program_data->extraTextures ? 1 + g_TextureSetMax : 1;
        ok = vkCreateDescriptorPool(g_GfxData.device, &pool_info, g_Allocator, &program_data->descriptorPool) == VK_SUCCESS;
    }
    if (ok)
    {
        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = program_data->descriptorPool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &program_data->setLayouts[2];
        ok = vkAllocateDescriptorSets(g_GfxData.device, &alloc_info, &program_data->constantSet) == VK_SUCCESS;
    }
    if (ok && program_data->extraTextures && !g_WhiteTexture)
    {
        const unsigned int white = 0xFFFFFFFF;
        const ImPlatform_TextureDesc white_desc = ImPlatform_TextureDesc_Default(1, 1);
        g_WhiteTexture = ImPlatform_CreateTexture(&white, &white_desc);
        ok = g_WhiteTexture != 0;
    }
    if (!ok)
    {
        fprintf(stderr, "[ImPlatform] Vulkan: Failed to create the layout of a shader program\n");
        ImPlatform_DestroyShaderProgram((ImPlatform_ShaderProgram)program_data);
        return NULL;
    }

    VkDescriptorBufferInfo buffer_info = { g_ConstantRing.buffer, 0, g_ConstantBlockSize };
    VkWriteDescriptorSet writes[2] = {};
    for (uint32_t b = 0; b < 2; b++)
    {
        writes[b].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[b].dstSet = program_data->constantSet;
        writes[b].dstBinding = b;
        writes[b].descriptorCount = 1;
        writes[b].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        writes[b].pBufferInfo = &buffer_info;
    }
    vkUpdateDescriptorSets(g_GfxData.device, 2, writes, 0, nullptr);

    return (ImPlatform_ShaderProgram)program_data;
}

IMPLATFORM_API void ImPlatform_DestroyShaderProgram(ImPlatform_ShaderProgram program)
{
    if (!program)
        return;

    ImPlatform_ShaderProgramData_Vulkan* program_data = (ImPlatform_ShaderProgramData_Vulkan*)program;
    g_ShaderPrograms.find_erase_unsorted(program_data);
    if (g_BoundShaderProgram == program_data)
        g_BoundShaderProgram = NULL;

    vkDeviceWaitIdle(g_GfxData.device);     // Frames in flight may still draw with it
    for (const ImPlatform_PipelineVariant_Vulkan& v : program_data->pipelines)
        vkDestroyPipeline(g_GfxData.device, v.pipeline, g_Allocator);
    if (program_data->pipelineLayout)
        vkDestroyPipelineLayout(g_GfxData.device, program_data->pipelineLayout, g_Allocator);
    if (program_data->descriptorPool)
        vkDestroyDescriptorPool(g_GfxData.device, program_data->descriptorPool, g_Allocator);   // Frees its sets
    for (VkDescriptorSetLayout layout : program_data->setLayouts)
        if (layout)
            vkDestroyDescriptorSetLayout(g_GfxData.device, layout, g_Allocator);
    IM_DELETE(program_data);
}

IMPLATFORM_API void ImPlatform_UseShaderProgram(ImPlatform_ShaderProgram /*program*/)
{
    // Not needed for Vulkan - shaders are bound through draw callbacks
}

// A whole b1 constant buffer, as on Direct3D (the name is not used)
IMPLATFORM_API bool ImPlatform_SetShaderUniform(ImPlatform_ShaderProgram program, const char* /*name*/, const void* data, unsigned int size)
{
    if (!program || !data || size == 0)
        return false;
    ImPlatform_ShaderProgramData_Vulkan* program_data = (ImPlatform_ShaderProgramData_Vulkan*)program;
    program_data->parameters.resize((int)size);
    memcpy(program_data->parameters.Data, data, size);
    return true;
}

// Set 3 holding the program's current t1..t8, written on first use of the combination. Null when the pool is full.
static VkDescriptorSet ImPlatform_Vulkan_TextureSet(ImPlatform_ShaderProgramData_Vulkan* program_data)
{
    for (const ImPlatform_TextureSet_Vulkan& s : program_data->textureSets)
        if (memcmp(s.ids, program_data->textures, sizeof(s.ids)) == 0)
            return s.set;

    VkDescriptorImageInfo infos[8] = {};
    for (int i = 0; i < 8; i++)
    {
        const ImPlatform_TexReg_Vulkan* e = ImPlatform_Vulkan_FindTexture(program_data->textures[i] ? program_data->textures[i] : g_WhiteTexture);
        if (!e)
            return VK_NULL_HANDLE;
        infos[i].imageView = e->view;
        infos[i].imageLayout = e->idleLayout;
    }
    ImPlatform_TextureSet_Vulkan entry;
    memcpy(entry.ids, program_data->textures, sizeof(entry.ids));
    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = program_data->descriptorPool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &program_data->setLayouts[3];
    if (vkAllocateDescriptorSets(g_GfxData.device, &alloc_info, &entry.set) != VK_SUCCESS)
    {
        static bool reported = false;
        if (!reported)
            fprintf(stderr, "[ImPlatform] Vulkan: more than %d texture combinations on a custom shader, draws skipped\n", g_TextureSetMax);
        reported = true;
        return VK_NULL_HANDLE;
    }
    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = entry.set;
    write.descriptorCount = 8;      // Bindings 0..7
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    write.pImageInfo = infos;
    vkUpdateDescriptorSets(g_GfxData.device, 1, &write, 0, nullptr);
    program_data->textureSets.push_back(entry);
    return entry.set;
}

// Drops the texture from every program (DestroyTexture): its sets are freed, slots holding it become unset.
static void ImPlatform_Vulkan_ForgetShaderTexture(ImTextureID texture)
{
    bool idle = false;
    for (ImPlatform_ShaderProgramData_Vulkan* p : g_ShaderPrograms)
    {
        for (ImTextureID& t : p->textures)
            if (t == texture)
                t = 0;
        if (p->texture0 == texture)
            p->texture0 = 0;
        for (int i = p->textureSets.Size - 1; i >= 0; i--)
        {
            bool uses = false;
            for (ImTextureID id : p->textureSets[i].ids)
                uses |= id == texture;
            if (!uses)
                continue;
            if (!idle)
                vkDeviceWaitIdle(g_GfxData.device);     // Frames in flight may still read the set
            idle = true;
            vkFreeDescriptorSets(g_GfxData.device, p->descriptorPool, 1, &p->textureSets[i].set);
            p->textureSets.erase(p->textureSets.Data + i);
        }
    }
}

// t<slot>: an ImPlatform texture. Slots 1..8 (0: unset, a white texture) apply from the program's next bind; slot 0 serves
// the direct draws (ImPlatform_DrawQuadNow, ImPlatform_DrawIndexed; Dear ImGui's draws bind their own t0). Both apply at
// once inside a draw callback with the program bound, as on Direct3D.
IMPLATFORM_API bool ImPlatform_SetShaderTexture(ImPlatform_ShaderProgram program, const char* /*name*/, unsigned int slot, ImTextureID texture)
{
    ImPlatform_ShaderProgramData_Vulkan* program_data = (ImPlatform_ShaderProgramData_Vulkan*)program;
    if (!program_data || slot > 8 || (slot >= 1 && !program_data->extraTextures) || (texture && !ImPlatform_Vulkan_FindTexture(texture)))
        return false;
    ImGui_ImplVulkan_RenderState* render_state = (ImGui_ImplVulkan_RenderState*)ImGui::GetPlatformIO().Renderer_RenderState;
    const bool bound = render_state && render_state->CommandBuffer && g_BoundShaderProgram == program_data;
    if (slot == 0)
    {
        // The ImTextureID is the descriptor set Dear ImGui binds as set 0 (layout compatible with the program's)
        program_data->texture0 = texture;
        VkDescriptorSet set = (VkDescriptorSet)texture;
        if (bound && set != VK_NULL_HANDLE)
            vkCmdBindDescriptorSets(render_state->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, program_data->pipelineLayout, 0, 1, &set, 0, nullptr);
        return true;
    }
    program_data->textures[slot - 1] = texture;
    if (bound)
    {
        VkDescriptorSet set = ImPlatform_Vulkan_TextureSet(program_data);
        if (set == VK_NULL_HANDLE)
            return false;
        vkCmdBindDescriptorSets(render_state->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, program_data->pipelineLayout, 3, 1, &set, 0, nullptr);
    }
    return true;
}

static ImVector<char> g_UniformBlockBytes;      // Accumulated block (D3D packing: back to back)

IMPLATFORM_API void ImPlatform_BeginUniformBlock(ImPlatform_ShaderProgram program)
{
    if (!program)
        return;
    g_CurrentUniformBlockProgram = program;
    g_UniformBlockBytes.resize(0);
}

IMPLATFORM_API bool ImPlatform_SetUniform(const char* /*name*/, const void* data, unsigned int size)
{
    if (!g_CurrentUniformBlockProgram || !data || size == 0)
        return false;
    const int at = g_UniformBlockBytes.Size;
    g_UniformBlockBytes.resize(at + (int)size);
    memcpy(g_UniformBlockBytes.Data + at, data, size);
    return true;
}

IMPLATFORM_API void ImPlatform_EndUniformBlock(ImPlatform_ShaderProgram program)
{
    if (!program || program != g_CurrentUniformBlockProgram)
        return;
    ImPlatform_ShaderProgramData_Vulkan* program_data = (ImPlatform_ShaderProgramData_Vulkan*)program;
    program_data->parameters = g_UniformBlockBytes;
    g_UniformBlockBytes.resize(0);
    g_CurrentUniformBlockProgram = nullptr;
}

// ============================================================================
// Custom Shader DrawList Integration
// ============================================================================

// The draw data being rendered: the pass's, else the viewport owning parent_list, else the main one.
static ImDrawData* ImPlatform_Vulkan_FindDrawData(const ImDrawList* parent_list)
{
    if (g_CurrentDrawData)
        return g_CurrentDrawData;
#ifdef IMGUI_HAS_VIEWPORT
    if (parent_list)
    {
        ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
        for (ImGuiViewport* viewport : platform_io.Viewports)
            if (viewport->DrawData)
                for (ImDrawList* list : viewport->DrawData->CmdLists)
                    if (list == parent_list)
                        return viewport->DrawData;
    }
#else
    (void)parent_list;
#endif
    return ImGui::GetDrawData();
}

// b1 of ImPlatform_PushShaderConstants, until ImPlatform_PopShaderConstants
static ImVector<char> g_PushedConstants;
static bool g_PushedConstantsActive = false;

// Orthographic projection of a rectangle; Vulkan's clip space points down, the top (T) maps to -1 as in ImGui's backend
static void ImPlatform_Vulkan_Projection(float L, float T, float R, float B, float mvp[4][4])
{
    const float m[4][4] =
    {
        { 2.0f/(R-L),   0.0f,           0.0f,       0.0f },
        { 0.0f,         2.0f/(B-T),     0.0f,       0.0f },
        { 0.0f,         0.0f,          -1.0f,       0.0f },
        { (R+L)/(L-R),  (T+B)/(T-B),    0.0f,       1.0f },
    };
    memcpy(mvp, m, sizeof(m));
}

// Binds a program and its constants (b0: the draw data's projection or 'projection', b1: the pushed constants or its
// parameters) in cmd. texture_set, when not null, is bound as set 0 (else the program's slot 0 texture, if any): ImGui then
// rebinds set 0 with a layout compatible for it, which keeps sets 2 and 3. 'vertices' selects the pipeline of a vertex
// buffer layout (ImPlatform_DrawIndexed); a program with its own layout binds its pipeline there only.
static void ImPlatform_Vulkan_BindProgram(ImPlatform_ShaderProgramData_Vulkan* program_data, VkCommandBuffer cmd, ImDrawData* draw_data, VkDescriptorSet texture_set,
                                          const ImPlatform_VertexBufferDesc* vertices = NULL, unsigned int topology = 0, const float* projection = NULL)
{
    if (!draw_data || cmd == VK_NULL_HANDLE)
        return;
    VkPipeline pipeline = ImPlatform_Vulkan_ProgramPipeline(program_data, g_CurrentRenderPass ? g_CurrentRenderPass : g_MainWindowData.RenderPass,
                                                            vertices, topology);
    if (pipeline == VK_NULL_HANDLE && (vertices || !program_data->customLayout))
        return;

    float mvp[4][4];
    if (projection)
        memcpy(mvp, projection, sizeof(mvp));
    else
        ImPlatform_Vulkan_Projection(draw_data->DisplayPos.x, draw_data->DisplayPos.y, draw_data->DisplayPos.x + draw_data->DisplaySize.x,
                                     draw_data->DisplayPos.y + draw_data->DisplaySize.y, mvp);
    const ImVector<char>& b1 = g_PushedConstantsActive ? g_PushedConstants : program_data->parameters;
    uint32_t offsets[2];
    if (!ImPlatform_Vulkan_WriteConstants(cmd, mvp, sizeof(mvp), &offsets[0]) ||
        !ImPlatform_Vulkan_WriteConstants(cmd, b1.Data, (uint32_t)b1.Size, &offsets[1]))
    {
        static bool reported = false;
        if (!reported)
            fprintf(stderr, "[ImPlatform] Vulkan: custom shader constants full, draws skipped\n");
        reported = true;
        return;
    }

    VkDescriptorSet extra_set = VK_NULL_HANDLE;
    if (program_data->extraTextures && (extra_set = ImPlatform_Vulkan_TextureSet(program_data)) == VK_NULL_HANDLE)
        return;

    if (pipeline != VK_NULL_HANDLE)
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    if (texture_set == VK_NULL_HANDLE)
        texture_set = (VkDescriptorSet)program_data->texture0;
    if (texture_set != VK_NULL_HANDLE)
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, program_data->pipelineLayout, 0, 1, &texture_set, 0, nullptr);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, program_data->pipelineLayout, 2, 1, &program_data->constantSet, 2, offsets);
    if (extra_set != VK_NULL_HANDLE)
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, program_data->pipelineLayout, 3, 1, &extra_set, 0, nullptr);
    g_BoundShaderProgram = program_data;
}

// ImDrawCallback handler to activate a custom shader
static void ImPlatform_SetCustomShader(const ImDrawList* parent_list, const ImDrawCmd* cmd)
{
    ImPlatform_ShaderProgramData_Vulkan* program_data = (ImPlatform_ShaderProgramData_Vulkan*)cmd->UserCallbackData;
    if (!program_data)
        return;
    // ImGui's render state holds the command buffer of whichever viewport or pass is being recorded
    ImGui_ImplVulkan_RenderState* render_state = (ImGui_ImplVulkan_RenderState*)ImGui::GetPlatformIO().Renderer_RenderState;
    VkCommandBuffer command_buffer = render_state ? render_state->CommandBuffer : g_CurrentCommandBuffer;
    // The texture of the next draw, bound now so that set 0 is always bound with a compatible layout
    VkDescriptorSet texture_set = VK_NULL_HANDLE;
    for (const ImDrawCmd* next = cmd + 1; next < parent_list->CmdBuffer.Data + parent_list->CmdBuffer.Size; next++)
        if (next->UserCallback == NULL)
        {
            texture_set = (VkDescriptorSet)next->GetTexID();
            break;
        }
    ImPlatform_Vulkan_BindProgram(program_data, command_buffer, ImPlatform_Vulkan_FindDrawData(parent_list), texture_set);
}

// Activate a custom shader immediately (for use inside draw callbacks).
IMPLATFORM_API void ImPlatform_BeginCustomShader_Render(ImPlatform_ShaderProgram program)
{
    if (!program)
        return;
    ImGui_ImplVulkan_RenderState* render_state = (ImGui_ImplVulkan_RenderState*)ImGui::GetPlatformIO().Renderer_RenderState;
    ImPlatform_Vulkan_BindProgram((ImPlatform_ShaderProgramData_Vulkan*)program, render_state ? render_state->CommandBuffer : g_CurrentCommandBuffer,
                                  ImPlatform_Vulkan_FindDrawData(NULL), VK_NULL_HANDLE);
}

IMPLATFORM_API void ImPlatform_BeginCustomShader(ImDrawList* draw, ImPlatform_ShaderProgram shader)
{
    if (!draw || !shader)
        return;

    draw->AddCallback(&ImPlatform_SetCustomShader, shader);
}

IMPLATFORM_API void ImPlatform_EndCustomShader(ImDrawList* draw)
{
    if (!draw)
        return;

    draw->AddCallback([](const ImDrawList*, const ImDrawCmd*) { g_BoundShaderProgram = NULL; }, NULL);
    draw->AddCallback(ImGui::GetPlatformIO().DrawCallback_ResetRenderState, NULL);
}

// b1 for the draws that follow in this draw callback, and for Dear ImGui's draws after it until a program is bound again
// (as the Direct3D 11 backend leaves it bound). At most 256 bytes (the constant block).
IMPLATFORM_API void* ImPlatform_PushShaderConstants(const void* data, unsigned int size)
{
    if (!data || size == 0 || size > g_ConstantBlockSize)
        return nullptr;
    g_PushedConstants.resize((int)size);
    memcpy(g_PushedConstants.Data, data, size);
    g_PushedConstantsActive = true;
    ImGui_ImplVulkan_RenderState* render_state = (ImGui_ImplVulkan_RenderState*)ImGui::GetPlatformIO().Renderer_RenderState;
    if (render_state && render_state->CommandBuffer && g_BoundShaderProgram)
        ImPlatform_Vulkan_BindProgram(g_BoundShaderProgram, render_state->CommandBuffer, ImPlatform_Vulkan_FindDrawData(NULL), VK_NULL_HANDLE);
    return &g_PushedConstants;
}

IMPLATFORM_API void ImPlatform_PopShaderConstants(void* handle)
{
    if (handle == &g_PushedConstants)
        g_PushedConstantsActive = false;
}

// ============================================================================
// Sampler Override API - Vulkan
// ============================================================================
// Uses the standard DrawCallback_SetSamplerLinear/Nearest callbacks of the ImGui Vulkan backend,
// which binds its own linear/nearest VK_DESCRIPTOR_TYPE_SAMPLER descriptor sets.
// Only the filter is honored: the wrap mode stays clamp.

IMPLATFORM_API void ImPlatform_PushSampler(ImPlatform_TextureFilter filter, ImPlatform_TextureWrap /*wrap*/)
{
    ImPlatform_PushSampler_StdCallbacks(filter);
}

IMPLATFORM_API void ImPlatform_PopSampler(void)
{
    ImPlatform_PopSampler_StdCallbacks();
}

// ============================================================================
// Readback and compute
// ============================================================================
// shortcut: each call records the upload command buffer and waits for the queue to idle (like
// ImPlatform_CreateTexture): simple and ordered with frames, at the cost of a GPU sync per call.
enum { ImPlatform_Vulkan_MaxComputeInputs = 8, ImPlatform_Vulkan_MaxComputeOutputs = 8 };

static VkCommandBuffer ImPlatform_Vulkan_BeginOneShot()
{
    if (g_UploadCommandPool == VK_NULL_HANDLE)
    {
        VkCommandPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = g_QueueFamily;
        if (vkCreateCommandPool(g_GfxData.device, &pool_info, g_Allocator, &g_UploadCommandPool) != VK_SUCCESS)
            return VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo cb_info = {};
        cb_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cb_info.commandPool = g_UploadCommandPool;
        cb_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cb_info.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(g_GfxData.device, &cb_info, &g_UploadCommandBuffer) != VK_SUCCESS)
            return VK_NULL_HANDLE;
    }
    vkResetCommandBuffer(g_UploadCommandBuffer, 0);
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    return vkBeginCommandBuffer(g_UploadCommandBuffer, &begin_info) == VK_SUCCESS ? g_UploadCommandBuffer : VK_NULL_HANDLE;
}
static bool ImPlatform_Vulkan_EndOneShot(VkCommandBuffer cb)
{
    vkEndCommandBuffer(cb);
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cb;
    if (vkQueueSubmit(g_GfxData.queue, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS)
        return false;
    return vkQueueWaitIdle(g_GfxData.queue) == VK_SUCCESS;
}
static void ImPlatform_Vulkan_ImageBarrier(VkCommandBuffer cb, VkImage image, VkImageLayout from, VkImageLayout to,
                                           VkPipelineStageFlags dst_stage, VkAccessFlags dst_access)
{
    VkImageMemoryBarrier b = {};
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.oldLayout = from;
    b.newLayout = to;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = image;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.levelCount = 1;
    b.subresourceRange.layerCount = 1;
    b.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    b.dstAccessMask = dst_access;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, dst_stage, 0, 0, NULL, 0, NULL, 1, &b);
}
// Host-visible, coherent buffer (staging, readback, constants)
static bool ImPlatform_Vulkan_CreateHostBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer* buf, VkDeviceMemory* mem)
{
    VkBufferCreateInfo bi = {};
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(g_GfxData.device, &bi, g_Allocator, buf) != VK_SUCCESS)
        return false;
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(g_GfxData.device, *buf, &req);
    VkMemoryAllocateInfo ai = {};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = ImPlatform_FindMemoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, req.memoryTypeBits);
    if (ai.memoryTypeIndex == 0xFFFFFFFF || vkAllocateMemory(g_GfxData.device, &ai, g_Allocator, mem) != VK_SUCCESS)
    {
        vkDestroyBuffer(g_GfxData.device, *buf, g_Allocator);
        return false;
    }
    vkBindBufferMemory(g_GfxData.device, *buf, *mem, 0);
    return true;
}
static void ImPlatform_Vulkan_DestroyHostBuffer(VkBuffer buf, VkDeviceMemory mem)
{
    vkDestroyBuffer(g_GfxData.device, buf, g_Allocator);
    vkFreeMemory(g_GfxData.device, mem, g_Allocator);
}

IMPLATFORM_API bool ImPlatform_SupportsReadback(void) { return g_GfxData.device != VK_NULL_HANDLE; }

IMPLATFORM_API bool ImPlatform_ReadTexture(ImTextureID texture, unsigned int x, unsigned int y,
                                           unsigned int width, unsigned int height, void* out_pixels, size_t out_size)
{
    ImPlatform_TexReg_Vulkan* t = ImPlatform_Vulkan_FindTexture(texture);
    const VkDeviceSize size = (VkDeviceSize)width * height * (t ? t->bpp : 0);
    if (!t || !out_pixels || !width || !height || !g_GfxData.device
        || x + width > t->width || y + height > t->height || out_size < (size_t)size)
        return false;

    VkBuffer buf;
    VkDeviceMemory mem;
    if (!ImPlatform_Vulkan_CreateHostBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, &buf, &mem))
        return false;
    VkCommandBuffer cb = ImPlatform_Vulkan_BeginOneShot();
    if (!cb)
    {
        ImPlatform_Vulkan_DestroyHostBuffer(buf, mem);
        return false;
    }
    ImPlatform_Vulkan_ImageBarrier(cb, t->image, t->idleLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    VkBufferImageCopy region = {};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { (int32_t)x, (int32_t)y, 0 };
    region.imageExtent = { width, height, 1 };
    vkCmdCopyImageToBuffer(cb, t->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buf, 1, &region);
    ImPlatform_Vulkan_ImageBarrier(cb, t->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, t->idleLayout, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT);
    VkMemoryBarrier host = {};
    host.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &host, 0, NULL, 0, NULL);
    bool ok = ImPlatform_Vulkan_EndOneShot(cb);

    void* mapped = NULL;
    ok = ok && vkMapMemory(g_GfxData.device, mem, 0, size, 0, &mapped) == VK_SUCCESS;
    if (ok)
    {
        memcpy(out_pixels, mapped, (size_t)size);   // rows are tightly packed (bufferRowLength 0)
        vkUnmapMemory(g_GfxData.device, mem);
    }
    ImPlatform_Vulkan_DestroyHostBuffer(buf, mem);
    return ok;
}

IMPLATFORM_API bool ImPlatform_SupportsCompute(void) { return g_GfxData.device != VK_NULL_HANDLE; }

IMPLATFORM_API ImTextureID ImPlatform_CreateStorageTexture(const void* pixel_data, const ImPlatform_TextureDesc* desc)
{
    if (!desc || !desc->width || !desc->height || !g_GfxData.device)
        return (ImTextureID)0;
    int bpp = 0;
    const VkFormat format = ImPlatform_GetVulkanFormat(desc->format, &bpp);

    VkImageCreateInfo ii = {};
    ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = format;
    ii.extent = { desc->width, desc->height, 1 };
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImage image;
    if (vkCreateImage(g_GfxData.device, &ii, g_Allocator, &image) != VK_SUCCESS)
        return (ImTextureID)0;
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(g_GfxData.device, image, &req);
    VkMemoryAllocateInfo ai = {};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = ImPlatform_FindMemoryType(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, req.memoryTypeBits);
    VkDeviceMemory memory;
    if (vkAllocateMemory(g_GfxData.device, &ai, g_Allocator, &memory) != VK_SUCCESS)
    {
        vkDestroyImage(g_GfxData.device, image, g_Allocator);
        return (ImTextureID)0;
    }
    vkBindImageMemory(g_GfxData.device, image, memory, 0);

    VkImageViewCreateInfo vi = {};
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = format;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    VkImageView view;
    if (vkCreateImageView(g_GfxData.device, &vi, g_Allocator, &view) != VK_SUCCESS)
    {
        vkFreeMemory(g_GfxData.device, memory, g_Allocator);
        vkDestroyImage(g_GfxData.device, image, g_Allocator);
        return (ImTextureID)0;
    }

    // Fill with the pixels, or clear to zero (the texture starts zeroed, as on DX11); it lives in GENERAL,
    // where it can be both sampled and written as a storage image.
    const VkDeviceSize upload_size = (VkDeviceSize)desc->width * desc->height * bpp;
    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory staging_mem = VK_NULL_HANDLE;
    bool ok = !pixel_data || ImPlatform_Vulkan_CreateHostBuffer(upload_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &staging, &staging_mem);
    void* mapped = NULL;
    if (ok && pixel_data && vkMapMemory(g_GfxData.device, staging_mem, 0, upload_size, 0, &mapped) == VK_SUCCESS)
    {
        memcpy(mapped, pixel_data, (size_t)upload_size);
        vkUnmapMemory(g_GfxData.device, staging_mem);
    }
    VkCommandBuffer cb = ok ? ImPlatform_Vulkan_BeginOneShot() : VK_NULL_HANDLE;
    if (cb)
    {
        ImPlatform_Vulkan_ImageBarrier(cb, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        if (pixel_data)
        {
            VkBufferImageCopy region = {};
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.layerCount = 1;
            region.imageExtent = { desc->width, desc->height, 1 };
            vkCmdCopyBufferToImage(cb, staging, image, VK_IMAGE_LAYOUT_GENERAL, 1, &region);
        }
        else
        {
            VkClearColorValue zero = {};
            VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            vkCmdClearColorImage(cb, image, VK_IMAGE_LAYOUT_GENERAL, &zero, 1, &range);
        }
        ImPlatform_Vulkan_ImageBarrier(cb, image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT);
        ok = ImPlatform_Vulkan_EndOneShot(cb);
    }
    else
        ok = false;
    if (staging)
        ImPlatform_Vulkan_DestroyHostBuffer(staging, staging_mem);
    VkDescriptorSet ds = ok ? ImGui_ImplVulkan_AddTexture(view, VK_IMAGE_LAYOUT_GENERAL) : VK_NULL_HANDLE;
    if (!ds)
    {
        vkDestroyImageView(g_GfxData.device, view, g_Allocator);
        vkFreeMemory(g_GfxData.device, memory, g_Allocator);
        vkDestroyImage(g_GfxData.device, image, g_Allocator);
        return (ImTextureID)0;
    }
    ImPlatform_Vulkan_RegisterTexture(ds, image, view, memory, format, desc->width, desc->height, bpp, VK_IMAGE_LAYOUT_GENERAL, true);
    return (ImTextureID)ds;
}

// One layout shared by every compute shader (set 0): binding 0 = uniform buffer (the constants),
// bindings 1..8 = combined image samplers for the inputs (linear clamp), 9..16 = storage images for the outputs.
static bool ImPlatform_Vulkan_ComputeLayout()
{
    if (g_ComputePipelineLayout)
        return true;
    VkSamplerCreateInfo si = {};
    si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.magFilter = si.minFilter = VK_FILTER_LINEAR;
    si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.maxLod = VK_LOD_CLAMP_NONE;
    if (!g_ComputeSampler && vkCreateSampler(g_GfxData.device, &si, g_Allocator, &g_ComputeSampler) != VK_SUCCESS)
        return false;
    VkDescriptorSetLayoutBinding b[1 + ImPlatform_Vulkan_MaxComputeInputs + ImPlatform_Vulkan_MaxComputeOutputs] = {};
    for (uint32_t i = 0; i < IM_ARRAYSIZE(b); i++)
    {
        b[i].binding = i;
        b[i].descriptorType = i == 0 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                            : i <= ImPlatform_Vulkan_MaxComputeInputs ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
                            : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        b[i].descriptorCount = 1;
        b[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    VkDescriptorSetLayoutCreateInfo li = {};
    li.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    li.bindingCount = IM_ARRAYSIZE(b);
    li.pBindings = b;
    if (!g_ComputeSetLayout && vkCreateDescriptorSetLayout(g_GfxData.device, &li, g_Allocator, &g_ComputeSetLayout) != VK_SUCCESS)
        return false;
    VkPipelineLayoutCreateInfo pi = {};
    pi.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pi.setLayoutCount = 1;
    pi.pSetLayouts = &g_ComputeSetLayout;
    return vkCreatePipelineLayout(g_GfxData.device, &pi, g_Allocator, &g_ComputePipelineLayout) == VK_SUCCESS;
}

IMPLATFORM_API bool ImPlatform_DispatchCompute(ImPlatform_Shader compute_shader,
                                               const ImTextureID* inputs, unsigned int input_count,
                                               const ImTextureID* outputs, unsigned int output_count,
                                               const void* constants, unsigned int constants_size,
                                               unsigned int groups_x, unsigned int groups_y, unsigned int groups_z)
{
    ImPlatform_ShaderData_Vulkan* cs = (ImPlatform_ShaderData_Vulkan*)compute_shader;
    if (!cs || cs->stage != ImPlatform_ShaderStage_Compute || !cs->shaderModule || !g_GfxData.device
        || input_count > ImPlatform_Vulkan_MaxComputeInputs || output_count > ImPlatform_Vulkan_MaxComputeOutputs
        || (input_count && !inputs) || (output_count && !outputs) || (constants_size && !constants)
        || !ImPlatform_Vulkan_ComputeLayout())
        return false;
    ImPlatform_TexReg_Vulkan* in[ImPlatform_Vulkan_MaxComputeInputs] = {};
    ImPlatform_TexReg_Vulkan* out[ImPlatform_Vulkan_MaxComputeOutputs] = {};
    for (unsigned int i = 0; i < input_count; i++)
        if (!(in[i] = ImPlatform_Vulkan_FindTexture(inputs[i])))
            return false;
    for (unsigned int i = 0; i < output_count; i++)
        if (!(out[i] = ImPlatform_Vulkan_FindTexture(outputs[i])) || !out[i]->storage)
            return false;     // not a storage texture
    if (!cs->computePipeline)
    {
        VkComputePipelineCreateInfo ci = {};
        ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        ci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        ci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        ci.stage.module = cs->shaderModule;
        ci.stage.pName = "main";
        ci.layout = g_ComputePipelineLayout;
        if (vkCreateComputePipelines(g_GfxData.device, g_VulkanPipelineCache, 1, &ci, g_Allocator, &cs->computePipeline) != VK_SUCCESS)
            return false;
    }

    // Per-dispatch descriptor pool: freed once the dispatch has completed
    VkDescriptorPoolSize sizes[3] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, ImPlatform_Vulkan_MaxComputeInputs },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, ImPlatform_Vulkan_MaxComputeOutputs } };
    VkDescriptorPoolCreateInfo pci = {};
    pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pci.maxSets = 1;
    pci.poolSizeCount = 3;
    pci.pPoolSizes = sizes;
    VkDescriptorPool pool;
    if (vkCreateDescriptorPool(g_GfxData.device, &pci, g_Allocator, &pool) != VK_SUCCESS)
        return false;
    VkDescriptorSetAllocateInfo dai = {};
    dai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dai.descriptorPool = pool;
    dai.descriptorSetCount = 1;
    dai.pSetLayouts = &g_ComputeSetLayout;
    VkDescriptorSet set;
    VkBuffer ubo = VK_NULL_HANDLE;
    VkDeviceMemory ubo_mem = VK_NULL_HANDLE;
    bool ok = vkAllocateDescriptorSets(g_GfxData.device, &dai, &set) == VK_SUCCESS;
    if (ok && constants_size)
    {
        void* mapped = NULL;
        ok = ImPlatform_Vulkan_CreateHostBuffer(constants_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &ubo, &ubo_mem)
          && vkMapMemory(g_GfxData.device, ubo_mem, 0, constants_size, 0, &mapped) == VK_SUCCESS;
        if (ok)
        {
            memcpy(mapped, constants, constants_size);
            vkUnmapMemory(g_GfxData.device, ubo_mem);
        }
    }
    VkCommandBuffer cb = ok ? ImPlatform_Vulkan_BeginOneShot() : VK_NULL_HANDLE;
    if (cb)
    {
        VkWriteDescriptorSet w[1 + ImPlatform_Vulkan_MaxComputeInputs + ImPlatform_Vulkan_MaxComputeOutputs] = {};
        VkDescriptorImageInfo img[ImPlatform_Vulkan_MaxComputeInputs + ImPlatform_Vulkan_MaxComputeOutputs] = {};
        VkDescriptorBufferInfo bi = { ubo, 0, VK_WHOLE_SIZE };
        uint32_t n = 0;
        if (ubo)
        {
            w[n].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w[n].dstSet = set;
            w[n].dstBinding = 0;
            w[n].descriptorCount = 1;
            w[n].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            w[n].pBufferInfo = &bi;
            n++;
        }
        for (unsigned int i = 0; i < input_count + output_count; i++)
        {
            const bool is_in = i < input_count;
            ImPlatform_TexReg_Vulkan* t = is_in ? in[i] : out[i - input_count];
            img[i].sampler = is_in ? g_ComputeSampler : VK_NULL_HANDLE;
            img[i].imageView = t->view;
            img[i].imageLayout = t->idleLayout;   // inputs sample in their idle layout, outputs are GENERAL
            w[n].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w[n].dstSet = set;
            w[n].dstBinding = is_in ? 1 + i : 1 + ImPlatform_Vulkan_MaxComputeInputs + (i - input_count);
            w[n].descriptorCount = 1;
            w[n].descriptorType = is_in ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            w[n].pImageInfo = &img[i];
            n++;
        }
        vkUpdateDescriptorSets(g_GfxData.device, n, w, 0, NULL);

        VkMemoryBarrier mb = {};
        mb.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        mb.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
        mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &mb, 0, NULL, 0, NULL);
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, cs->computePipeline);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, g_ComputePipelineLayout, 0, 1, &set, 0, NULL);
        vkCmdDispatch(cb, groups_x, groups_y, groups_z);
        mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        mb.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &mb, 0, NULL, 0, NULL);
        ok = ImPlatform_Vulkan_EndOneShot(cb);
    }
    else
        ok = false;
    if (ubo)
        ImPlatform_Vulkan_DestroyHostBuffer(ubo, ubo_mem);
    vkDestroyDescriptorPool(g_GfxData.device, pool, g_Allocator);
    return ok;
}

// ---- Direct draws from draw callbacks ----------------------------------------------------------------------------
// Draw callbacks record on the frame's command buffer inside a render pass (g_ActivePasses). Switching targets or copying
// the current one ends that pass and begins it again afterwards with a LOAD pass, so what was drawn stays.
// shortcut: secondary viewports' passes belong to the ImGui backend; target switches and copies return false there.

static VkCommandBuffer ImPlatform_Vulkan_CallbackCommandBuffer()
{
    ImGui_ImplVulkan_RenderState* render_state = (ImGui_ImplVulkan_RenderState*)ImGui::GetPlatformIO().Renderer_RenderState;
    return render_state && !g_ActivePasses.empty() ? render_state->CommandBuffer : VK_NULL_HANDLE;
}

// Begins the innermost active pass again after it was ended, keeping its content.
static void ImPlatform_Vulkan_ResumePass(VkCommandBuffer cmd)
{
    const ImPlatform_ActivePass_Vulkan& p = g_ActivePasses.back();
    VkRenderPassBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = ImPlatform_PassLoadRenderPass(p.format, p.restLayout);
    info.framebuffer = p.framebuffer;
    info.renderArea.extent.width = p.width;
    info.renderArea.extent.height = p.height;
    vkCmdBeginRenderPass(cmd, &info, VK_SUBPASS_CONTENTS_INLINE);
    const VkViewport viewport = { 0.0f, 0.0f, (float)p.width, (float)p.height, 0.0f, 1.0f };
    const VkRect2D scissor = { { 0, 0 }, { p.width, p.height } };
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);      // Dear ImGui sets the scissor of each of its draws again.
    g_CurrentRenderPass = p.renderPass;
}

IMPLATFORM_API bool ImPlatform_PushRenderTarget(ImTextureID texture, bool clear)
{
    VkCommandBuffer cmd = ImPlatform_Vulkan_CallbackCommandBuffer();
    ImPlatform_RTTracking_Vulkan* rt = ImPlatform_FindRT_Vulkan(texture);
    if (cmd == VK_NULL_HANDLE || !rt)
        return false;
    for (const ImPlatform_ActivePass_Vulkan& p : g_ActivePasses)
        if (p.image == rt->image)
            return false;
    const bool clear_it = clear || !rt->rendered;      // Undefined content: cleared the first time.
    VkRenderPass render_pass = clear_it ? rt->renderPass : ImPlatform_PassLoadRenderPass(rt->format);
    if (render_pass == VK_NULL_HANDLE)
        return false;
    vkCmdEndRenderPass(cmd);
    VkMemoryBarrier barrier = {};      // Orders the target after its earlier sampling and drawing (see ImPlatform_RunRenderPass)
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    VkClearValue transparent = {};
    VkRenderPassBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = render_pass;
    info.framebuffer = rt->framebuffer;
    info.renderArea.extent.width = rt->width;
    info.renderArea.extent.height = rt->height;
    info.clearValueCount = clear_it ? 1 : 0;
    info.pClearValues = &transparent;
    vkCmdBeginRenderPass(cmd, &info, VK_SUBPASS_CONTENTS_INLINE);
    const ImPlatform_ActivePass_Vulkan pushed = { rt->renderPass, rt->framebuffer, rt->image, rt->format, rt->width, rt->height,
                                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    g_ActivePasses.push_back(pushed);
    g_PushedTargetCount++;
    rt->rendered = true;
    const VkViewport viewport = { 0.0f, 0.0f, (float)rt->width, (float)rt->height, 0.0f, 1.0f };
    const VkRect2D scissor = { { 0, 0 }, { rt->width, rt->height } };
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    g_CurrentRenderPass = rt->renderPass;
    return true;
}

IMPLATFORM_API void ImPlatform_PopRenderTarget(void)
{
    VkCommandBuffer cmd = ImPlatform_Vulkan_CallbackCommandBuffer();
    if (cmd == VK_NULL_HANDLE || g_PushedTargetCount == 0)
        return;
    vkCmdEndRenderPass(cmd);
    g_ActivePasses.pop_back();
    g_PushedTargetCount--;
    ImPlatform_Vulkan_ResumePass(cmd);
}

// From a draw callback: copies the target being drawn (a pushed render target, a render pass's texture or the main
// viewport's swapchain image) into 'dst'. A blit converts the swapchain's format (BGRA on most platforms).
IMPLATFORM_API bool ImPlatform_CopyCurrentTarget(ImTextureID dst, int src_x, int src_y, int w, int h, int dst_x, int dst_y)
{
    VkCommandBuffer cmd = ImPlatform_Vulkan_CallbackCommandBuffer();
    ImPlatform_TexReg_Vulkan* d = ImPlatform_Vulkan_FindTexture(dst);
    if (cmd == VK_NULL_HANDLE || !d || d->image == g_ActivePasses.back().image)
        return false;
    const ImPlatform_ActivePass_Vulkan& src = g_ActivePasses.back();
    // Clipped to both images.
    if (src_x < 0) { dst_x -= src_x; w += src_x; src_x = 0; }
    if (src_y < 0) { dst_y -= src_y; h += src_y; src_y = 0; }
    if (dst_x < 0) { src_x -= dst_x; w += dst_x; dst_x = 0; }
    if (dst_y < 0) { src_y -= dst_y; h += dst_y; dst_y = 0; }
    const int max_w = (int)src.width - src_x < (int)d->width - dst_x ? (int)src.width - src_x : (int)d->width - dst_x;
    const int max_h = (int)src.height - src_y < (int)d->height - dst_y ? (int)src.height - src_y : (int)d->height - dst_y;
    if (w > max_w) w = max_w;
    if (h > max_h) h = max_h;
    if (w <= 0 || h <= 0)
        return false;

    vkCmdEndRenderPass(cmd);
    ImPlatform_Vulkan_ImageBarrier(cmd, src.image, src.restLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    ImPlatform_Vulkan_ImageBarrier(cmd, d->image, d->idleLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    VkImageBlit blit = {};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.layerCount = 1;
    blit.dstSubresource = blit.srcSubresource;
    blit.srcOffsets[0] = { src_x, src_y, 0 };
    blit.srcOffsets[1] = { src_x + w, src_y + h, 1 };
    blit.dstOffsets[0] = { dst_x, dst_y, 0 };
    blit.dstOffsets[1] = { dst_x + w, dst_y + h, 1 };
    vkCmdBlitImage(cmd, src.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, d->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
    ImPlatform_Vulkan_ImageBarrier(cmd, d->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, d->idleLayout,
                                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
    ImPlatform_Vulkan_ImageBarrier(cmd, src.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, src.restLayout, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                   VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    ImPlatform_Vulkan_ResumePass(cmd);
    return true;
}

IMPLATFORM_API void ImPlatform_GetCurrentTargetSize(unsigned int* width, unsigned int* height)
{
    const bool active = !g_ActivePasses.empty();
    if (width) *width = active ? g_ActivePasses.back().width : (unsigned int)g_MainWindowData.Width;
    if (height) *height = active ? g_ActivePasses.back().height : (unsigned int)g_MainWindowData.Height;
}

// Draws dst_rect (current target pixels) with the program, t0 its slot 0 texture, into the current target.
IMPLATFORM_API void ImPlatform_DrawQuadNow(ImPlatform_ShaderProgram program, ImVec4 dst_rect, ImVec4 uv_rect)
{
    ImPlatform_ShaderProgramData_Vulkan* program_data = (ImPlatform_ShaderProgramData_Vulkan*)program;
    VkCommandBuffer cmd = ImPlatform_Vulkan_CallbackCommandBuffer();
    if (!program_data || program_data->customLayout || cmd == VK_NULL_HANDLE)
        return;
    struct { ImDrawVert v[4]; uint16_t i[6]; } quad;
    const ImU32 white = IM_COL32_WHITE;
    quad.v[0].pos = ImVec2(dst_rect.x, dst_rect.y); quad.v[0].uv = ImVec2(uv_rect.x, uv_rect.y); quad.v[0].col = white;
    quad.v[1].pos = ImVec2(dst_rect.z, dst_rect.y); quad.v[1].uv = ImVec2(uv_rect.z, uv_rect.y); quad.v[1].col = white;
    quad.v[2].pos = ImVec2(dst_rect.z, dst_rect.w); quad.v[2].uv = ImVec2(uv_rect.z, uv_rect.w); quad.v[2].col = white;
    quad.v[3].pos = ImVec2(dst_rect.x, dst_rect.w); quad.v[3].uv = ImVec2(uv_rect.x, uv_rect.w); quad.v[3].col = white;
    const uint16_t indices[6] = { 0, 1, 2, 0, 2, 3 };
    memcpy(quad.i, indices, sizeof(indices));
    uint32_t offset = 0;
    if (!ImPlatform_Vulkan_WriteConstants(cmd, &quad, (uint32_t)sizeof(quad), &offset))     // A constant-ring block holds both
        return;
    float mvp[4][4];     // b0 in the target's pixels, as the Direct3D 11 backend projects it
    ImPlatform_Vulkan_Projection(0.0f, 0.0f, (float)g_ActivePasses.back().width, (float)g_ActivePasses.back().height, mvp);
    g_BoundShaderProgram = NULL;
    ImPlatform_Vulkan_BindProgram(program_data, cmd, ImPlatform_Vulkan_FindDrawData(NULL), VK_NULL_HANDLE, NULL, 0, &mvp[0][0]);
    if (g_BoundShaderProgram != program_data)
        return;
    const VkDeviceSize vertex_offset = offset;
    vkCmdBindVertexBuffers(cmd, 0, 1, &g_ConstantRing.buffer, &vertex_offset);
    vkCmdBindIndexBuffer(cmd, g_ConstantRing.buffer, offset + (VkDeviceSize)offsetof(decltype(quad), i), VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
}

// Draws the bound buffers inside a draw callback with the program bound last there (ImPlatform_BeginCustomShader,
// ImPlatform_BeginCustomShader_Render), Dear ImGui's blending, into the current target. primitive_type: 0 triangles,
// 1 lines, 2 points.
IMPLATFORM_API void ImPlatform_DrawIndexed(unsigned int primitive_type, unsigned int index_count, unsigned int start_index)
{
    ImPlatform_ShaderProgramData_Vulkan* program_data = g_BoundShaderProgram;
    ImPlatform_BufferData_Vulkan* vb = g_BoundVertexBuffer;
    ImPlatform_BufferData_Vulkan* ib = g_BoundIndexBuffer;
    ImGui_ImplVulkan_RenderState* render_state = (ImGui_ImplVulkan_RenderState*)ImGui::GetPlatformIO().Renderer_RenderState;
    VkCommandBuffer cmd = render_state ? render_state->CommandBuffer : VK_NULL_HANDLE;
    if (cmd == VK_NULL_HANDLE || !program_data || !vb || vb->index || !ib || !ib->index || primitive_type > 2
        || start_index > ib->ib_desc.index_count)
        return;
    if (index_count == 0)
        index_count = ib->ib_desc.index_count - start_index;    // The rest of the buffer
    if ((VkDeviceSize)start_index + index_count > ib->ib_desc.index_count)
        return;
    g_BoundShaderProgram = NULL;
    ImPlatform_Vulkan_BindProgram(program_data, cmd, ImPlatform_Vulkan_FindDrawData(NULL), VK_NULL_HANDLE, &vb->vb_desc, primitive_type);
    if (g_BoundShaderProgram != program_data)
        return;
    const VkDeviceSize zero = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb->buffer, &zero);
    vkCmdBindIndexBuffer(cmd, ib->buffer, 0, ib->ib_desc.format == ImPlatform_IndexFormat_UInt16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, index_count, 1, start_index, 0, 0);
}

#endif // IM_GFX_VULKAN
