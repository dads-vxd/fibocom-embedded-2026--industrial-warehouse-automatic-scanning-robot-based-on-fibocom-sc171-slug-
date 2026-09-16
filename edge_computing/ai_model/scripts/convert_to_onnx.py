import argparse
from pathlib import Path

from ultralytics import YOLO


def main() -> None:
    parser = argparse.ArgumentParser(description="将 YOLO .pt 权重转换为 ONNX")
    parser.add_argument("--weights", type=str, required=True, help="模型权重路径,例如 best.pt")
    parser.add_argument("--imgsz", type=int, default=640, help="输入图片尺寸")
    args = parser.parse_args()

    weights = Path(args.weights)
    if not weights.exists():
        raise FileNotFoundError(f"权重文件不存在: {weights}")

    model = YOLO(str(weights))
    output = model.export(format="onnx", imgsz=args.imgsz)
    print(f"ONNX 模型已保存到: {output}")


if __name__ == "__main__":
    main()