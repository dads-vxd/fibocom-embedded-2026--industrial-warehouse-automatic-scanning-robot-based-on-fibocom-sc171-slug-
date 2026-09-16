import argparse
import random
from io import BytesIO
from pathlib import Path

import requests
from PIL import Image

LOREMFLICKR = "https://loremflickr.com/640/480/{tag}?lock={lock}"
ROOT = Path(__file__).resolve().parent
DEFAULT_OBJECT = ROOT / "1_03.png"
DEFAULT_OUT = ROOT.parents[1] / "edge_computing" / "ai_model" / "data"


def download_background(tag: str, rng: random.Random, attempts: int = 5) -> Image.Image:
    for _ in range(attempts):
        url = LOREMFLICKR.format(tag=tag, lock=rng.randint(0, 1_000_000))
        resp = requests.get(url, timeout=30)
        if resp.status_code == 200 and resp.headers.get("Content-Type", "").startswith("image/"):
            return Image.open(BytesIO(resp.content)).convert("RGB")
    raise RuntimeError(f"下载背景图失败: {url}")


def load_object(path: Path) -> Image.Image:
    if not path.exists():
        raise FileNotFoundError(f"目标图片不存在: {path}")
    img = Image.open(path).convert("RGBA")
    bbox = img.getchannel("A").getbbox()
    if bbox is not None:
        img = img.crop(bbox)
    return img


Rect = tuple[int, int, int, int]


def make_target(bg: Image.Image, obj: Image.Image, rng: random.Random, max_scale: float) -> Image.Image:
    scale = rng.uniform(0.15, max_scale)
    target_w = max(8, int(bg.width * scale))
    target_h = max(8, int(obj.height / obj.width * target_w))
    o = obj.resize((target_w, target_h), Image.BICUBIC)

    angle = rng.uniform(0, 360)
    o = o.rotate(angle, expand=True, resample=Image.BICUBIC)
    return o.crop(o.getchannel("A").getbbox())


def intersects(a: Rect, b: Rect, margin: int = 16) -> bool:
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    return not (ax + aw + margin <= bx or bx + bw + margin <= ax
                or ay + ah + margin <= by or by + bh + margin <= ay)


def place_one(bg: Image.Image, obj: Image.Image, rng: random.Random, placed: list[Rect]) -> tuple[Image.Image, int, int] | None:
    for shrink in range(8):
        max_scale = 0.6 - shrink * 0.06
        for _ in range(40):
            o = make_target(bg, obj, rng, max_scale)
            if o.width >= bg.width or o.height >= bg.height:
                continue
            x = rng.randint(0, bg.width - o.width)
            y = rng.randint(0, bg.height - o.height)
            rect: Rect = (x, y, o.width, o.height)
            if all(not intersects(rect, p) for p in placed):
                return o, x, y
    return None


def compose(bg: Image.Image, obj: Image.Image, rng: random.Random) -> tuple[Image.Image, list[tuple[float, float, float, float]]]:
    n_targets = rng.randint(3, 6)
    for _ in range(200):
        composite = bg.copy()
        placed: list[Rect] = []
        boxes: list[tuple[float, float, float, float]] = []

        while len(placed) < n_targets:
            placed_obj = place_one(bg, obj, rng, placed)
            if placed_obj is None:
                break
            o, x, y = placed_obj
            composite.paste(o, (x, y), o)
            placed.append((x, y, o.width, o.height))
            cx = (x + o.width / 2) / bg.width
            cy = (y + o.height / 2) / bg.height
            w = o.width / bg.width
            h = o.height / bg.height
            boxes.append((cx, cy, w, h))

        if len(placed) == n_targets:
            return composite, boxes
    raise RuntimeError("无法在背景图上放置 3-6 个不重叠目标")


def main() -> None:
    parser = argparse.ArgumentParser(description="生成合成 YOLO 数据集:目标随机旋转缩放贴到风景背景上")
    parser.add_argument("--object", type=Path, default=DEFAULT_OBJECT, help="目标对象图片路径")
    parser.add_argument("-n", "--count", type=int, default=100, help="生成图片数量(背景图数量)")
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT, help="数据集输出目录(内含 images/ 与 labels/)")
    parser.add_argument("--tag", type=str, default="landscape,nature", help="背景图标签,逗号分隔")
    parser.add_argument("--val-split", type=float, default=0.2, help="验证集占比")
    parser.add_argument("--seed", type=int, default=42, help="随机数种子")
    args = parser.parse_args()

    obj = load_object(args.object)
    train_dir = args.out / "images" / "train"
    val_dir = args.out / "images" / "val"
    train_label_dir = args.out / "labels" / "train"
    val_label_dir = args.out / "labels" / "val"
    for d in (train_dir, val_dir, train_label_dir, val_label_dir):
        d.mkdir(parents=True, exist_ok=True)

    rng = random.Random(args.seed)
    indices = list(range(args.count))
    rng.shuffle(indices)
    split = int(args.count * (1 - args.val_split))

    n_train = n_val = 0
    for i in range(args.count):
        bg = download_background(args.tag, rng)
        composite, boxes = compose(bg, obj, rng)

        is_val = i >= split
        img_dir = val_dir if is_val else train_dir
        label_dir = val_label_dir if is_val else train_label_dir

        name = f"syn_{i:05d}"
        composite.save(img_dir / f"{name}.jpg", quality=90)
        with open(label_dir / f"{name}.txt", "w") as f:
            for cx, cy, w, h in boxes:
                f.write(f"0 {cx:.6f} {cy:.6f} {w:.6f} {h:.6f}\n")

        if is_val:
            n_val += 1
        else:
            n_train += 1
        if (i + 1) % 10 == 0:
            print(f"已生成 {i + 1}/{args.count}")

    print(f"完成: train={n_train} val={n_val} -> {args.out.resolve()}")


if __name__ == "__main__":
    main()