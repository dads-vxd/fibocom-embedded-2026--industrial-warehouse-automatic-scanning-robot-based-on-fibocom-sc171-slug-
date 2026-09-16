import { useCallback, useRef, useState } from "react";
import type { Barcode, FieldDef } from "../types";

interface BarcodeTableProps {
  barcodes: Barcode[];
  fieldDefs: FieldDef[];
  imageBaseUrl?: string;
  onEdit?: (barcode: Barcode) => void;
  onDelete?: (id: number) => void;
}

interface PreviewState {
  src: string;
  rect: DOMRect;
}

export function BarcodeTable({
  barcodes,
  fieldDefs,
  imageBaseUrl = "",
  onEdit,
  onDelete,
}: BarcodeTableProps) {
  const [preview, setPreview] = useState<PreviewState | null>(null);
  const [closing, setClosing] = useState(false);
  const [ready, setReady] = useState(false);
  const imgRefMap = useRef<Map<number, HTMLImageElement>>(new Map());

  const getImageSrc = useCallback(
    (barcode: Barcode) =>
      barcode.image && barcode.image.startsWith("data:")
        ? barcode.image
        : `${imageBaseUrl}${barcode.image || ""}`,
    [imageBaseUrl]
  );

  const openPreview = useCallback(
    (barcode: Barcode) => {
      if (!barcode.image) return;
      const img = imgRefMap.current.get(barcode.id);
      if (!img) return;
      setClosing(false);
      setReady(false);
      setPreview({ src: getImageSrc(barcode), rect: img.getBoundingClientRect() });
      requestAnimationFrame(() => setReady(true));
    },
    [getImageSrc]
  );

  const closePreview = useCallback(() => {
    setClosing(true);
    setReady(false);
    setTimeout(() => setPreview(null), 350);
  }, []);

  const formatDate = (dateStr: string) => new Date(dateStr).toLocaleString("zh-CN");

  const getPreviewStyle = (): React.CSSProperties => {
    if (!preview) return {};
    const { rect } = preview;
    if (!ready && !closing) {
      return { position: "fixed", top: rect.top, left: rect.left, width: rect.width, height: rect.height, borderRadius: 8, transition: "all 0.35s cubic-bezier(0.25, 0.8, 0.25, 1)", zIndex: 9999 };
    }
    if (ready) {
      const maxW = window.innerWidth * 0.8;
      const maxH = window.innerHeight * 0.8;
      const scale = Math.min(maxW / rect.width, maxH / rect.height, 6);
      const finalW = rect.width * scale;
      const finalH = rect.height * scale;
      return { position: "fixed", top: (window.innerHeight - finalH) / 2, left: (window.innerWidth - finalW) / 2, width: finalW, height: finalH, borderRadius: 12, transition: "all 0.35s cubic-bezier(0.25, 0.8, 0.25, 1)", zIndex: 9999, boxShadow: "0 25px 60px rgba(0,0,0,0.5)" };
    }
    return { position: "fixed", top: rect.top, left: rect.left, width: rect.width, height: rect.height, borderRadius: 8, transition: "all 0.35s cubic-bezier(0.25, 0.8, 0.25, 1)", zIndex: 9999 };
  };

  const colClass = "text-left p-3 glass-light font-semibold text-sm text-subtext border-b-2 border-border whitespace-nowrap";
  const tdClass = "p-3 border-b border-border text-sm text-text max-w-[160px] truncate";
  const colSpan = 4 + fieldDefs.length + 1;

  return (
    <div className="overflow-x-auto">
      <table className="w-full">
        <thead>
          <tr>
            <th className={colClass}>ID</th>
            <th className={colClass}>条码数据</th>
            <th className={colClass}>图片</th>
            {fieldDefs.map((fd) => (
              <th key={fd.key} className={colClass}>{fd.label}</th>
            ))}
            <th className={colClass}>创建时间</th>
            <th className={colClass}>操作</th>
          </tr>
        </thead>
        <tbody>
          {barcodes.map((barcode) => (
            <tr key={barcode.id} className="hover:bg-surface1/30">
              <td className="p-3 border-b border-border text-sm text-text">{barcode.id}</td>
              <td className={`${tdClass} font-mono text-xs`}>{barcode.data}</td>
              <td className="p-3 border-b border-border">
                {barcode.image ? (
                  <img
                    ref={(el) => { if (el) imgRefMap.current.set(barcode.id, el); }}
                    src={getImageSrc(barcode)}
                    alt="barcode"
                    className="object-cover rounded-lg glass-light cursor-pointer hover:opacity-80 transition-opacity"
                    style={{ width: "48px", height: "48px" }}
                    onClick={() => openPreview(barcode)}
                  />
                ) : (
                  <div className="glass-light rounded-lg flex items-center justify-center text-subtext text-xs" style={{ width: "48px", height: "48px" }}>无</div>
                )}
              </td>
              {fieldDefs.map((fd) => (
                <td key={fd.key} className={tdClass}>
                  {(barcode.field_values as Record<string, string>)?.[fd.key] || "-"}
                </td>
              ))}
              <td className="p-3 border-b border-border text-sm text-subtext">{formatDate(barcode.created_at)}</td>
              <td className="p-3 border-b border-border text-sm whitespace-nowrap">
                {onEdit && <button onClick={() => onEdit(barcode)} className="text-blue hover:text-lavender mr-3">编辑</button>}
                {onDelete && <button onClick={() => onDelete(barcode.id)} className="text-red hover:text-peach">删除</button>}
              </td>
            </tr>
          ))}
          {barcodes.length === 0 && (
            <tr>
              <td colSpan={colSpan} className="p-8 text-center text-subtext">暂无数据</td>
            </tr>
          )}
        </tbody>
      </table>
      {preview && (
        <>
          <div className="fixed inset-0 bg-black/60" style={{ zIndex: 9998, opacity: ready && !closing ? 1 : 0, transition: "opacity 0.35s ease" }} onClick={closePreview} />
          {/* eslint-disable-next-line jsx-a11y/no-noninteractive-element-interactions */}
          <img src={preview.src} alt="barcode preview" className="object-contain" style={getPreviewStyle()} onClick={closePreview} onKeyDown={(e) => e.key === "Escape" && closePreview()} />
        </>
      )}
    </div>
  );
}
