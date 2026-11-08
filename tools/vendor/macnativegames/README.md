# MacNativeGames extraction scripts

Unmodified copies from https://github.com/ly05010419/MacNativeGames at `b75be8cfa704df3701e86130c8ced268e79d6b11`, MIT (see `LICENSE`).

| File | Upstream path |
|---|---|
| `extract_exefs.py` | `scripts/extract_exefs.py` |
| `extract_romfs.py` | `scripts/extract_romfs.py` |
| `extract_update_romfs.py` | `scripts/extract_update_romfs.py` |
| `extract_shared_fonts.py` | `scripts/extract_shared_fonts.py` |

`tools/data/expected-v*.json` come from `assets/` at the same commit.

Keep these files byte-identical to upstream so they can be updated by copying. Windows-specific behaviour lives in `tools/extract.py` and `tools/extract_fonts.py`.
