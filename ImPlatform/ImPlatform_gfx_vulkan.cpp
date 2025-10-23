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
#elif defined(IM_PLATFORM_GLFW)
    #define GLFW_INCLUDE_NONE
    #define GLFW_INCLUDE_VULKAN
    #include <GLFW/glfw3.h>
#elif defined(IM_PLATFORM_SDL2)
    #include <SDL.h>
    #include <SDL_vulkan.h>
#elif defined(IM_PLATFORM_SDL3)
    #include <SDL3/SDL.h>
    #include <SDL3/SDL_vulkan.h>
#endif

// Global state
static ImPlatform_GfxData_Vulkan g_GfxData = { 0 };
static VkAllocationCallbacks* g_Allocator = NULL;
static ImGui_ImplVulkanH_Window g_MainWindowData;  // Don't use = {} - let constructor run!
static bool g_SwapChainRebuild = false;
static uint32_t g_QueueFamily = (uint32_t)-1;

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
#elif defined(IM_PLATFORM_GLFW)
    GLFWwindow* pWindow = ImPlatform_App_GetGLFWWindow();
    return ImPlatform_Gfx_CreateDevice_Vulkan(pWindow, &g_GfxData);
#elif defined(IM_PLATFORM_SDL2)
    SDL_Window* pWindow = ImPlatform_App_GetSDL2Window();
    return ImPlatform_Gfx_CreateDevice_Vulkan(pWindow, &g_GfxData);
#elif defined(IM_PLATFORM_SDL3)
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

    if (!ImGui_ImplVulkan_Init(&init_info))
        return false;

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
#elif defined(IM_PLATFORM_GLFW)
        glfwGetFramebufferSize(ImPlatform_App_GetGLFWWindow(), &width, &height);
#elif defined(IM_PLATFORM_SDL2)
        SDL_GetWindowSize(ImPlatform_App_GetSDL2Window(), &width, &height);
#elif defined(IM_PLATFORM_SDL3)
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
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), fd->CommandBuffer);

    vkCmdEndRenderPass(fd->CommandBuffer);

    VkResult err = vkEndCommandBuffer(fd->CommandBuffer);
    check_vk_result(err);

    return true;
}

// ImPlatform API - GfxViewportPre
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

#endif // IM_GFX_VULKAN
