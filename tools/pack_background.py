from pathlib import Path
import argparse
import hashlib
import json
from PIL import Image, ImageDraw


# ミカ：元の透明余白を除いて同じ絵を一枚のアトラスへまとめる
def pack(source, output):
    output.mkdir(parents=True, exist_ok=True)
    names = ["Aurora", "Blue Balloon", "Bridge", "Bubble1", "Castle", "Chocolate",
             "Cloud", "Cloud4", "Hot air balloon", "moon", "PineappleCloud", "Rainbow",
             "RainbowPlanet", "Red Balloon", "Road", "Saturn", "Star1", "StrawberryCloud",
             "StrawberryHouse", "StrawberryHouse2", "StrawberryHouse3", "Sun"]
    images, provenance = {}, {}
    for name in names:
        path = source / (name + ".png")
        image = Image.open(path).convert("RGBA")
        bounds = image.getbbox()
        assert bounds, name
        cropped = image.crop(bounds)
        cropped.thumbnail((960, 640), Image.Resampling.LANCZOS)
        images[name.replace(" ", "")] = cropped
        provenance[name] = {"sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                            "source_size": image.size, "alpha_bounds": bounds}
    for name, top, bottom in [
        ("DaySky", (114, 213, 245), (213, 246, 240)),
        ("EveningSky", (132, 101, 163), (255, 189, 112)),
        ("NightSky", (3, 24, 43), (21, 36, 71))]:
        image = Image.new("RGBA", (4, 256))
        for y in range(256):
            color = tuple(round(a + (b - a) * y / 255) for a, b in zip(top, bottom)) + (255,)
            for x in range(4):
                image.putpixel((x, y), color)
        images[name] = image
    hill = Image.new("RGBA", (512, 256), (255, 255, 255, 0))
    hill_mask = Image.new("L", (2048, 1024))
    ImageDraw.Draw(hill_mask).ellipse((0, 0, 2047, 1023), fill=255)
    hill.putalpha(hill_mask.resize(hill.size, Image.Resampling.LANCZOS))
    images["Hill"] = hill
    glow = Image.new("RGBA", (128, 128))
    for y in range(128):
        for x in range(128):
            distance = (((x - 63.5) / 63.5) ** 2 + ((y - 63.5) / 63.5) ** 2) ** .5
            glow.putpixel((x, y), (255, 255, 255, round(255 * max(0, 1 - distance) ** 2)))
    images["Glow"] = glow
    images["Sparkle"] = glow.resize((16, 16), Image.Resampling.LANCZOS)
    atlas = Image.new("RGBA", (4096, 2048))
    x, y, shelf, rects = 4, 4, 0, {}
    for name, image in sorted(images.items(), key=lambda pair: (-pair[1].height, pair[0])):
        w, h = image.size
        if x + w + 4 > atlas.width:
            x, y, shelf = 4, y + shelf + 8, 0
        if y + h + 4 > atlas.height:
            raise RuntimeError("Background atlas exceeds 4096x2048")
        atlas.paste(image, (x, y))
        # ミカ：線形補間で隣の素材を拾わないように端の色を余白へ複製する
        atlas.paste(image.crop((0, 0, 1, h)).resize((2, h)), (x - 2, y))
        atlas.paste(image.crop((w - 1, 0, w, h)).resize((2, h)), (x + w, y))
        atlas.paste(image.crop((0, 0, w, 1)).resize((w, 2)), (x, y - 2))
        atlas.paste(image.crop((0, h - 1, w, h)).resize((w, 2)), (x, y + h))
        rects[name] = (x, y, w, h)
        x, shelf = x + w + 8, max(shelf, h)
    atlas.save(output / "atlas.png", optimize=True)
    (output / "atlas.csv").write_text("\n".join(
        name + "," + ",".join(map(str, rect)) for name, rect in sorted(rects.items())) + "\n", encoding="utf-8")
    (output / "sources.json").write_text(json.dumps({
        "source_folder": "https://drive.google.com/drive/folders/1e2zCWMiD1E3l57l1bQ81qI-gnr_BcSp4",
        "assets": provenance}, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Packed {len(rects)} regions into 4096x2048 RGBA8 (32 MiB)")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[1] / "HEWGame/Assets/Texture/Background")
    args = parser.parse_args()
    pack(args.source, args.output)
