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
static VkAllocationCallbacks* g_Allocator = NULL;
static ImGui_ImplVulkanH_Window g_MainWindowData;  // Don't use = {} - let constructor run!
static bool g_SwapChainRebuild = false;
static uint32_t g_QueueFamily = (uint32_t)-1;

// Uniform block API state
static ImPlatform_ShaderProgram g_CurrentUniformBlockProgram = nullptr;
static void* g_UniformBlockData = nullptr;
static size_t g_UniformBlockSize = 0;

// Current command buffer for custom shader rendering
static VkCommandBuffer g_CurrentCommandBuffer = VK_NULL_HANDLE;

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
        VkInstanceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

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
        const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
        create_info.enabledLayerCount = 1;
        create_info.ppEnabledLayerNames = layers;
        extensions[ext_idx++] = "VK_EXT_debug_report";
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
        const char* device_extensions[] = { "VK_KHR_swapchain" };
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
    {
        VkDescriptorPoolSize pool_sizes[] =
        {
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000;
        pool_info.poolSizeCount = (uint32_t)(sizeof(pool_sizes) / sizeof(pool_sizes[0]));
        pool_info.pPoolSizes = pool_sizes;
        err = vkCreateDescriptorPool(g_GfxData.device, &pool_info, g_Allocator, &g_GfxData.descriptorPool);
        check_vk_result(err);
    }

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

    // Setup window data and select surface format/present mode
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
    g_MainWindowData.SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(pData->physicalDevice, g_MainWindowData.Surface, requestSurfaceImageFormat, 4, requestSurfaceColorSpace);

    // Select Present Mode
#ifdef IMPLATFORM_VULKAN_UNLIMITED_FRAMERATE
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
    g_MainWindowData.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(pData->physicalDevice, g_MainWindowData.Surface, present_modes, 3);
#else
    // FIFO is always supported (vsync enabled)
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
    g_MainWindowData.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(pData->physicalDevice, g_MainWindowData.Surface, present_modes, 1);
#endif

    // Create SwapChain, RenderPass, Framebuffer, etc.
    ImGui_ImplVulkanH_CreateOrResizeWindow(pData->instance, pData->physicalDevice, pData->device,
        &g_MainWindowData, g_QueueFamily, g_Allocator, 1280, 720, pData->minImageCount, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

    return true;
}
#endif

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
bool ImPlatform_Gfx_CreateDevice_Vulkan(void* pWindow, ImPlatform_GfxData_Vulkan* pData)
{
    GLFWwindow* window = (GLFWwindow*)pWindow;

    // Get required extensions from GLFW
    uint32_t extensions_count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&extensions_count);

    if (!SetupVulkan(extensions, extensions_count))
        return false;

    // Create Window Surface
    VkResult err = glfwCreateWindowSurface(pData->instance, window, g_Allocator, &pData->surface);
    check_vk_result(err);

    // Get framebuffer size
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);

    // Setup window data and select surface format/present mode
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
    g_MainWindowData.SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(pData->physicalDevice, g_MainWindowData.Surface, requestSurfaceImageFormat, 4, requestSurfaceColorSpace);

    // Select Present Mode
#ifdef IMPLATFORM_VULKAN_UNLIMITED_FRAMERATE
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
    g_MainWindowData.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(pData->physicalDevice, g_MainWindowData.Surface, present_modes, 3);
#else
    // FIFO is always supported (vsync enabled)
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
    g_MainWindowData.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(pData->physicalDevice, g_MainWindowData.Surface, present_modes, 1);
