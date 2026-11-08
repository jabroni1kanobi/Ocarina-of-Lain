// The settings menu's art from the game's own interface textures (ui/menu_art.cpp).
#pragma once
#include "imgui.h"

typedef enum {
    MENU_ART_CURSOR_GLOW,   // the glow of the game's button cursor (white mask; tinted by the caller)
    MENU_ART_CURSOR_LINE,   // its outline
    MENU_ART_FRAME_DECO,    // a frame with ornamented corners
    MENU_ART_GUIDE_BAR,     // the control guide's bar
    MENU_ART_WINDOW_BASE,   // a rounded window base
    MENU_ART_COUNT
} MenuArt;

extern "C" void menu_art_start(void);    // decodes the textures on a worker, once
// The texture for the menu's draw lists and its size in pixels; 0 while it is not there (still decoding, or absent).
// Window thread: the first call for an art uploads it.
extern "C" int menu_art(MenuArt id, ImTextureID *tex, float *w, float *h);
extern "C" void menu_art_release(void);  // before ImGui's Vulkan back end shuts down
// Draws an art stretched over [a, b] with a tint; 0 (nothing drawn) while it is not there.
extern "C" int menu_art_draw(ImDrawList *dl, MenuArt id, ImVec2 a, ImVec2 b, ImU32 tint);
// Draws an art as nine pieces: corners `corner` pixels on screen from the outer `frac` of the texture, edges stretched,
// the middle too when fill.
extern "C" int menu_art_draw9(ImDrawList *dl, MenuArt id, ImVec2 a, ImVec2 b, float corner, float frac, ImU32 tint, int fill);
// The same with the texture turned half a turn (turned 1): a top-left corner piece drawn at the bottom right.
extern "C" int menu_art_draw9_turned(ImDrawList *dl, MenuArt id, ImVec2 a, ImVec2 b, float corner, float frac, ImU32 tint, int fill,
                                     int turned);
