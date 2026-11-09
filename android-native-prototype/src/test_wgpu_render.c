#include "test_wgpu_render.h"
#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>
#include <android/log.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define LOG_TAG "CricketWGPU"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static CricketWgpuRenderState g_render_state = {0};

static WGPUInstance g_instance = NULL;
static WGPUSurface g_surface = NULL;
static WGPUAdapter g_adapter = NULL;
static WGPUDevice g_device = NULL;
static WGPUQueue g_queue = NULL;
static WGPURenderPipeline g_pipeline = NULL;
static WGPUSurfaceConfiguration g_config = {0};
static WGPUTextureFormat g_surface_format = WGPUTextureFormat_Undefined;
static ANativeWindow* g_window = NULL;

// Embedded WGSL Shader for Phase B.7 verification
// Vertex shader: Generates a centered golden triangle (emulating pitch/crease marker)
// Fragment shader: Renders cricket ball gold/yellow RGB(1.0, 0.85, 0.1, 1.0)
static const char* k_triangle_wgsl =
    "@vertex\n"
    "fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4<f32> {\n"
    "    var pos = array<vec2<f32>, 3>(\n"
    "        vec2<f32>( 0.0,  0.55),\n"
    "        vec2<f32>(-0.55, -0.45),\n"
    "        vec2<f32>( 0.55, -0.45)\n"
    "    );\n"
    "    return vec4<f32>(pos[in_vertex_index], 0.0, 1.0);\n"
    "}\n\n"
    "@fragment\n"
    "fn fs_main() -> @location(0) vec4<f32> {\n"
    "    return vec4<f32>(1.0, 0.85, 0.1, 1.0);\n"
    "}\n";

// wgpu log forwarder to Android logcat
static void wgpu_log_callback(WGPULogLevel level, char const* message, void* userdata) {
    (void)userdata;
    switch (level) {
        case WGPULogLevel_Error:
            LOGE("[Rust-wgpu-core] %s", message);
            break;
        case WGPULogLevel_Warn:
            LOGW("[Rust-wgpu-core] %s", message);
            break;
        case WGPULogLevel_Info:
        default:
            LOGI("[Rust-wgpu-core] %s", message);
            break;
    }
}

static void on_uncaptured_error(WGPUErrorType type, char const* message, void* userdata) {
    (void)userdata;
    LOGE("WebGPU Uncaptured Error (Type: %d): %s", type, message ? message : "unknown");
}

typedef struct {
    void* result;
    bool done;
} AsyncSyncData;

static void on_adapter_received(WGPURequestAdapterStatus status, WGPUAdapter adapter, char const* message, void* userdata) {
    AsyncSyncData* data = (AsyncSyncData*)userdata;
    if (status == WGPURequestAdapterStatus_Success && adapter) {
        data->result = adapter;
        LOGI("WebGPU Adapter successfully acquired!");
    } else {
        LOGE("wgpuInstanceRequestAdapter failed (status 0x%08X): %s", status, message ? message : "none");
    }
    data->done = true;
}

static void on_device_received(WGPURequestDeviceStatus status, WGPUDevice device, char const* message, void* userdata) {
    AsyncSyncData* data = (AsyncSyncData*)userdata;
    if (status == WGPURequestDeviceStatus_Success && device) {
        data->result = device;
        LOGI("WebGPU Logical Device successfully created!");
    } else {
        LOGE("wgpuAdapterRequestDevice failed (status 0x%08X): %s", status, message ? message : "none");
    }
    data->done = true;
}

