// The settings menu's art from the game's own interface textures (menu_assets.c reads them from the user's RomFS):
// the frame's corner ornaments, the control guide's bar, and the layers of the buttons, cursor, panels and divider that
// ui/menu_skin.cpp puts together. A worker decodes them into RGBA once at start (the archive unpacks to about 240 MB and
// takes a moment); the window thread uploads each to the GPU with its smaller levels the first time the menu asks for
// it, and registers it with Dear ImGui's Vulkan back end. Without the files the menu draws its plain shapes.
#include <atomic>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"

extern "C" {
#include "host.h"
#include "vk.h"
#include "platform/platform.h"
}
#include "menu_assets.h"
#include "ui/menu_art.h"
#include "ui/menu_skin.h"

#define COMMON_BNTX "UI/Layout2Archive/Common.Product.100.Nin_Ounce_NVN2.bntx.zs"

// in the order of MenuArt (tools/menu_preview.py reads this list too)
static const char *const kNames[MENU_ART_COUNT] = {
    "LocationDeco_00", "CtrlGuideBG_00", "MessageWindowCommonBase_00",
    "BtnThinBase_01", "BtnThinBase_00", "BtnThinBase_05", "BtnThinBase_04", "BtnThinBase_02",
    "OptionBtnThinCursorGlow_00", "OptionBtnThinCursorLine_00", "SystemCaptionBg_00",
    "OptionSeparateMark_00", "SystemLine_03", "MenuHeaderSelect_00",
};
// Levels uploaded for each art: the buttons draw it at about half size and smaller, where a line one or two texels wide
// would break up if sampled from the full image alone.
enum { MAX_LEVELS = 6 };

static struct Art {
    uint8_t *rgba;
    uint32_t w, h;
    VkImage img;
    VmaAllocation alloc;
    VkImageView view;
    VkDescriptorSet ds;
    int failed;
} g_art[MENU_ART_COUNT];
static std::atomic<int> g_state;   // 0 not started, 1 decoding, 2 decoded, 3 nothing to show
static VkSampler g_sampler;

static void decode_all(void *) {
    size_t n = 0;
    uint8_t *bntx = menu_asset_load(COMMON_BNTX, &n);
    int got = 0;
    if (!bntx) rt_log("menu: %s could not be read from the RomFS", COMMON_BNTX);
    for (int i = 0; bntx && i < MENU_ART_COUNT; i++) {
        Art &a = g_art[i];
        if (menu_asset_texture(bntx, n, kNames[i], &a.rgba, &a.w, &a.h)) got++, skin_art_prepare(i, a.rgba, a.w, a.h);
        else rt_log("menu: interface texture %s not decoded (archive of %llu bytes)", kNames[i], (unsigned long long)n);
    }
    free(bntx);
    rt_log("menu: %d of %d interface textures from the RomFS", got, (int)MENU_ART_COUNT);
    g_state.store(got ? 2 : 3, std::memory_order_release);
}

extern "C" void menu_art_start(void) {
    int expect = 0;
    if (!g_state.compare_exchange_strong(expect, 1)) return;
    if (!plat_thread_spawn(decode_all, nullptr, "menu-art", 0)) decode_all(nullptr);
}

