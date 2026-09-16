import argparse
from pathlib import Path

from ultralytics import YOLO

DATA_YAML = Path(__file__).resolve().parent.parent / "data" / "data.yaml"


def main() -> None:
    parser = argparse.ArgumentParser(description="YOLO 目标检测训练")
    parser.add_argument("--data", type=str, default=str(DATA_YAML), help="数据集 data.yaml 路径")
    parser.add_argument("--model", type=str, default="yolo11n.pt", help="预训练模型权重")
    parser.add_argument("--epochs", type=int, default=100, help="训练轮数")
    parser.add_argument("--imgsz", type=int, default=640, help="输入图片尺寸")
    args = parser.parse_args()

    model = YOLO(args.model)
    model.train(data=args.data, epochs=args.epochs, imgsz=args.imgsz)


if __name__ == "__main__":
    main()