bool cricket_wgpu_init(ANativeWindow* window) {
    if (!window) {
        LOGE("Cannot initialize WebGPU: ANativeWindow is NULL!");
        return false;
    }
    g_window = window;

    LOGI("==========================================================");
    LOGI("INITIALIZING NATIVE WEBGPU VIA wgpu-native (VULKAN BACKEND)");
    LOGI("==========================================================");

    // Setup logging
    wgpuSetLogCallback(wgpu_log_callback, NULL);
    wgpuSetLogLevel(WGPULogLevel_Info);

    // 1. Create WebGPU Instance
    g_instance = wgpuCreateInstance(NULL);
    if (!g_instance) {
        LOGE("Failed to create WGPUInstance!");
        return false;
    }
    LOGI("WGPUInstance created successfully (%p)", g_instance);

    // 2. Create Surface from Android ANativeWindow
    uint32_t win_width = (uint32_t)ANativeWindow_getWidth(window);
    uint32_t win_height = (uint32_t)ANativeWindow_getHeight(window);
    LOGI("Attaching ANativeWindow to WebGPU surface (Surface Dimensions: %ux%u)", win_width, win_height);

    WGPUSurfaceDescriptorFromAndroidNativeWindow android_surface_source = {
        .chain = {
            .next = NULL,
            .sType = WGPUSType_SurfaceDescriptorFromAndroidNativeWindow
        },
        .window = window
    };

    WGPUSurfaceDescriptor surface_desc = {
        .nextInChain = (const WGPUChainedStruct*)&android_surface_source,
        .label = "CricketLegacy_Android_Surface"
    };

    g_surface = wgpuInstanceCreateSurface(g_instance, &surface_desc);
    if (!g_surface) {
        LOGE("Failed to create WGPUSurface from ANativeWindow!");
        cricket_wgpu_shutdown();
        return false;
    }
    LOGI("WGPUSurface created successfully (%p)", g_surface);

    // 3. Request Adapter compatible with Android Surface
    WGPURequestAdapterOptions adapter_opts = {
        .nextInChain = NULL,
        .compatibleSurface = g_surface,
        .powerPreference = WGPUPowerPreference_HighPerformance,
        .forceFallbackAdapter = false
    };

    AsyncSyncData adapter_sync = { .result = NULL, .done = false };
    wgpuInstanceRequestAdapter(g_instance, &adapter_opts, on_adapter_received, &adapter_sync);

    if (!adapter_sync.result) {
        LOGW("Waiting for adapter async callback...");
        wgpuInstanceProcessEvents(g_instance);
    }
    g_adapter = (WGPUAdapter)adapter_sync.result;
    if (!g_adapter) {
        LOGE("Failed to acquire compatible WGPUAdapter!");
        cricket_wgpu_shutdown();
        return false;
    }

    // Query Adapter Info
    WGPUAdapterInfo adapter_info = {0};
    wgpuAdapterGetInfo(g_adapter, &adapter_info);
    LOGI("GPU Adapter: %s | Vendor: %s | Architecture: %s",
         adapter_info.device ? adapter_info.device : "Unknown Device",
         adapter_info.vendor ? adapter_info.vendor : "Unknown Vendor",
         adapter_info.architecture ? adapter_info.architecture : "Unknown Arch");
    LOGI("Adapter Backend Type: %d (1=Null, 2=D3D11, 3=D3D12, 4=Metal, 5=Vulkan, 6=OpenGL)",
         adapter_info.backendType);

    snprintf(g_render_state.adapter_name, sizeof(g_render_state.adapter_name), "%s",
             adapter_info.device ? adapter_info.device : "Vulkan Device");
    snprintf(g_render_state.backend_info, sizeof(g_render_state.backend_info), "Vulkan (Backend %d)",
             adapter_info.backendType);

    // 4. Request Logical Device
    WGPUDeviceDescriptor dev_desc = {
        .nextInChain = NULL,
        .label = "CricketLegacy_Device",
        .requiredFeatureCount = 0,
        .requiredFeatures = NULL,
        .requiredLimits = NULL,
        .defaultQueue = {
            .nextInChain = NULL,
            .label = "CricketLegacy_Queue"
        },
        .deviceLostCallback = NULL,
        .deviceLostUserdata = NULL,
        .uncapturedErrorCallbackInfo = {
            .callback = on_uncaptured_error,
            .userdata = NULL
        }
    };

    AsyncSyncData dev_sync = { .result = NULL, .done = false };
    wgpuAdapterRequestDevice(g_adapter, &dev_desc, on_device_received, &dev_sync);
    if (!dev_sync.result) {
        LOGW("Waiting for device async callback...");
        wgpuInstanceProcessEvents(g_instance);
    }
    g_device = (WGPUDevice)dev_sync.result;
    if (!g_device) {
        LOGE("Failed to create WGPUDevice!");
        cricket_wgpu_shutdown();
        return false;
    }

    g_queue = wgpuDeviceGetQueue(g_device);
    LOGI("WGPUQueue acquired successfully (%p)", g_queue);

    // 5. Query Surface Capabilities & Configure Swapchain
    WGPUSurfaceCapabilities caps = {0};
    wgpuSurfaceGetCapabilities(g_surface, g_adapter, &caps);
    LOGI("Surface Capabilities: %zu formats, %zu alpha modes, %zu present modes, usages=0x%08X",
         caps.formatCount, caps.alphaModeCount, caps.presentModeCount, (unsigned int)caps.usages);

    for (size_t i = 0; i < caps.formatCount; ++i) {
        LOGI("  Format [%zu]: %d", i, caps.formats[i]);
    }
    for (size_t i = 0; i < caps.alphaModeCount; ++i) {
        LOGI("  AlphaMode [%zu]: %d", i, caps.alphaModes[i]);
    }
    for (size_t i = 0; i < caps.presentModeCount; ++i) {
        LOGI("  PresentMode [%zu]: %d", i, caps.presentModes[i]);
    }

    if (caps.formatCount > 0) {
        g_surface_format = caps.formats[0];
    } else {
        g_surface_format = WGPUTextureFormat_RGBA8Unorm;
    }
    LOGI("Selected Surface Format: %d", g_surface_format);

    WGPUCompositeAlphaMode selected_alpha_mode = WGPUCompositeAlphaMode_Auto;
    if (caps.alphaModeCount > 0) {
        selected_alpha_mode = caps.alphaModes[0];
    }
    LOGI("Selected Composite Alpha Mode: %d", selected_alpha_mode);

    WGPUPresentMode selected_present_mode = WGPUPresentMode_Fifo;
    if (caps.presentModeCount > 0) {
        selected_present_mode = caps.presentModes[0];
    }
    LOGI("Selected Present Mode: %d", selected_present_mode);

    g_config.nextInChain = NULL;
    g_config.device = g_device;
    g_config.format = g_surface_format;
    g_config.usage = WGPUTextureUsage_RenderAttachment;
    g_config.viewFormatCount = 0;
    g_config.viewFormats = NULL;
    g_config.alphaMode = selected_alpha_mode;
    g_config.width = win_width > 0 ? win_width : 1280;
    g_config.height = win_height > 0 ? win_height : 720;
    g_config.presentMode = selected_present_mode;

    wgpuSurfaceConfigure(g_surface, &g_config);
    LOGI("wgpuSurfaceConfigure completed successfully (%ux%u)", g_config.width, g_config.height);

    // 6. Build Render Pipeline for WGSL colored triangle test
    WGPUShaderModuleWGSLDescriptor wgsl_desc = {
        .chain = {
            .next = NULL,
            .sType = WGPUSType_ShaderModuleWGSLDescriptor
        },
        .code = k_triangle_wgsl
    };
    WGPUShaderModuleDescriptor sm_desc = {
        .nextInChain = (const WGPUChainedStruct*)&wgsl_desc,
        .label = "Cricket_Triangle_Shader"
    };
    WGPUShaderModule shader_module = wgpuDeviceCreateShaderModule(g_device, &sm_desc);
    if (!shader_module) {
        LOGE("Failed to compile embedded WGSL shader module!");
        cricket_wgpu_shutdown();
        return false;
    }
    LOGI("WGSL Shader Module compiled successfully!");

    WGPUPipelineLayoutDescriptor pl_desc = {
        .nextInChain = NULL,
        .label = "Cricket_Pipeline_Layout",
        .bindGroupLayoutCount = 0,
        .bindGroupLayouts = NULL
    };
    WGPUPipelineLayout pipeline_layout = wgpuDeviceCreatePipelineLayout(g_device, &pl_desc);

    WGPUColorTargetState color_target = {
        .nextInChain = NULL,
        .format = g_surface_format,
        .blend = NULL,
        .writeMask = WGPUColorWriteMask_All
    };

    WGPUFragmentState fragment_state = {
        .nextInChain = NULL,
        .module = shader_module,
        .entryPoint = "fs_main",
        .constantCount = 0,
        .constants = NULL,
        .targetCount = 1,
        .targets = &color_target
    };

    WGPURenderPipelineDescriptor pipeline_desc = {
        .nextInChain = NULL,
        .label = "Cricket_Triangle_Render_Pipeline",
        .layout = pipeline_layout,
        .vertex = {
            .nextInChain = NULL,
            .module = shader_module,
            .entryPoint = "vs_main",
            .constantCount = 0,
            .constants = NULL,
            .bufferCount = 0,
            .buffers = NULL
        },
        .primitive = {
            .nextInChain = NULL,
            .topology = WGPUPrimitiveTopology_TriangleList,
            .stripIndexFormat = WGPUIndexFormat_Undefined,
            .frontFace = WGPUFrontFace_CCW,
            .cullMode = WGPUCullMode_None
        },
        .depthStencil = NULL,
        .multisample = {
            .nextInChain = NULL,
            .count = 1,
            .mask = 0xFFFFFFFF,
            .alphaToCoverageEnabled = false
        },
        .fragment = &fragment_state
    };

    g_pipeline = wgpuDeviceCreateRenderPipeline(g_device, &pipeline_desc);
    wgpuShaderModuleRelease(shader_module);
    wgpuPipelineLayoutRelease(pipeline_layout);

    if (!g_pipeline) {
        LOGE("Failed to create WGPURenderPipeline!");
        cricket_wgpu_shutdown();
        return false;
    }
    LOGI("WGPURenderPipeline successfully created!");

    g_render_state.is_initialized = true;
    g_render_state.width = g_config.width;
    g_render_state.height = g_config.height;

    LOGI("==========================================================");
    LOGI("WEBGPU PIPELINE READY: Android Surface -> wgpu -> Vulkan");
    LOGI("==========================================================");

    return true;
}