static int upload(Art &a) {
    VkCtx *c = vk_ctx();
    if (!c || !a.rgba) return 0;
    if (!g_sampler) {
        VkSamplerCreateInfo si = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        si.magFilter = si.minFilter = VK_FILTER_LINEAR;
        si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.maxLod = MAX_LEVELS;
        if (vkCreateSampler(c->device, &si, nullptr, &g_sampler) != VK_SUCCESS) return 0;
    }
    uint32_t lw[MAX_LEVELS] = {a.w}, lh[MAX_LEVELS] = {a.h}, levels = 1;
    VkDeviceSize bytes = (VkDeviceSize)a.w * a.h * 4;
    while (levels < MAX_LEVELS && (lw[levels - 1] > 1 || lh[levels - 1] > 1)) {
        lw[levels] = lw[levels - 1] > 1 ? lw[levels - 1] / 2 : 1;
        lh[levels] = lh[levels - 1] > 1 ? lh[levels - 1] / 2 : 1;
        bytes += (VkDeviceSize)lw[levels] * lh[levels] * 4;
        levels++;
    }
    VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = VK_FORMAT_R8G8B8A8_UNORM;
    ii.extent = {a.w, a.h, 1};
    ii.mipLevels = levels;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    VmaAllocationCreateInfo ai = {};
    ai.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    if (vmaCreateImage(c->vma, &ii, &ai, &a.img, &a.alloc, nullptr) != VK_SUCCESS) return 0;
    VkBuffer sb;
    VmaAllocation sa;
    uint8_t *map = (uint8_t *)vk_staging(bytes, &sb, &sa);
    if (!map) return 0;
    // the levels one after the other in a plain allocation, each halved from the one before (the staging memory may be
    // slow to read back)
    uint8_t *chain = (uint8_t *)malloc((size_t)bytes);
    if (!chain) {
        vmaDestroyBuffer(c->vma, sb, sa);
        return 0;
    }
    memcpy(chain, a.rgba, (size_t)a.w * a.h * 4);
    VkBufferImageCopy regions[MAX_LEVELS] = {};
    VkDeviceSize at = 0;
    for (uint32_t l = 0; l < levels; l++) {
        regions[l].bufferOffset = at;
        regions[l].imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, l, 0, 1};
        regions[l].imageExtent = {lw[l], lh[l], 1};
        if (l) skin_art_halve(chain + regions[l - 1].bufferOffset, lw[l - 1], lh[l - 1], chain + at, lw[l], lh[l]);
        at += (VkDeviceSize)lw[l] * lh[l] * 4;
    }
    memcpy(map, chain, (size_t)bytes);
    free(chain);
    VkCommandBuffer cb = vk_upload_begin();
    VkImageMemoryBarrier2 b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    b.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
    b.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
    b.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = a.img;
    b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, levels, 0, 1};
    VkDependencyInfo di = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    di.imageMemoryBarrierCount = 1;
    di.pImageMemoryBarriers = &b;
    vkCmdPipelineBarrier2(cb, &di);
    vkCmdCopyBufferToImage(cb, sb, a.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, levels, regions);
    b.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
    b.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    b.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    b.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
    b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier2(cb, &di);
    vk_upload_end();
    vk_upload_flush(1);
    vmaDestroyBuffer(c->vma, sb, sa);
    VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = a.img;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = VK_FORMAT_R8G8B8A8_UNORM;
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, levels, 0, 1};
    if (vkCreateImageView(c->device, &vi, nullptr, &a.view) != VK_SUCCESS) return 0;
    a.ds = ImGui_ImplVulkan_AddTexture(g_sampler, a.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return a.ds != VK_NULL_HANDLE;   // the pixels stay (a few MB): the back end may start again on another format
}

extern "C" int menu_art(MenuArt id, ImTextureID *tex, float *w, float *h) {
    if ((unsigned)id >= MENU_ART_COUNT || g_state.load(std::memory_order_acquire) != 2) return 0;
    Art &a = g_art[id];
    if (!a.ds && !a.failed && !upload(a)) a.failed = 1;
    if (!a.ds) return 0;
    *tex = (ImTextureID)a.ds;
    if (w) *w = (float)a.w;
    if (h) *h = (float)a.h;
    return 1;
}

extern "C" void menu_art_release(void) {
    VkCtx *c = vk_ctx();
    if (!c) return;
    for (Art &a : g_art) {
        if (a.ds) ImGui_ImplVulkan_RemoveTexture(a.ds);
        if (a.view) vkDestroyImageView(c->device, a.view, nullptr);
        if (a.img) vmaDestroyImage(c->vma, a.img, a.alloc);
        a.ds = VK_NULL_HANDLE, a.view = VK_NULL_HANDLE, a.img = VK_NULL_HANDLE, a.alloc = nullptr;
        a.failed = 0;   // uploaded again at the next use
    }
    if (g_sampler) vkDestroySampler(c->device, g_sampler, nullptr), g_sampler = VK_NULL_HANDLE;
}
