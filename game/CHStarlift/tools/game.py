"""STARLIFT: what the shared CHGame tools need to know (`cl run` and `cl gif` read this)."""

# One framebuffer row a DMA chunk (CHGfx's default is 2): about 500 B of RAM back.
DEFINES = ["GFX_CHUNK_ROWS=1"]
# A board perf run (cl run --device CHStarlift/tools/scripts/perf.txt): add "SL_BOT", "SL_START=n" here (the -D flag does not reach
# the board build), then delete CHStarlift/build.
TESTS = {}
# One save page (since 0.14): 256 B more for the game. A power cut in the middle of a save loses that save.
SAVE_PAGES = 1