bool cricket_wgpu_render_frame(void) {
    if (!g_render_state.is_initialized || !g_surface || !g_device || !g_queue) {
        return false;
    }

    // 1. Acquire current swapchain texture
    WGPUSurfaceTexture surface_texture = {0};
    wgpuSurfaceGetCurrentTexture(g_surface, &surface_texture);

    if (surface_texture.status == WGPUSurfaceGetCurrentTextureStatus_Timeout ||
        surface_texture.status == WGPUSurfaceGetCurrentTextureStatus_Outdated ||
        surface_texture.status == WGPUSurfaceGetCurrentTextureStatus_Lost) {
        LOGW("Surface texture status %d; reconfiguring surface...", surface_texture.status);
        if (surface_texture.texture) {
            wgpuTextureRelease(surface_texture.texture);
        }
        wgpuSurfaceConfigure(g_surface, &g_config);
        return false;
    }

    if (surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_Success || !surface_texture.texture) {
        LOGE("wgpuSurfaceGetCurrentTexture failed with status %d", surface_texture.status);
        return false;
    }

    // 2. Create Texture View for the frame
    WGPUTextureViewDescriptor view_desc = {
        .nextInChain = NULL,
        .label = "Cricket_Frame_TextureView",
        .format = g_surface_format,
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .aspect = WGPUTextureAspect_All
    };
    WGPUTextureView view = wgpuTextureCreateView(surface_texture.texture, &view_desc);
    if (!view) {
        LOGE("Failed to create WGPUTextureView from surface texture!");
        wgpuTextureRelease(surface_texture.texture);
        return false;
    }

    // 3. Create Command Encoder
    WGPUCommandEncoderDescriptor enc_desc = {
        .nextInChain = NULL,
        .label = "Cricket_Render_CommandEncoder"
    };
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(g_device, &enc_desc);

    // 4. Begin Render Pass
    // Clear to authentic Cricket 07 stadium turf green: RGB(0.12, 0.48, 0.18, 1.0)
    WGPURenderPassColorAttachment color_attachment = {
        .nextInChain = NULL,
        .view = view,
        .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
        .resolveTarget = NULL,
        .loadOp = WGPULoadOp_Clear,
        .storeOp = WGPUStoreOp_Store,
        .clearValue = {
            .r = 0.12,
            .g = 0.48,
            .b = 0.18,
            .a = 1.0
        }
    };

    WGPURenderPassDescriptor pass_desc = {
        .nextInChain = NULL,
        .label = "Cricket_Clear_and_Draw_Pass",
        .colorAttachmentCount = 1,
        .colorAttachments = &color_attachment,
        .depthStencilAttachment = NULL,
        .occlusionQuerySet = NULL,
        .timestampWrites = NULL
    };

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &pass_desc);

    // Draw the test triangle using our compiled pipeline
    wgpuRenderPassEncoderSetPipeline(pass, g_pipeline);
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);

    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    // 5. Finish Command Buffer & Submit to GPU Queue
    WGPUCommandBufferDescriptor cmd_desc = {
        .nextInChain = NULL,
        .label = "Cricket_Render_CommandBuffer"
    };
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, &cmd_desc);
    wgpuCommandEncoderRelease(encoder);

    wgpuQueueSubmit(g_queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);

    // 6. Present Surface to Android ANativeWindow
    wgpuSurfacePresent(g_surface);

    // Cleanup per-frame resources
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(surface_texture.texture);

    g_render_state.frame_rendered = true;
    g_render_state.frame_count++;

    if (g_render_state.frame_count == 1 || (g_render_state.frame_count % 60) == 0) {
        LOGI("[FRAME PRESENTED] Frame #%u rendered via wgpu-native -> Vulkan -> Android Surface (Resolution: %ux%u)",
             g_render_state.frame_count, g_config.width, g_config.height);
    }

    return true;
}

