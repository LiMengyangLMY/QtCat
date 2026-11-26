"""Generate Minecraft-style pixel art sprites for the QtCat runner mini-game.

Outputs crisp nearest-neighbor PNGs into resources/assets/game/:
  - cactus.png        (single blocky pixel cactus)
  - cactus_cluster.png(double cactus obstacle)
  - cucumber.png      (pixel cucumber, used on ground & mid-air)
  - blue_fish.png     (Minecraft-style blue dried fish collectible)
"""

from __future__ import annotations

from pathlib import Path
from PIL import Image


OUT_DIR = Path(__file__).resolve().parent.parent / "resources" / "assets" / "game"


def render_grid(rows: list[str], palette: dict[str, tuple[int, int, int, int]], scale: int = 4) -> Image.Image:
    h = len(rows)
    w = max(len(r) for r in rows)
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    px = img.load()
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch != "." and ch in palette:
                px[x, y] = palette[ch]
    return img.resize((w * scale, h * scale), Image.NEAREST)


# 1. Single Cactus (16x18 grid -> 64x72)
CACTUS_PAL = {
    "k": (18, 46, 20, 255),      # dark pixel outline
    "s": (34, 92, 39, 255),      # shadow green
    "m": (56, 142, 60, 255),     # mid green
    "l": (102, 187, 106, 255),   # highlight green
    "p": (230, 242, 181, 255),   # pixel spine
    "t": (141, 78, 53, 255),     # terracotta pot mid
    "d": (98, 50, 33, 255),      # terracotta pot shadow
    "h": (179, 106, 76, 255),    # terracotta pot highlight
    "b": (58, 30, 20, 255),      # pot dark outline
}

CACTUS_GRID = [
    "......kkkk......",
    ".....klmmlk.....",
    "....pklsmskp....",
    ".....klmmlk.....",
    ".kkk.klsmsk.....",
    "klmk.klmmlk.kkk.",
    "klsk.klsmsk.klmk",
    "klmk.klmmlk.klsk",
    "klskkklmmskkklmk",
    "kllmmmlmmlmmmskk",
    ".kkkkklmmskkkkk.",
    "....pklsmskp....",
    ".....klmmlk.....",
    ".....klsmsk.....",
    "....bbbbbbbb....",
    "...bhttttttd b...".replace(" ", ""),
    "....bhttttdb....",
    ".....bbbbbb.....",
]

# 2. Double Cactus Cluster (20x18 grid -> 80x72)
CACTUS_CLUSTER_GRID = [
    "....kkkk............",
    "...klmmlk...........",
    "..pklsmskp...kkkk...",
    "...klmmlk...klmmlk..",
    "kkkklsmsk...klsmskp.",
    "klmklmmlk...klmmlk..",
    "klsklsmskk..klsmsk..",
    "kllmmlmmskk.klmmlkkk",
    ".kkkklmmlk..klmmslmk",
    "....klsmsk..klsmskk.",
    "...pklsmsk..klmmlk..",
    "....klmmlk.pklsmsk..",
    "....klsmsk..klmmlk..",
    "....klmmlk..klsmsk..",
    "...bbbbbbbbbbbbbbbb.",
    "..bhtttttttttttttdb.",
    "...bhtttttttttttdb..",
    "....bbbbbbbbbbbbbb..",
]

# 3. Pixel Cucumber (18x12 grid -> 72x48) — Horizontal bumpy cucumber, great for ground & flying!
CUCUMBER_PAL = {
    "k": (15, 43, 18, 255),      # dark outline
    "s": (27, 87, 34, 255),      # deep cucumber green
    "m": (48, 138, 56, 255),     # mid cucumber green
    "l": (88, 186, 82, 255),     # fresh highlight green
    "h": (167, 235, 138, 255),   # specular wart/bump highlight
    "w": (110, 71, 35, 255),     # stem brown
    "d": (64, 39, 18, 255),      # stem dark
}

CUCUMBER_GRID = [
    "..............kkkk..",
    "..........kkkkllhmkw",
    ".......kkkllhlmmmskd",
    "....kkkllmmmlmmmsskk",
    "..kkllhmmmmhmmmsskk.",
    ".klmmmmmmhmmmmsskk..",
    "klmmhmmmmmmmssskk...",
    "ksmmmmmmmsssskk.....",
    ".kksssssssskk.......",
    "...kkkkkkkk.........",
]

# 4. Minecraft-style Blue Dried Fish (18x14 grid -> 72x56)
BLUE_FISH_PAL = {
    "k": (12, 28, 58, 255),      # deep navy outline
    "d": (23, 62, 138, 255),     # dark ocean blue
    "m": (37, 99, 235, 255),     # royal lapis blue
    "l": (56, 189, 248, 255),    # cyan-blue scale highlight
    "h": (186, 230, 253, 255),   # icy specular gleam
    "e": (10, 14, 26, 255),      # eye pupil
    "w": (240, 249, 255, 255),   # eye sparkle
}

BLUE_FISH_GRID = [
    ".......kkkk.......",
    ".....kklllmkk.....",
    "....kllhllmmmk..kk",
    "..kkllhlmlmmmmkkhk",
    ".kllhmmlmlmmmmkllk",
    "kllweklmmlmmmmdlmk",
    "klllmmmlmlmmmddmk.",
    ".kdmmmmmmmmmdddk..",
    "..kdddmmmddddmdlmk",
    "...kkkkdddkkk.kddk",
    "......kkkk.....kk.",
]


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    sprites = {
        "cactus.png": render_grid(CACTUS_GRID, CACTUS_PAL, scale=4),
        "cactus_cluster.png": render_grid(CACTUS_CLUSTER_GRID, CACTUS_PAL, scale=4),
        "cucumber.png": render_grid(CUCUMBER_GRID, CUCUMBER_PAL, scale=4),
        "blue_fish.png": render_grid(BLUE_FISH_GRID, BLUE_FISH_PAL, scale=4),
    }

    for name, img in sprites.items():
        img.save(OUT_DIR / name)
        print(f"Saved {name}: {img.size}")


if __name__ == "__main__":
    main()
