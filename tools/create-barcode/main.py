"""二维码与条形码标签图片生成器。

读取 data.json，根据 category_id 选择对应字段，按指定布局生成包含
元数据、二维码、条形码及条形码内容的标签图片。
所有尺寸、字号、行列位置均可通过 Config 超参数调整。
"""

import json
from dataclasses import dataclass, field
from io import BytesIO
from pathlib import Path

import barcode
import qrcode
from barcode.writer import ImageWriter
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent
DATA_FILE = ROOT / "data.json"
OUTPUT_DIR = ROOT / "output"


# ==================== 可调超参数 ====================
@dataclass
class Config:
    """集中管理所有可调超参数（含各元素行列位置）。"""

    # ---- 图片整体大小（像素）----
    image_width: int = 1980
    image_height: int = 640

    # ---- 元数据列位置（x 中心坐标）----
    col1_x: int = 30  # 第一列：种类名称 / 公司名称
    col2_x: int = 500  # 第二列：数量重量 / 型号规格
    col3_x: int = 960  # 第三列：辅料代码 / 生产日期

    # ---- 元数据行位置（y 中心坐标）----
    row1_y: int = 80  # 第一行：种类名称、数量/重量、辅料代码
    row2_y: int = 210  # 第二行：公司名称、型号/规格、生产日期

    # ---- 二维码（左上角坐标 + 正方形边长）----
    qr_x: int = 1300
    qr_y: int = 40
    qr_size: int = 560

    # ---- 条形码（左上角坐标 + 宽高）----
    barcode_x: int = 30
    barcode_y: int = 300
    barcode_width: int = 1280
    barcode_height: int = 200

    # ---- 条形码内容文本（中心坐标）----
    barcode_text_x: int = 650
    barcode_text_y: int = 500

    # ---- 字体大小（像素）----
    metadata_font_size: int = 36  # 元数据字体
    barcode_text_font_size: int = 32  # 条形码内容文本字体

    # ---- 配色 ----
    background_color: str = "white"
    text_color: str = "black"

    # ---- 字段值网格映射 ----
    # 按 JSON fields 中的出现顺序，第 i 个值放到 (列索引, 行索引) 对应位置。
    # 列: 0=col1_x 1=col2_x 2=col3_x；行: 0=row1_y 1=row2_y
    # 布局：
    #   [种类名称] [field0] [field1]   ← row1
    #   [field2]   [field3] [field4]   ← row2
    field_grid: list = field(default_factory=lambda: [
        (1, 0), (2, 0), (0, 1), (1, 1), (2, 1),
    ])


# ==================== 种类名称映射（占位，后续补充）====================
CATEGORY_NAMES: dict[int, str] = {
    1: "七匹狼(纯雅)条盒(二维1)",
    2: "七匹狼(纯雅)小盒(二维1)",
    3: "七匹狼(纯雅)内衬纸-1",
    4: "七匹狼(纯雅)框架纸(厦门)",
}


# ==================== 字体加载 ====================
_FONT_CANDIDATES = [
    "C:/Windows/Fonts/msyh.ttc",
    "C:/Windows/Fonts/msyhbd.ttc",
    "C:/Windows/Fonts/simhei.ttf",
    "C:/Windows/Fonts/simsun.ttc",
]
_font_cache: dict[int, ImageFont.FreeTypeFont] = {}


def get_font(size: int) -> ImageFont.FreeTypeFont:
    if size not in _font_cache:
        for p in _FONT_CANDIDATES:
            if Path(p).exists():
                _font_cache[size] = ImageFont.truetype(p, size)
                break
        else:
            _font_cache[size] = ImageFont.load_default()
    return _font_cache[size]


# ==================== 字段解析 ====================
def resolve_fields(item: dict) -> dict:
    """按 JSON 中的出现顺序提取字段值，不依赖 key 名称。"""
    cid = item["category_id"]
    return {
        "category_name": CATEGORY_NAMES.get(cid, f"种类{cid}"),
        "field_values": list(item["fields"].values()),
        "data": item.get("data", ""),
    }


