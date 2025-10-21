// dear imgui: Graphics API Abstraction - Vulkan Backend
// This handles Vulkan instance, device creation, rendering, and presentation

#include "ImPlatform_Internal.h"

#ifdef IM_GFX_VULKAN

#include "../imgui.h"
#include "../imgui/backends/imgui_impl_vulkan.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef IM_PLATFORM_WIN32
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
static ImGui_ImplVulkanH_Window g_MainWindowData = {};
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
#ifdef IM_PLATFORM_WIN32
bool ImPlatform_Gfx_CreateDevice_Vulkan(HWND hWnd, ImPlatform_GfxData_Vulkan* pData)
{
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
    createInfo.hwnd = hWnd;
    VkResult err = vkCreateWin32SurfaceKHR(pData->instance, &createInfo, g_Allocator, &pData->surface);
    check_vk_result(err);

    // Create render pass and setup window data
    g_MainWindowData.Surface = pData->surface;
    pData->minImageCount = 2;
    ImGui_ImplVulkanH_CreateOrResizeWindow(pData->instance, pData->physicalDevice, pData->device,
        &g_MainWindowData, g_QueueFamily, g_Allocator, 1280, 720, pData->minImageCount);

    return true;
}
#endif

#ifdef IM_PLATFORM_GLFW
bool ImPlatform_Gfx_CreateDevice_Vulkan(GLFWwindow* pWindow, ImPlatform_GfxData_Vulkan* pData)
{
    // Get required extensions from GLFW
    uint32_t extensions_count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&extensions_count);

    if (!SetupVulkan(extensions, extensions_count))
        return false;

    // Create Window Surface
    VkResult err = glfwCreateWindowSurface(pData->instance, pWindow, g_Allocator, &pData->surface);
    check_vk_result(err);

    // Get framebuffer size
    int w, h;
    glfwGetFramebufferSize(pWindow, &w, &h);

    // Create render pass and setup window data
    g_MainWindowData.Surface = pData->surface;
    pData->minImageCount = 2;
    ImGui_ImplVulkanH_CreateOrResizeWindow(pData->instance, pData->physicalDevice, pData->device,
        &g_MainWindowData, g_QueueFamily, g_Allocator, w, h, pData->minImageCount);

    return true;
}
#endif

#ifdef IM_PLATFORM_SDL2
bool ImPlatform_Gfx_CreateDevice_Vulkan(SDL_Window* pWindow, ImPlatform_GfxData_Vulkan* pData)
{
    // Get required extensions from SDL2
    unsigned int extensions_count = 0;
    SDL_Vulkan_GetInstanceExtensions(pWindow, &extensions_count, NULL);
    const char** extensions = (const char**)malloc(sizeof(const char*) * extensions_count);
    SDL_Vulkan_GetInstanceExtensions(pWindow, &extensions_count, extensions);

    if (!SetupVulkan(extensions, extensions_count))
    {
        free(extensions);
        return false;
    }
    free(extensions);

    // Create Window Surface
    if (!SDL_Vulkan_CreateSurface(pWindow, pData->instance, &pData->surface))
        return false;

    // Get window size
    int w, h;
    SDL_GetWindowSize(pWindow, &w, &h);

    // Create render pass and setup window data
    g_MainWindowData.Surface = pData->surface;
    pData->minImageCount = 2;
    ImGui_ImplVulkanH_CreateOrResizeWindow(pData->instance, pData->physicalDevice, pData->device,
        &g_MainWindowData, g_QueueFamily, g_Allocator, w, h, pData->minImageCount);

    return true;
}
#endif

#ifdef IM_PLATFORM_SDL3
bool ImPlatform_Gfx_CreateDevice_Vulkan(SDL_Window* pWindow, ImPlatform_GfxData_Vulkan* pData)
{
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
    if (!SDL_Vulkan_CreateSurface(pWindow, pData->instance, NULL, &pData->surface))
        return false;

    // Get window size
    int w, h;
    SDL_GetWindowSize(pWindow, &w, &h);

    // Create render pass and setup window data
    g_MainWindowData.Surface = pData->surface;
    pData->minImageCount = 2;
    ImGui_ImplVulkanH_CreateOrResizeWindow(pData->instance, pData->physicalDevice, pData->device,
        &g_MainWindowData, g_QueueFamily, g_Allocator, w, h, pData->minImageCount);

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
#ifdef IM_PLATFORM_WIN32
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
    init_info.RenderPass = g_MainWindowData.RenderPass;
    init_info.Subpass = 0;
    init_info.MinImageCount = g_GfxData.minImageCount;
    init_info.ImageCount = g_MainWindowData.ImageCount;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = g_Allocator;
    init_info.CheckVkResultFn = check_vk_result;

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
#ifdef IM_PLATFORM_WIN32
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
                &g_MainWindowData, g_QueueFamily, g_Allocator, width, height, g_GfxData.minImageCount);
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

    ImGui_ImplVulkanH_Frame* fd = &g_MainWindowData.Frames[g_MainWindowData.FrameIndex];
    VkResult err = vkWaitForFences(g_GfxData.device, 1, &fd->Fence, VK_TRUE, UINT64_MAX);
    check_vk_result(err);

    err = vkResetFences(g_GfxData.device, 1, &fd->Fence);
    check_vk_result(err);

    err = vkAcquireNextImageKHR(g_GfxData.device, g_MainWindowData.Swapchain, UINT64_MAX, fd->ImageAcquiredSemaphore, VK_NULL_HANDLE, &g_MainWindowData.FrameIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
    {
        g_SwapChainRebuild = true;
        return false;
    }
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
    ImGui_ImplVulkanH_Frame* fd = &g_MainWindowData.Frames[g_MainWindowData.FrameIndex];

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &fd->ImageAcquiredSemaphore;
    info.pWaitDstStageMask = &wait_stage;
    info.commandBufferCount = 1;
    info.pCommandBuffers = &fd->CommandBuffer;
    info.signalSemaphoreCount = 1;
    info.pSignalSemaphores = &fd->RenderCompleteSemaphore;

    VkResult err = vkQueueSubmit(g_GfxData.queue, 1, &info, fd->Fence);
    check_vk_result(err);

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &fd->RenderCompleteSemaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &g_MainWindowData.Swapchain;
    present_info.pImageIndices = &g_MainWindowData.FrameIndex;
    err = vkQueuePresentKHR(g_GfxData.queue, &present_info);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
    {
        g_SwapChainRebuild = true;
        return true;
    }
    check_vk_result(err);

    g_MainWindowData.FrameIndex = (g_MainWindowData.FrameIndex + 1) % g_MainWindowData.ImageCount;

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

#endif // IM_GFX_VULKAN