void cricket_wgpu_resize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return;
    g_config.width = width;
    g_config.height = height;
    g_render_state.width = width;
    g_render_state.height = height;
    if (g_surface && g_device) {
        wgpuSurfaceConfigure(g_surface, &g_config);
        LOGI("wgpuSurfaceConfigure reconfigured on resize to %ux%u", width, height);
    }
}

void cricket_wgpu_shutdown(void) {
    LOGI("Shutting down WebGPU runtime and releasing resources...");
    if (g_pipeline) {
        wgpuRenderPipelineRelease(g_pipeline);
        g_pipeline = NULL;
    }
    if (g_queue) {
        wgpuQueueRelease(g_queue);
        g_queue = NULL;
    }
    if (g_device) {
        wgpuDeviceRelease(g_device);
        g_device = NULL;
    }
    if (g_adapter) {
        wgpuAdapterRelease(g_adapter);
        g_adapter = NULL;
    }
    if (g_surface) {
        wgpuSurfaceRelease(g_surface);
        g_surface = NULL;
    }
    if (g_instance) {
        wgpuInstanceRelease(g_instance);
        g_instance = NULL;
    }
    g_render_state.is_initialized = false;
    g_render_state.frame_rendered = false;
    LOGI("WebGPU runtime released.");
}

const CricketWgpuRenderState* cricket_wgpu_get_state(void) {
    return &g_render_state;
}
