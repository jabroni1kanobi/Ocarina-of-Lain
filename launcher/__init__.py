"""The NX2navi launcher: installs the build tools, builds the game from the player's own dump, starts it and keeps
the port up to date (docs/launcher.md).

Standard library only, plus numpy for drawing the window (launcher/ui/art.py), zstandard where Python lacks
compression.zstd, and truststore for Windows' own certificate check (launcher/net.py). tkinter draws the window;
everything that does work runs without it (launcher/cli.py).
"""

NAME = 'NX2navi'