# ==================== 二维码生成 ====================
def generate_qr(data: str, size: int) -> Image.Image:
    qr = qrcode.QRCode(
        error_correction=qrcode.constants.ERROR_CORRECT_M,
        box_size=10,
        border=2,
    )
    qr.add_data(data)
    qr.make(fit=True)
    img = qr.make_image(fill_color="black", back_color="white").convert("RGB")
    return img.resize((size, size), Image.NEAREST)


# ==================== 条形码生成 ====================
def generate_barcode(data: str, width: int, height: int) -> Image.Image:
    code128 = barcode.get_barcode_class("code128")
    obj = code128(data, writer=ImageWriter())
    fp = BytesIO()
    obj.write(
        fp,
        options={
            "write_text": False,
            "module_width": 0.15,
            "module_height": 15.0,
            "quiet_zone": 1.0,
        },
    )
    fp.seek(0)
    img = Image.open(fp).convert("RGB")
    return img.resize((width, height), Image.NEAREST)


# ==================== 文本绘制辅助 ====================
def _draw_centered(draw, text, font, cx, cy, fill="black"):
    bbox = draw.textbbox((0, 0), text, font=font)
    w, h = bbox[2] - bbox[0], bbox[3] - bbox[1]
    draw.text((cx - w / 2, cy - h / 2), text, font=font, fill=fill)


def _draw_left(draw, text, font, x, cy, fill="black"):
    """左对齐（x 为文本左边缘），垂直居中（cy 为中心）。"""
    bbox = draw.textbbox((0, 0), text, font=font)
    h = bbox[3] - bbox[1]
    draw.text((x, cy - h / 2), text, font=font, fill=fill)


# ==================== 主合成函数 ====================
def compose_label(item: dict, config: Config) -> Image.Image:
    fields = resolve_fields(item)
    W, H = config.image_width, config.image_height

    canvas = Image.new("RGB", (W, H), config.background_color)
    draw = ImageDraw.Draw(canvas)

    meta_font = get_font(config.metadata_font_size)
    bc_font = get_font(config.barcode_text_font_size)
    tc = config.text_color

    # ---- 元数据（左对齐，按 JSON 顺序映射到网格位置）----
    cols = [config.col1_x, config.col2_x, config.col3_x]
    rows = [config.row1_y, config.row2_y]

    # 种类名称固定在 (col1, row1)
    _draw_left(draw, fields["category_name"], meta_font, cols[0], rows[0], tc)

    # 其余字段按 JSON 出现顺序，经 field_grid 映射到 (列, 行)
    for i, val in enumerate(fields["field_values"]):
        if i >= len(config.field_grid):
            break
        col_i, row_i = config.field_grid[i]
        _draw_left(draw, val, meta_font, cols[col_i], rows[row_i], tc)

    # ---- 二维码 ----
    qr_img = generate_qr(fields["data"], config.qr_size)
    canvas.paste(qr_img, (config.qr_x, config.qr_y))

    # ---- 条形码 ----
    bc_img = generate_barcode(
        fields["data"], config.barcode_width, config.barcode_height
    )
    canvas.paste(bc_img, (config.barcode_x, config.barcode_y))

    # ---- 条形码内容文本 ----
    _draw_centered(
        draw, fields["data"], bc_font, config.barcode_text_x, config.barcode_text_y, tc
    )

    return canvas


# ==================== 主流程 ====================
def main():
    config = Config()
    items = json.loads(DATA_FILE.read_text(encoding="utf-8"))
    OUTPUT_DIR.mkdir(exist_ok=True)

    for idx, item in enumerate(items, start=1):
        img = compose_label(item, config)
        cid = item["category_id"]
        out_path = OUTPUT_DIR / f"{cid}_{idx:02d}.png"
        img.save(out_path)
        print(f"已生成: {out_path.name}")

    print(f"完成，共 {len(items)} 张，输出目录: {OUTPUT_DIR}")


if __name__ == "__main__":
    main()
