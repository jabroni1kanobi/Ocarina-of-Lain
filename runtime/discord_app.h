// The Discord application NX2navi's Rich Presence (discord_rpc.c) speaks for: the one place its defaults live.
//
// Discord shows the *application's* name above the presence ("Playing <name>") and takes the large image from the
// art assets uploaded to that application; neither comes from the game. So the presence needs an application of its
// own, registered at discord.com/developers/applications under the name NX2navi, with its art uploaded under
// Rich Presence > Art Assets with the key below. Its Application ID (General Information) goes into
// NX2NAVI_DISCORD_APP_ID_DEFAULT; a player can still point at another application with discord_app_id and
// discord_image in nx2navi.ini ([discord]), which take precedence over these defaults.
//
// While the id is empty and nx2navi.ini names none, the presence stays off even with discord_rpc = 1 and logs why:
// an id of some other application would show that application's name instead of NX2navi.
#pragma once

#define NX2NAVI_DISCORD_APP_ID_DEFAULT "1568862362969309275"   // the NX2navi application's id: digits, at most 20
#define NX2NAVI_DISCORD_IMAGE_DEFAULT "nx2navi_512x512"        // its large-image art asset key
