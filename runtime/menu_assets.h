// The game's own interface fonts and textures for the settings menu, read from the user's RomFS (menu_assets.c).
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// A RomFS file, a .zs one decompressed with the game's dictionaries; malloc'd, NULL when absent or unreadable.
uint8_t *menu_asset_load(const char *rel, size_t *n);
// A .bfotf / .bfttf font as plain OpenType / TrueType bytes; malloc'd, NULL when absent or not a font.
uint8_t *menu_asset_font(const char *rel, size_t *n);
// Texture `name` of a BNTX archive as RGBA8 (BC4 masks: white, the mask in alpha); 1 with a malloc'd *rgba.
int menu_asset_texture(const uint8_t *bntx, size_t n, const char *name, uint8_t **rgba, uint32_t *w, uint32_t *h);

#ifdef __cplusplus
}
#endif
