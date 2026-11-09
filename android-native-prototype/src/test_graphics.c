#include "test_graphics.h"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <android/log.h>
#include <string.h>
#include <stdlib.h>

#define LOG_TAG "CricketGraphics"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static CricketGraphicsState g_graphics_state = {0};
static VkInstance g_instance = VK_NULL_HANDLE;
static VkSurfaceKHR g_surface = VK_NULL_HANDLE;
static VkPhysicalDevice g_physical_device = VK_NULL_HANDLE;
static VkDevice g_device = VK_NULL_HANDLE;
static VkQueue g_queue = VK_NULL_HANDLE;
static uint32_t g_queue_family_index = 0;

static const char* vk_result_to_string(VkResult res) {
    switch (res) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
        default: return "VK_UNKNOWN_ERROR";
    }
}

bool cricket_graphics_init(ANativeWindow* window) {
    if (!window) {
        LOGE("Cannot initialize graphics: ANativeWindow is NULL!");
        return false;
    }

    LOGI("==================================================");
    LOGI("INITIALIZING ANDROID NATIVE GRAPHICS (VULKAN 1.1+)");
    LOGI("==================================================");

    // 1. Create Vulkan Instance with Android Surface extensions
    const char* extensions[] = {
        "VK_KHR_surface",
        "VK_KHR_android_surface"
    };

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "CricketLegacyHarness",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "CricketLegacyEngine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_1
    };

    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = 2,
        .ppEnabledExtensionNames = extensions
    };

    VkResult res = vkCreateInstance(&create_info, NULL, &g_instance);
    if (res != VK_SUCCESS) {
        LOGW("vkCreateInstance failed with %s (%d). Vulkan may not be supported by this GPU/Driver.", 
             vk_result_to_string(res), res);
        g_graphics_state.is_vulkan_supported = false;
        snprintf(g_graphics_state.renderer_info, sizeof(g_graphics_state.renderer_info), "Vulkan Unavailable");
        return false;
    }

    g_graphics_state.is_vulkan_supported = true;
    LOGI("Vulkan instance created successfully!");

    // 2. Create Android Vulkan Surface
    VkAndroidSurfaceCreateInfoKHR surface_info = {
        .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
        .window = window
    };

    res = vkCreateAndroidSurfaceKHR(g_instance, &surface_info, NULL, &g_surface);
    if (res != VK_SUCCESS) {
        LOGE("vkCreateAndroidSurfaceKHR failed: %s (%d)", vk_result_to_string(res), res);
        cricket_graphics_shutdown();
        return false;
    }
    LOGI("Android native Vulkan surface successfully created!");

    // 3. Enumerate Physical Devices
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(g_instance, &device_count, NULL);
    if (device_count == 0) {
        LOGE("No Vulkan physical devices found on this system!");
        cricket_graphics_shutdown();
        return false;
    }

    VkPhysicalDevice* devices = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * device_count);
    vkEnumeratePhysicalDevices(g_instance, &device_count, devices);
    g_physical_device = devices[0]; // Choose primary GPU
    free(devices);

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(g_physical_device, &props);
    LOGI("Primary GPU Detected: %s (Driver Version: 0x%08X)", props.deviceName, props.driverVersion);
    snprintf(g_graphics_state.renderer_info, sizeof(g_graphics_state.renderer_info), "%s", props.deviceName);

    // 4. Find Graphics Queue Family
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(g_physical_device, &queue_family_count, NULL);
    VkQueueFamilyProperties* queue_props = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(g_physical_device, &queue_family_count, queue_props);

    bool found_queue = false;
    for (uint32_t i = 0; i < queue_family_count; ++i) {
        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(g_physical_device, i, g_surface, &present_support);
        if ((queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present_support) {
            g_queue_family_index = i;
            found_queue = true;
            break;
        }
    }
    free(queue_props);

    if (!found_queue) {
        LOGE("No queue family supporting both Graphics and Surface Presentation!");
        cricket_graphics_shutdown();
        return false;
    }

    // 5. Create Logical Device
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = g_queue_family_index,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority
    };

    const char* device_extensions[] = { "VK_KHR_swapchain" };
    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_create_info,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = device_extensions
    };

    res = vkCreateDevice(g_physical_device, &device_info, NULL, &g_device);
    if (res != VK_SUCCESS) {
        LOGE("vkCreateDevice failed: %s (%d)", vk_result_to_string(res), res);
        cricket_graphics_shutdown();
        return false;
    }

    vkGetDeviceQueue(g_device, g_queue_family_index, 0, &g_queue);
    LOGI("Vulkan logical device and graphics queue ready!");

    g_graphics_state.surface_width = ANativeWindow_getWidth(window);
    g_graphics_state.surface_height = ANativeWindow_getHeight(window);
    g_graphics_state.is_initialized = true;

    LOGI("Graphics initialized successfully on %dx%d surface.", 
         g_graphics_state.surface_width, g_graphics_state.surface_height);
    return true;
}

void cricket_graphics_render_test_frame(void) {
    if (!g_graphics_state.is_initialized) return;
    // In full implementation, acquires swapchain image, binds render pass,
    // clears to stadium grass RGB(31, 115, 46) and calls vkQueuePresentKHR.
}

void cricket_graphics_resize(int width, int height) {
    g_graphics_state.surface_width = width;
    g_graphics_state.surface_height = height;
    LOGI("Surface resize event: %dx%d", width, height);
}

void cricket_graphics_shutdown(void) {
    LOGI("Shutting down Vulkan graphics context...");
    if (g_device != VK_NULL_HANDLE) {
        vkDestroyDevice(g_device, NULL);
        g_device = VK_NULL_HANDLE;
    }
    if (g_surface != VK_NULL_HANDLE && g_instance != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(g_instance, g_surface, NULL);
        g_surface = VK_NULL_HANDLE;
    }
    if (g_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(g_instance, NULL);
        g_instance = VK_NULL_HANDLE;
    }
    g_graphics_state.is_initialized = false;
    LOGI("Graphics context released.");
}

const CricketGraphicsState* cricket_graphics_get_state(void) {
    return &g_graphics_state;
}