#endif

    // Create SwapChain, RenderPass, Framebuffer, etc.
    ImGui_ImplVulkanH_CreateOrResizeWindow(pData->instance, pData->physicalDevice, pData->device,
        &g_MainWindowData, g_QueueFamily, g_Allocator, w, h, pData->minImageCount, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

    return true;
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

    // Get window size
    int w, h;
    SDL_GetWindowSize(window, &w, &h);

    // Setup window data and select surface format/present mode
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
    g_MainWindowData.SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(pData->physicalDevice, g_MainWindowData.Surface, requestSurfaceImageFormat, 4, requestSurfaceColorSpace);

    // Select Present Mode
#ifdef IMPLATFORM_VULKAN_UNLIMITED_FRAMERATE
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
    g_MainWindowData.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(pData->physicalDevice, g_MainWindowData.Surface, present_modes, 3);
#else
    // FIFO is always supported (vsync enabled)
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
    g_MainWindowData.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(pData->physicalDevice, g_MainWindowData.Surface, present_modes, 1);
#endif

    // Create SwapChain, RenderPass, Framebuffer, etc.
    ImGui_ImplVulkanH_CreateOrResizeWindow(pData->instance, pData->physicalDevice, pData->device,
        &g_MainWindowData, g_QueueFamily, g_Allocator, w, h, pData->minImageCount, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

    return true;
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
    if (!SDL_Vulkan_CreateSurface(window, pData->instance, NULL, &pData->surface))
        return false;

    // Get window size
    int w, h;
    SDL_GetWindowSize(window, &w, &h);

    // Setup window data and select surface format/present mode
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
    g_MainWindowData.SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(pData->physicalDevice, g_MainWindowData.Surface, requestSurfaceImageFormat, 4, requestSurfaceColorSpace);

    // Select Present Mode
#ifdef IMPLATFORM_VULKAN_UNLIMITED_FRAMERATE
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
    g_MainWindowData.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(pData->physicalDevice, g_MainWindowData.Surface, present_modes, 3);
#else
    // FIFO is always supported (vsync enabled)
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
    g_MainWindowData.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(pData->physicalDevice, g_MainWindowData.Surface, present_modes, 1);
#endif

    // Create SwapChain, RenderPass, Framebuffer, etc.
    ImGui_ImplVulkanH_CreateOrResizeWindow(pData->instance, pData->physicalDevice, pData->device,
        &g_MainWindowData, g_QueueFamily, g_Allocator, w, h, pData->minImageCount, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

    return true;
}
#endif

// Internal API - Cleanup Vulkan device
void ImPlatform_Gfx_CleanupDevice_Vulkan(ImPlatform_GfxData_Vulkan* pData)
{
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

// ImPlatform API - InitGfx
IMPLATFORM_API bool ImPlatform_InitGfx(void)
{
    // Setup ImGui Vulkan backend
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = g_GfxData.instance;
    init_info.PhysicalDevice = g_GfxData.physicalDevice;
    init_info.Device = g_GfxData.device;
    init_info.QueueFamily = g_QueueFamily;
    init_info.Queue = g_GfxData.queue;
    init_info.PipelineCache = g_GfxData.pipelineCache;
    init_info.DescriptorPool = g_GfxData.descriptorPool;
    init_info.MinImageCount = g_GfxData.minImageCount;
    init_info.ImageCount = g_MainWindowData.ImageCount;
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

    // Create a default white 1x1 texture for custom shaders
    // This will be used as the default texture (binding 0) in custom shader descriptor sets
    g_GfxData.defaultSampler = VK_NULL_HANDLE;
    g_GfxData.defaultImageView = VK_NULL_HANDLE;
    g_GfxData.defaultImage = VK_NULL_HANDLE;
    g_GfxData.defaultImageMemory = VK_NULL_HANDLE;

    // Create default white 1x1 texture
    {
        VkResult err;

        // Create image
        VkImageCreateInfo image_info = {};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
        image_info.extent.width = 1;
        image_info.extent.height = 1;
        image_info.extent.depth = 1;
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        err = vkCreateImage(g_GfxData.device, &image_info, g_Allocator, &g_GfxData.defaultImage);
        check_vk_result(err);

        // Allocate memory
        VkMemoryRequirements mem_reqs;
        vkGetImageMemoryRequirements(g_GfxData.device, g_GfxData.defaultImage, &mem_reqs);

        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_reqs.size;
        alloc_info.memoryTypeIndex = 0; // Find device local memory
        VkPhysicalDeviceMemoryProperties mem_props;
        vkGetPhysicalDeviceMemoryProperties(g_GfxData.physicalDevice, &mem_props);
        for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++)
        {
            if ((mem_reqs.memoryTypeBits & (1 << i)) &&
                (mem_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
            {
                alloc_info.memoryTypeIndex = i;
                break;
            }
        }

        err = vkAllocateMemory(g_GfxData.device, &alloc_info, g_Allocator, &g_GfxData.defaultImageMemory);
        check_vk_result(err);

        err = vkBindImageMemory(g_GfxData.device, g_GfxData.defaultImage, g_GfxData.defaultImageMemory, 0);
        check_vk_result(err);

        // Create image view
        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = g_GfxData.defaultImage;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = VK_FORMAT_R8G8B8A8_UNORM;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.layerCount = 1;

        err = vkCreateImageView(g_GfxData.device, &view_info, g_Allocator, &g_GfxData.defaultImageView);
        check_vk_result(err);

        // Create sampler
        VkSamplerCreateInfo sampler_info = {};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.minLod = -1000;
        sampler_info.maxLod = 1000;
        sampler_info.maxAnisotropy = 1.0f;

        err = vkCreateSampler(g_GfxData.device, &sampler_info, g_Allocator, &g_GfxData.defaultSampler);
        check_vk_result(err);

        // Upload white pixel data (we need to create a staging buffer and copy)
        // For simplicity, we'll transition the image to the correct layout but skip the upload
        // The image will contain undefined data, but that's okay for now - we'll fix this later
        // TODO: Upload actual white pixel data using a staging buffer

        // Transition image layout to shader read
        VkCommandBuffer cmd_buf = g_MainWindowData.Frames[g_MainWindowData.FrameIndex].CommandBuffer;

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = g_GfxData.defaultImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        // Note: This transition should ideally happen in a one-time command buffer
        // For now, we'll leave it and the image will be transitioned on first use
    }

    return true;
}

// ImPlatform API - GfxCheck
IMPLATFORM_API bool ImPlatform_GfxCheck(void)
{
    // Handle swapchain rebuild
    if (g_SwapChainRebuild)
    {
        int width, height;
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
        RECT rect;
        GetClientRect(ImPlatform_App_GetHWND(), &rect);
        width = rect.right - rect.left;
        height = rect.bottom - rect.top;
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
        glfwGetFramebufferSize(ImPlatform_App_GetGLFWWindow(), &width, &height);
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
        SDL_GetWindowSize(ImPlatform_App_GetSDL2Window(), &width, &height);
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
        SDL_GetWindowSize(ImPlatform_App_GetSDL3Window(), &width, &height);
#endif

        if (width > 0 && height > 0)
        {
            ImGui_ImplVulkan_SetMinImageCount(g_GfxData.minImageCount);
            ImGui_ImplVulkanH_CreateOrResizeWindow(g_GfxData.instance, g_GfxData.physicalDevice, g_GfxData.device,
                &g_MainWindowData, g_QueueFamily, g_Allocator, width, height, g_GfxData.minImageCount, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
            g_MainWindowData.FrameIndex = 0;
            g_SwapChainRebuild = false;
        }
    }

    return true;
}

// ImPlatform API - GfxAPINewFrame
IMPLATFORM_API void ImPlatform_GfxAPINewFrame(void)
{
    ImGui_ImplVulkan_NewFrame();
}

// ImPlatform API - GfxAPIClear
IMPLATFORM_API bool ImPlatform_GfxAPIClear(ImVec4 const vClearColor)
{
    ImGui::Render();

    // Acquire next image using new semaphore API
    VkSemaphore image_acquired_semaphore = g_MainWindowData.FrameSemaphores[g_MainWindowData.SemaphoreIndex].ImageAcquiredSemaphore;
    VkResult err = vkAcquireNextImageKHR(g_GfxData.device, g_MainWindowData.Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &g_MainWindowData.FrameIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
    {
        g_SwapChainRebuild = true;
        return false;
    }
    if (err == VK_ERROR_OUT_OF_DATE_KHR)
        return false;
    check_vk_result(err);

    // Wait for fence from previous frame
    ImGui_ImplVulkanH_Frame* fd = &g_MainWindowData.Frames[g_MainWindowData.FrameIndex];
    err = vkWaitForFences(g_GfxData.device, 1, &fd->Fence, VK_TRUE, UINT64_MAX);
    check_vk_result(err);

    err = vkResetFences(g_GfxData.device, 1, &fd->Fence);
    check_vk_result(err);

    err = vkResetCommandPool(g_GfxData.device, fd->CommandPool, 0);
    check_vk_result(err);

    VkCommandBufferBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
    check_vk_result(err);

    VkRenderPassBeginInfo render_info = {};
    render_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_info.renderPass = g_MainWindowData.RenderPass;
    render_info.framebuffer = fd->Framebuffer;
    render_info.renderArea.extent.width = g_MainWindowData.Width;
    render_info.renderArea.extent.height = g_MainWindowData.Height;
    render_info.clearValueCount = 1;
    VkClearValue clearValue = {};
    clearValue.color.float32[0] = vClearColor.x * vClearColor.w;
    clearValue.color.float32[1] = vClearColor.y * vClearColor.w;
    clearValue.color.float32[2] = vClearColor.z * vClearColor.w;
    clearValue.color.float32[3] = vClearColor.w;
    render_info.pClearValues = &clearValue;
    vkCmdBeginRenderPass(fd->CommandBuffer, &render_info, VK_SUBPASS_CONTENTS_INLINE);

    return true;
}

// ImPlatform API - GfxAPIRender
IMPLATFORM_API bool ImPlatform_GfxAPIRender(ImVec4 const vClearColor)
{
    (void)vClearColor;

    ImGui_ImplVulkanH_Frame* fd = &g_MainWindowData.Frames[g_MainWindowData.FrameIndex];

    // Store command buffer for custom shader callbacks
    g_CurrentCommandBuffer = fd->CommandBuffer;

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), fd->CommandBuffer);

    // Clear command buffer reference
    g_CurrentCommandBuffer = VK_NULL_HANDLE;

    vkCmdEndRenderPass(fd->CommandBuffer);

    VkResult err = vkEndCommandBuffer(fd->CommandBuffer);
    check_vk_result(err);

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
IMPLATFORM_API bool ImPlatform_GfxAPISwapBuffer(void)
{
    if (g_SwapChainRebuild)
        return false;

    ImGui_ImplVulkanH_Frame* fd = &g_MainWindowData.Frames[g_MainWindowData.FrameIndex];
    VkSemaphore image_acquired_semaphore = g_MainWindowData.FrameSemaphores[g_MainWindowData.SemaphoreIndex].ImageAcquiredSemaphore;
    VkSemaphore render_complete_semaphore = g_MainWindowData.FrameSemaphores[g_MainWindowData.SemaphoreIndex].RenderCompleteSemaphore;

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

    VkResult err = vkQueueSubmit(g_GfxData.queue, 1, &info, fd->Fence);
    check_vk_result(err);

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &render_complete_semaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &g_MainWindowData.Swapchain;
    present_info.pImageIndices = &g_MainWindowData.FrameIndex;
    err = vkQueuePresentKHR(g_GfxData.queue, &present_info);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
    {
        g_SwapChainRebuild = true;
        return true;
    }
    if (err == VK_ERROR_OUT_OF_DATE_KHR)
        return false;
    if (err != VK_SUBOPTIMAL_KHR)
        check_vk_result(err);

    // Advance semaphore index (critical for new API)
    g_MainWindowData.SemaphoreIndex = (g_MainWindowData.SemaphoreIndex + 1) % g_MainWindowData.SemaphoreCount;

    return true;
}

// ImPlatform API - ShutdownGfxAPI
IMPLATFORM_API void ImPlatform_ShutdownGfxAPI(void)
{
    vkDeviceWaitIdle(g_GfxData.device);
}

// ImPlatform API - ShutdownWindow
IMPLATFORM_API void ImPlatform_ShutdownWindow(void)
{
    // Clean up default texture resources
    if (g_GfxData.defaultSampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(g_GfxData.device, g_GfxData.defaultSampler, g_Allocator);
        g_GfxData.defaultSampler = VK_NULL_HANDLE;
    }
    if (g_GfxData.defaultImageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(g_GfxData.device, g_GfxData.defaultImageView, g_Allocator);
        g_GfxData.defaultImageView = VK_NULL_HANDLE;
    }
    if (g_GfxData.defaultImage != VK_NULL_HANDLE)
    {
        vkDestroyImage(g_GfxData.device, g_GfxData.defaultImage, g_Allocator);
        g_GfxData.defaultImage = VK_NULL_HANDLE;
    }
    if (g_GfxData.defaultImageMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(g_GfxData.device, g_GfxData.defaultImageMemory, g_Allocator);
        g_GfxData.defaultImageMemory = VK_NULL_HANDLE;
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplVulkanH_DestroyWindow(g_GfxData.instance, g_GfxData.device, &g_MainWindowData, g_Allocator);
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

IMPLATFORM_API ImTextureID ImPlatform_CreateTexture(const void* pixel_data, const ImPlatform_TextureDesc* desc)
{
    if (!desc || !pixel_data || !g_GfxData.device)
        return NULL;

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
            return NULL;

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
            return NULL;
        }

        err = vkBindBufferMemory(g_GfxData.device, staging_buffer, staging_memory, 0);
        if (err != VK_SUCCESS)
        {
            vkDestroyBuffer(g_GfxData.device, staging_buffer, g_Allocator);
            vkFreeMemory(g_GfxData.device, staging_memory, g_Allocator);
            return NULL;
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
            return NULL;
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
            return NULL;
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
        image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        err = vkCreateImage(g_GfxData.device, &image_info, g_Allocator, &image);
        if (err != VK_SUCCESS)
        {
            vkDestroyBuffer(g_GfxData.device, staging_buffer, g_Allocator);
            vkFreeMemory(g_GfxData.device, staging_memory, g_Allocator);
            return NULL;
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
            return NULL;
        }

        err = vkBindImageMemory(g_GfxData.device, image, image_memory, 0);
        if (err != VK_SUCCESS)
        {
            vkFreeMemory(g_GfxData.device, image_memory, g_Allocator);
            vkDestroyImage(g_GfxData.device, image, g_Allocator);
            vkDestroyBuffer(g_GfxData.device, staging_buffer, g_Allocator);
            vkFreeMemory(g_GfxData.device, staging_memory, g_Allocator);
            return NULL;
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
            return NULL;
        }
    }

    // Create sampler
    VkSampler sampler;
    {
        VkSamplerCreateInfo sampler_info = {};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = (desc->mag_filter == ImPlatform_TextureFilter_Nearest) ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
        sampler_info.minFilter = (desc->min_filter == ImPlatform_TextureFilter_Nearest) ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

        VkSamplerAddressMode wrap_u = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        VkSamplerAddressMode wrap_v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        if (desc->wrap_u == ImPlatform_TextureWrap_Repeat)
            wrap_u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        else if (desc->wrap_u == ImPlatform_TextureWrap_Mirror)
            wrap_u = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        if (desc->wrap_v == ImPlatform_TextureWrap_Repeat)
            wrap_v = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        else if (desc->wrap_v == ImPlatform_TextureWrap_Mirror)
            wrap_v = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;

        sampler_info.addressModeU = wrap_u;
        sampler_info.addressModeV = wrap_v;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.minLod = -1000;
        sampler_info.maxLod = 1000;
        sampler_info.maxAnisotropy = 1.0f;
        err = vkCreateSampler(g_GfxData.device, &sampler_info, g_Allocator, &sampler);
        if (err != VK_SUCCESS)
        {
            vkDestroyImageView(g_GfxData.device, image_view, g_Allocator);
            vkFreeMemory(g_GfxData.device, image_memory, g_Allocator);
            vkDestroyImage(g_GfxData.device, image, g_Allocator);
            vkDestroyBuffer(g_GfxData.device, staging_buffer, g_Allocator);
            vkFreeMemory(g_GfxData.device, staging_memory, g_Allocator);
            return NULL;
        }
    }

    // Create descriptor set
    VkDescriptorSet descriptor_set = (VkDescriptorSet)ImGui_ImplVulkan_AddTexture(sampler, image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Upload to GPU using command buffer
    {
        VkCommandBuffer command_buffer = g_MainWindowData.Frames[g_MainWindowData.FrameIndex].CommandBuffer;

        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(command_buffer, &begin_info);
        if (err != VK_SUCCESS)
        {
            // Cleanup and return null
            ImGui_ImplVulkan_RemoveTexture(descriptor_set);
            vkDestroySampler(g_GfxData.device, sampler, g_Allocator);
            vkDestroyImageView(g_GfxData.device, image_view, g_Allocator);
            vkFreeMemory(g_GfxData.device, image_memory, g_Allocator);
            vkDestroyImage(g_GfxData.device, image, g_Allocator);
            vkDestroyBuffer(g_GfxData.device, staging_buffer, g_Allocator);
            vkFreeMemory(g_GfxData.device, staging_memory, g_Allocator);
            return NULL;
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

    // Note: We're leaking image and image_memory here as we have no way to track them
    // A production implementation would need a texture registry
    // The descriptor set is the handle we return

    return (ImTextureID)descriptor_set;
}

IMPLATFORM_API bool ImPlatform_UpdateTexture(ImTextureID texture_id, const void* pixel_data,
                                              unsigned int x, unsigned int y,
                                              unsigned int width, unsigned int height)
{
    // Vulkan texture updates require staging buffers and command lists
    // This is complex and not implemented yet
    return false;
}

IMPLATFORM_API void ImPlatform_DestroyTexture(ImTextureID texture_id)
{
    if (!texture_id)
        return;

    // Remove from ImGui's texture registry
    VkDescriptorSet descriptor_set = (VkDescriptorSet)texture_id;
    ImGui_ImplVulkan_RemoveTexture(descriptor_set);

    // Note: We're not cleaning up the image, image_memory, image_view, or sampler
    // because we don't track them. A production implementation would need proper tracking.
}

// ============================================================================
// Custom Vertex/Index Buffer Management API - Vulkan (Stubs)
// ============================================================================

IMPLATFORM_API ImPlatform_VertexBuffer ImPlatform_CreateVertexBuffer(const void* vertex_data, const ImPlatform_VertexBufferDesc* desc) { return NULL; }
IMPLATFORM_API bool ImPlatform_UpdateVertexBuffer(ImPlatform_VertexBuffer vertex_buffer, const void* vertex_data, unsigned int vertex_count, unsigned int offset) { return false; }
IMPLATFORM_API void ImPlatform_DestroyVertexBuffer(ImPlatform_VertexBuffer vertex_buffer) {}
IMPLATFORM_API ImPlatform_IndexBuffer ImPlatform_CreateIndexBuffer(const void* index_data, const ImPlatform_IndexBufferDesc* desc) { return NULL; }
IMPLATFORM_API bool ImPlatform_UpdateIndexBuffer(ImPlatform_IndexBuffer index_buffer, const void* index_data, unsigned int index_count, unsigned int offset) { return false; }
IMPLATFORM_API void ImPlatform_DestroyIndexBuffer(ImPlatform_IndexBuffer index_buffer) {}
IMPLATFORM_API void ImPlatform_BindVertexBuffer(ImPlatform_VertexBuffer vertex_buffer) {}
IMPLATFORM_API void ImPlatform_BindIndexBuffer(ImPlatform_IndexBuffer index_buffer) {}
IMPLATFORM_API void ImPlatform_DrawIndexed(unsigned int primitive_type, unsigned int index_count, unsigned int start_index) {}

// ============================================================================
// Custom Shader System - Vulkan
// ============================================================================

// Shader data structures
struct ImPlatform_ShaderData_Vulkan
{
    VkShaderModule shaderModule;
    ImPlatform_ShaderStage stage;
};

struct ImPlatform_ShaderProgramData_Vulkan
{
    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;
    VkBuffer uniformBuffer;
    VkDeviceMemory uniformBufferMemory;
    void* uniformBufferMapped;
    size_t uniformBufferSize;
    bool uniformBufferDirty;
};

// Custom Shader System API - Vulkan
IMPLATFORM_API ImPlatform_Shader ImPlatform_CreateShader(const ImPlatform_ShaderDesc* desc)
{
    if (!desc || !desc->bytecode || desc->bytecode_size == 0)
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

    // Create shader module from SPIR-V bytecode
    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = desc->bytecode_size;
    create_info.pCode = (const uint32_t*)desc->bytecode;

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

    if (shader_data->shaderModule)
        vkDestroyShaderModule(g_GfxData.device, shader_data->shaderModule, g_Allocator);

    free(shader_data);
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

    ImPlatform_ShaderProgramData_Vulkan* program_data = (ImPlatform_ShaderProgramData_Vulkan*)malloc(sizeof(ImPlatform_ShaderProgramData_Vulkan));
    if (!program_data)
        return NULL;

    memset(program_data, 0, sizeof(ImPlatform_ShaderProgramData_Vulkan));
    program_data->vertShaderModule = vs_data->shaderModule;
    program_data->fragShaderModule = fs_data->shaderModule;

    VkResult err;

    // Create descriptor set layout for uniforms and textures
    // This layout matches ImGui's expectations:
    // - Binding 0: Combined image sampler (texture)
    // - Binding 1: Uniform buffer (our custom parameters)
    {
        VkDescriptorSetLayoutBinding bindings[1] = {};

        // Binding 0: Texture sampler (compatible with ImGui)
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        // Binding 1: Uniform buffer for custom parameters
        // bindings[1].binding = 1;
        // bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        // bindings[1].descriptorCount = 1;
        // bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = 1;
        layout_info.pBindings = bindings;

        err = vkCreateDescriptorSetLayout(g_GfxData.device, &layout_info, g_Allocator, &program_data->descriptorSetLayout);
        if (err != VK_SUCCESS)
        {
            fprintf(stderr, "[ImPlatform] Vulkan: Failed to create descriptor set layout (VkResult = %d)\n", err);
            free(program_data);
            return NULL;
        }
    }

    // Create pipeline layout
    // Uses push constants for projection matrix (compatible with ImGui)
    {
        VkDescriptorSetLayout set_layouts[1] = { program_data->descriptorSetLayout };

        VkPushConstantRange push_constants = {};
        push_constants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        push_constants.offset = 0;
        push_constants.size = 128; // Projection matrix (64 bytes) + custom uniforms (64 bytes max)

        VkPipelineLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = set_layouts;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = &push_constants;

        err = vkCreatePipelineLayout(g_GfxData.device, &layout_info, g_Allocator, &program_data->pipelineLayout);
        if (err != VK_SUCCESS)
        {
            fprintf(stderr, "[ImPlatform] Vulkan: Failed to create pipeline layout (VkResult = %d)\n", err);
            vkDestroyDescriptorSetLayout(g_GfxData.device, program_data->descriptorSetLayout, g_Allocator);
            free(program_data);
            return NULL;
        }
    }

    // Create graphics pipeline
    {
        VkPipelineShaderStageCreateInfo stage[2] = {};
        stage[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stage[0].module = program_data->vertShaderModule;
        stage[0].pName = "main";
        stage[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stage[1].module = program_data->fragShaderModule;
        stage[1].pName = "main";

        // Vertex input: same as ImGui (ImDrawVert)
        VkVertexInputBindingDescription binding_desc = {};
        binding_desc.stride = sizeof(ImDrawVert);
        binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attribute_desc[3] = {};
        attribute_desc[0].location = 0;
        attribute_desc[0].binding = 0;
        attribute_desc[0].format = VK_FORMAT_R32G32_SFLOAT;
        attribute_desc[0].offset = offsetof(ImDrawVert, pos);
        attribute_desc[1].location = 1;
        attribute_desc[1].binding = 0;
        attribute_desc[1].format = VK_FORMAT_R32G32_SFLOAT;
        attribute_desc[1].offset = offsetof(ImDrawVert, uv);
        attribute_desc[2].location = 2;
        attribute_desc[2].binding = 0;
        attribute_desc[2].format = VK_FORMAT_R8G8B8A8_UNORM;
        attribute_desc[2].offset = offsetof(ImDrawVert, col);

        VkPipelineVertexInputStateCreateInfo vertex_info = {};
        vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_info.vertexBindingDescriptionCount = 1;
        vertex_info.pVertexBindingDescriptions = &binding_desc;
        vertex_info.vertexAttributeDescriptionCount = 3;
        vertex_info.pVertexAttributeDescriptions = attribute_desc;

        VkPipelineInputAssemblyStateCreateInfo ia_info = {};
        ia_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

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
        pipeline_info.flags = 0; // No special flags needed
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
        pipeline_info.renderPass = g_MainWindowData.RenderPass;
        pipeline_info.subpass = 0;

        err = vkCreateGraphicsPipelines(g_GfxData.device, VK_NULL_HANDLE, 1, &pipeline_info, g_Allocator, &program_data->pipeline);
        if (err != VK_SUCCESS)
        {
            fprintf(stderr, "[ImPlatform] Vulkan: Failed to create graphics pipeline (VkResult = %d)\n", err);
            vkDestroyPipelineLayout(g_GfxData.device, program_data->pipelineLayout, g_Allocator);
            vkDestroyDescriptorSetLayout(g_GfxData.device, program_data->descriptorSetLayout, g_Allocator);
            free(program_data);
            return NULL;
        }
    }

    // Create descriptor pool and initial descriptor set
    // This ensures we always have a valid descriptor set even if uniforms are never set
    {
        // Create descriptor pool
        VkDescriptorPoolSize pool_sizes[1] = {};
        pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        pool_sizes[0].descriptorCount = 1;
        // pool_sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        // pool_sizes[1].descriptorCount = 1;

        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.poolSizeCount = 1;
        pool_info.pPoolSizes = pool_sizes;
        pool_info.maxSets = 1;

        err = vkCreateDescriptorPool(g_GfxData.device, &pool_info, g_Allocator, &program_data->descriptorPool);
        if (err != VK_SUCCESS)
        {
            fprintf(stderr, "[ImPlatform] Vulkan: Failed to create descriptor pool (VkResult = %d)\n", err);
            vkDestroyPipeline(g_GfxData.device, program_data->pipeline, g_Allocator);
            vkDestroyPipelineLayout(g_GfxData.device, program_data->pipelineLayout, g_Allocator);
            vkDestroyDescriptorSetLayout(g_GfxData.device, program_data->descriptorSetLayout, g_Allocator);
            free(program_data);
            return NULL;
        }

        // Allocate descriptor set
        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = program_data->descriptorPool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &program_data->descriptorSetLayout;

        err = vkAllocateDescriptorSets(g_GfxData.device, &alloc_info, &program_data->descriptorSet);
        if (err != VK_SUCCESS)
        {
            fprintf(stderr, "[ImPlatform] Vulkan: Failed to allocate descriptor set (VkResult = %d)\n", err);
            vkDestroyDescriptorPool(g_GfxData.device, program_data->descriptorPool, g_Allocator);
            vkDestroyPipeline(g_GfxData.device, program_data->pipeline, g_Allocator);
            vkDestroyPipelineLayout(g_GfxData.device, program_data->pipelineLayout, g_Allocator);
            vkDestroyDescriptorSetLayout(g_GfxData.device, program_data->descriptorSetLayout, g_Allocator);
            free(program_data);
            return NULL;
        }

        // Initialize descriptor set with default texture (binding 0)
        // Binding 1 (uniform buffer) will be updated later via EndUniformBlock
        VkDescriptorImageInfo image_info = {};
        image_info.sampler = g_GfxData.defaultSampler;
        image_info.imageView = g_GfxData.defaultImageView;
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet descriptor_write = {};
        descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write.dstSet = program_data->descriptorSet;
        descriptor_write.dstBinding = 0;  // Binding 0 for texture
        descriptor_write.dstArrayElement = 0;
        descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_write.descriptorCount = 1;
        descriptor_write.pImageInfo = &image_info;

        vkUpdateDescriptorSets(g_GfxData.device, 1, &descriptor_write, 0, nullptr);

        // Debug: log successful descriptor set creation
        fprintf(stderr, "[ImPlatform] Vulkan: Created shader program with descriptor set: %p, pipeline layout: %p\n",
                (void*)program_data->descriptorSet, (void*)program_data->pipelineLayout);
    }

    return (ImPlatform_ShaderProgram)program_data;
}

IMPLATFORM_API void ImPlatform_DestroyShaderProgram(ImPlatform_ShaderProgram program)
{
    if (!program)
        return;

    ImPlatform_ShaderProgramData_Vulkan* program_data = (ImPlatform_ShaderProgramData_Vulkan*)program;

    if (program_data->pipeline)
        vkDestroyPipeline(g_GfxData.device, program_data->pipeline, g_Allocator);
    if (program_data->pipelineLayout)
        vkDestroyPipelineLayout(g_GfxData.device, program_data->pipelineLayout, g_Allocator);
    if (program_data->descriptorSet && program_data->descriptorPool)
        vkFreeDescriptorSets(g_GfxData.device, program_data->descriptorPool, 1, &program_data->descriptorSet);
    if (program_data->descriptorPool)
        vkDestroyDescriptorPool(g_GfxData.device, program_data->descriptorPool, g_Allocator);
    if (program_data->descriptorSetLayout)
        vkDestroyDescriptorSetLayout(g_GfxData.device, program_data->descriptorSetLayout, g_Allocator);
    // Free uniform data buffer (used for push constants, not Vulkan buffers)
    if (program_data->uniformBufferMapped)
        free(program_data->uniformBufferMapped);

    free(program_data);
}

IMPLATFORM_API void ImPlatform_UseShaderProgram(ImPlatform_ShaderProgram program)
{
    // Not needed for Vulkan - shaders are bound through draw callbacks
}

IMPLATFORM_API bool ImPlatform_SetShaderUniform(ImPlatform_ShaderProgram program, const char* name, const void* data, unsigned int size)
{
    // Not needed for Vulkan - uniforms are set through uniform blocks
    return false;
}

IMPLATFORM_API bool ImPlatform_SetShaderTexture(ImPlatform_ShaderProgram program, const char* name, unsigned int slot, ImTextureID texture)
{
    // Not needed for Vulkan - textures are bound through descriptor sets
    return false;
}

IMPLATFORM_API void ImPlatform_BeginUniformBlock(ImPlatform_ShaderProgram program)
{
    if (!program)
        return;

    g_CurrentUniformBlockProgram = program;

    // Free any previous block data
    if (g_UniformBlockData)
    {
        free(g_UniformBlockData);
        g_UniformBlockData = nullptr;
        g_UniformBlockSize = 0;
    }
}

IMPLATFORM_API bool ImPlatform_SetUniform(const char* name, const void* data, unsigned int size)
{
    if (!g_CurrentUniformBlockProgram || !data || size == 0)
        return false;

    // Accumulate all uniforms into a single buffer
    // Allocate or expand the buffer
    size_t new_size = g_UniformBlockSize + size;
    void* new_data = realloc(g_UniformBlockData, new_size);
    if (!new_data)
        return false;

    g_UniformBlockData = new_data;
    memcpy((char*)g_UniformBlockData + g_UniformBlockSize, data, size);
    g_UniformBlockSize = new_size;

    return true;
}

IMPLATFORM_API void ImPlatform_EndUniformBlock(ImPlatform_ShaderProgram program)
{
    if (!program || program != g_CurrentUniformBlockProgram)
        return;

    ImPlatform_ShaderProgramData_Vulkan* program_data = (ImPlatform_ShaderProgramData_Vulkan*)program;

    // Store uniform data in the program structure for later use in push constants
    // No need to create a uniform buffer anymore - we will pass this via push constants
    if (g_UniformBlockData && g_UniformBlockSize > 0)
    {
        // Reallocate program uniform buffer to store the data
        if (program_data->uniformBufferSize != g_UniformBlockSize)
        {
            if (program_data->uniformBufferMapped)
                free(program_data->uniformBufferMapped);
            
            program_data->uniformBufferMapped = malloc(g_UniformBlockSize);
            program_data->uniformBufferSize = g_UniformBlockSize;
        }

        if (program_data->uniformBufferMapped)
        {
            memcpy(program_data->uniformBufferMapped, g_UniformBlockData, g_UniformBlockSize);
            program_data->uniformBufferDirty = true;
        }
    }

    g_CurrentUniformBlockProgram = nullptr;
}

// ============================================================================
// Custom Shader DrawList Integration
// ============================================================================

// ImDrawCallback handler to activate a custom shader
static void ImPlatform_SetCustomShader(const ImDrawList* parent_list, const ImDrawCmd* cmd)
{
    ImPlatform_ShaderProgram program = (ImPlatform_ShaderProgram)cmd->UserCallbackData;
    if (!program || g_CurrentCommandBuffer == VK_NULL_HANDLE)
        return;

    ImPlatform_ShaderProgramData_Vulkan* program_data = (ImPlatform_ShaderProgramData_Vulkan*)program;

    // Bind custom pipeline
    vkCmdBindPipeline(g_CurrentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, program_data->pipeline);

    // Rebind push constants (scale and translate for projection)
    // Find which viewport owns this draw list
    ImDrawData* draw_data = nullptr;

    // First, try the cached draw data (if multi-viewport wrapper is active)
    if (g_CurrentDrawData)
    {
        draw_data = g_CurrentDrawData;
    }
#ifdef IMGUI_HAS_VIEWPORT
    else
    {
        // Fallback: search through all viewports to find which one owns this draw list
        ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
        for (int i = 0; i < platform_io.Viewports.Size; i++)
        {
            ImGuiViewport* viewport = platform_io.Viewports[i];
            if (viewport->DrawData)
            {
                for (int j = 0; j < viewport->DrawData->CmdLists.Size; j++)
                {
                    if (viewport->DrawData->CmdLists[j] == parent_list)
                    {
                        draw_data = viewport->DrawData;
                        break;
                    }
                }
                if (draw_data)
                    break;
            }
        }
    }
#endif
    if (!draw_data)
        draw_data = ImGui::GetDrawData();

    if (!draw_data)
        return;

    // Build projection matrix (same as ImGui's approach)
    float L = draw_data->DisplayPos.x;
    float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float T = draw_data->DisplayPos.y;
    float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;

    // Create orthographic projection matrix
    float mvp[4][4] =
    {
        { 2.0f/(R-L),   0.0f,           0.0f,       0.0f },
        { 0.0f,         2.0f/(T-B),     0.0f,       0.0f },
        { 0.0f,         0.0f,          -1.0f,       0.0f },
        { (R+L)/(L-R),  (T+B)/(B-T),    0.0f,       1.0f },
    };

    // Push projection matrix (offset 0, 64 bytes)
    vkCmdPushConstants(g_CurrentCommandBuffer, program_data->pipelineLayout,
                      VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 16, mvp);

    // Push custom uniform data (offset 64, variable size)
    // This data was set via BeginUniformBlock/SetUniform/EndUniformBlock
    if (program_data->uniformBufferMapped && program_data->uniformBufferSize > 0)
    {
        vkCmdPushConstants(g_CurrentCommandBuffer, program_data->pipelineLayout,
                          VK_SHADER_STAGE_FRAGMENT_BIT, 64,
                          program_data->uniformBufferSize, program_data->uniformBufferMapped);
    }

    // Bind descriptor set (set 0) containing only the texture (binding 0)
    // This descriptor set layout is now IDENTICAL to ImGui's layout
    // Binding 0: Default white 1x1 texture (set during shader program creation)
    // Binding 1: Uniform buffer with custom shader parameters (updated via UpdateUniformBlock)
    if (program_data->descriptorSet == VK_NULL_HANDLE)
    {
        fprintf(stderr, "[ImPlatform] Vulkan: ERROR - Descriptor set is NULL in SetCustomShader callback!\n");
    }
    else
    {
        // Debug: log that we're binding our descriptor sets
        fprintf(stderr, "[ImPlatform] Vulkan: Binding custom shader descriptor set: %p with pipeline layout: %p\n",
                (void*)program_data->descriptorSet, (void*)program_data->pipelineLayout);
        vkCmdBindDescriptorSets(g_CurrentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, program_data->pipelineLayout, 0, 1, &program_data->descriptorSet, 0, nullptr);
    }
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

    draw->AddCallback(ImDrawCallback_ResetRenderState, NULL);
}

#endif // IM_GFX_VULKAN
