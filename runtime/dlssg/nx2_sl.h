// The C interface of nx2navi_dlssg.dll (runtime/dlssg/nx2_sl.cpp), which runtime/vk_dlssg.c loads for DLSS Frame
// Generation. The DLL is built with MSVC against NVIDIA's Streamline SDK (its C++ types have MSVC's ABI); nothing of
// the SDK is in this repository, and the game itself is built without it.
#pragma once
#include <stdint.h>
#include <vulkan/vulkan.h>

#define NX2SL_VERSION 1

typedef struct {
    VkImage image;
    VkImageView view;
    VkFormat format;
    uint32_t width, height;
    VkImageLayout layout;      // the layout it is in when Streamline reads it (at the present)
    VkImageUsageFlags usage;
} Nx2SlImage;

typedef struct {
    float view_to_clip[16], clip_to_view[16], clip_to_prev_clip[16], prev_clip_to_clip[16];   // row-major
    float pos[3], up[3], right[3], fwd[3];
    float znear, zfar, fov_y, aspect;
    float jitter[2];           // render pixels
    float mv_scale[2];         // what makes the motion vectors fractions of the render size
    int reset;
} Nx2SlCamera;

typedef void (*Nx2SlLog)(const char *line);

#ifdef __cplusplus
extern "C" {
#endif
#ifdef NX2SL_BUILD
#define NX2SL_API __declspec(dllexport)
#else
#define NX2SL_API
#endif
typedef int (*PFN_nx2sl_version)(void);
// slInit with DLSS-G, Reflex and PCL for Vulkan; dir: the folder of sl.interposer.dll and the plugins (the game's)
typedef int (*PFN_nx2sl_init)(const wchar_t *dir, Nx2SlLog log);
// The interposer's proc address functions: vkCreateInstance and vkCreateDevice through them add what DLSS-G needs
// (extensions, features, queues) and tell Streamline the device; the swapchain functions through them are its hooks.
typedef PFN_vkGetInstanceProcAddr (*PFN_nx2sl_instance_proc)(void);
typedef PFN_vkGetDeviceProcAddr (*PFN_nx2sl_device_proc)(void);
// 1 when DLSS-G loaded and supports this GPU (after the device exists)
typedef int (*PFN_nx2sl_available)(void);
// Each present, before the present call: a new frame, its latency markers up to the render's end, its constants and
// tags, and frame generation on or off. Then nx2sl_present_marker(0) before and (1) after the present.
typedef int (*PFN_nx2sl_frame)(VkCommandBuffer cb, const Nx2SlImage *depth, const Nx2SlImage *mv, const Nx2SlImage *hudless,
                               const Nx2SlCamera *cam, uint32_t out_w, uint32_t out_h, int enable);
typedef void (*PFN_nx2sl_present_marker)(int after);
typedef void (*PFN_nx2sl_stats)(uint64_t *presented, uint32_t *generated_per_frame);
typedef void (*PFN_nx2sl_shutdown)(void);
#ifdef __cplusplus
}
#endif
