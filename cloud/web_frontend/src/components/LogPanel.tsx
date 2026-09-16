import { useState, useEffect, useRef, useCallback } from "react";
import { barcodeApi, categoryApi } from "../api";
import type { Barcode, Video, WSBroadcast } from "../types";

const MIN_HEIGHT = 32;
const MAX_HEIGHT = 600;
const DEFAULT_HEIGHT = 220;
const MAX_LOGS = 100;

type LogEntry =
  | { type: "barcode"; data: Barcode }
  | { type: "video"; data: Video };

async function fetchRecentLogs(): Promise<LogEntry[]> {
  const cats = await categoryApi.getAll();
  const allBarcodes: Barcode[] = [];
  for (const cat of cats) {
    const res = await barcodeApi.getList({
      category_id: cat.id,
      page: 1,
      page_size: MAX_LOGS,
    });
    allBarcodes.push(...res.data);
  }
  allBarcodes.sort(
    (a, b) =>
      new Date(a.created_at).getTime() - new Date(b.created_at).getTime(),
  );

  const entries: LogEntry[] = allBarcodes
    .slice(-MAX_LOGS)
    .map((b) => ({ type: "barcode" as const, data: b }));
  return entries;
}

export function LogPanel() {
  const [logs, setLogs] = useState<LogEntry[]>([]);
  const [connected, setConnected] = useState(false);
  const [collapsed, setCollapsed] = useState(false);
  const [loading, setLoading] = useState(true);
  const [height, setHeight] = useState(DEFAULT_HEIGHT);
  const dragging = useRef(false);
  const wsRef = useRef<WebSocket | null>(null);
  const logEndRef = useRef<HTMLDivElement>(null);
  const scrollContainerRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    loadHistory();
  }, []);

  useEffect(() => {
    connectWebSocket();
    return () => {
      wsRef.current?.close();
    };
  }, []);

  useEffect(() => {
    if (!collapsed && logs.length > 0) {
      logEndRef.current?.scrollIntoView({ behavior: "smooth" });
    }
  }, [logs.length, collapsed]);

  const loadHistory = async () => {
    setLoading(true);
    try {
      const recent = await fetchRecentLogs();
      setLogs(recent);
    } catch (e) {
      console.error("Failed to load history:", e);
    } finally {
      setLoading(false);
    }
  };

  const connectWebSocket = useCallback(() => {
    const wsUrl = `${window.location.protocol === "https:" ? "wss:" : "ws:"}//${window.location.host}/ws/barcode`;
    const fallbackUrl = `${import.meta.env.VITE_WS_URL || "ws://localhost:3000"}/ws/barcode`;

    const tryConnect = (url: string) => {
      const ws = new WebSocket(url);
      wsRef.current = ws;

      ws.onopen = () => {
        setConnected(true);
      };

      ws.onmessage = (event) => {
        try {
          const broadcast: WSBroadcast = JSON.parse(event.data);
          if (broadcast.event === "barcode") {
            const barcode = broadcast.data as Barcode;
            const entry: LogEntry = { type: "barcode", data: barcode };
            setLogs((prev) =>
              [...prev, entry].slice(-MAX_LOGS),
            );
          } else if (broadcast.event === "video_uploaded") {
            const video = broadcast.data as Video;
            const entry: LogEntry = { type: "video", data: video };
            setLogs((prev) =>
              [...prev, entry].slice(-MAX_LOGS),
            );
          }
        } catch (e) {
          console.error("Failed to parse ws message:", e);
        }
      };

      ws.onclose = () => {
        setConnected(false);
        setTimeout(() => {
          if (wsRef.current === ws) {
            tryConnect(url);
          }
        }, 3000);
      };

      ws.onerror = () => {
        ws.close();
      };
    };

    if (window.location.hostname === "localhost") {
      tryConnect(fallbackUrl);
    } else {
      tryConnect(wsUrl);
    }
  }, []);

  const handleMouseDown = useCallback(
    (e: React.MouseEvent) => {
      e.preventDefault();
      dragging.current = true;
      const startY = e.clientY;
      const startH = height;

      const onMouseMove = (ev: MouseEvent) => {
        if (!dragging.current) return;
        const delta = startY - ev.clientY;
        const newH = Math.min(MAX_HEIGHT, Math.max(MIN_HEIGHT, startH + delta));
        setHeight(newH);
      };

      const onMouseUp = () => {
        dragging.current = false;
        document.removeEventListener("mousemove", onMouseMove);
        document.removeEventListener("mouseup", onMouseUp);
      };

      document.addEventListener("mousemove", onMouseMove);
      document.addEventListener("mouseup", onMouseUp);
    },
    [height],
  );

  const toggleCollapse = () => {
    setCollapsed((prev) => !prev);
  };

  const clearLogs = () => {
    setLogs([]);
  };

  return (
    <div
      className="glass-strong border-t border-border flex flex-col select-none"
      style={{ height: collapsed ? MIN_HEIGHT : height }}
    >
      <div
        className="flex items-center justify-between px-4 py-1.5 cursor-row-resize shrink-0"
        onMouseDown={handleMouseDown}
      >
        <div className="flex items-center gap-3">
          <div className="flex flex-col gap-0.5">
            <div className="w-6 h-0.5 bg-subtext/50 rounded-full" />
            <div className="w-6 h-0.5 bg-subtext/50 rounded-full" />
          </div>
          <span className="text-sm font-medium text-text">
            设备日志
          </span>
          <div className="flex items-center gap-1.5 text-xs">
            <span
              className={`w-2 h-2 rounded-full ${connected ? "bg-green animate-pulse" : "bg-red"}`}
            />
            <span className={connected ? "text-green" : "text-red"}>
              {connected ? "已连接" : "未连接"}
            </span>
            <span className="text-subtext ml-2">
              {logs.length} 条记录
            </span>
          </div>
        </div>
        <div className="flex items-center gap-2">
          {!collapsed && (
            <>
              <button
                onClick={(e) => {
                  e.stopPropagation();
                  loadHistory();
                }}
                disabled={loading}
                className="px-2 py-0.5 text-xs text-subtext hover:text-blue border border-border rounded transition-colors disabled:opacity-50"
              >
                {loading ? "加载中..." : "刷新"}
              </button>
              <button
                onClick={(e) => {
                  e.stopPropagation();
                  clearLogs();
                }}
                className="px-2 py-0.5 text-xs text-subtext hover:text-red border border-border rounded transition-colors"
              >
                清空
              </button>
            </>
          )}
          <button
            onClick={(e) => {
              e.stopPropagation();
              toggleCollapse();
            }}
            className="px-2 py-0.5 text-xs text-subtext hover:text-lavender border border-border rounded transition-colors"
          >
            {collapsed ? "展开" : "收起"}
          </button>
        </div>
      </div>

      {!collapsed && (
        <div
          ref={scrollContainerRef}
          className="flex-1 overflow-auto px-4 pb-2 font-mono text-xs leading-relaxed"
        >
          {loading && logs.length === 0 ? (
            <div className="flex items-center justify-center h-full text-subtext text-sm">
              加载中...
            </div>
          ) : logs.length === 0 ? (
            <div className="flex items-center justify-center h-full text-subtext text-sm">
              等待日志...
            </div>
          ) : (
            logs.map((log, idx) => {
              const isNew =
                idx === logs.length - 1 &&
                Date.now() -
                  new Date(
                    log.type === "barcode"
                      ? log.data.created_at
                      : log.data.created_at,
                  ).getTime() <
                  5000;

              if (log.type === "video") {
                const v = log.data as Video;
                return (
                  <div
                    key={`video-${v.id}-${idx}`}
                    className={`py-1 border-b border-border/30 ${
                      isNew ? "bg-lavender/10" : ""
                    }`}
                  >
                    <pre className="text-text whitespace-pre-wrap break-all">
{JSON.stringify(
  {
    type: "video_uploaded",
    id: v.id,
    filename: v.filename,
    duration: v.duration,
    size: v.size,
    created_at: v.created_at,
  },
  null,
  2,
)}
                    </pre>
                  </div>
                );
              }

              const b = log.data as Barcode;
              const isImageLong = b.image && b.image.length > 200;
              return (
                <div
                  key={`barcode-${b.id}-${idx}`}
                  className={`py-1 border-b border-border/30 ${
                    isNew ? "bg-lavender/10" : ""
                  }`}
                >
                  <div className="flex items-start gap-2">
                    {b.image && (
                      <img
                        src={b.image}
                        alt=""
                        className="w-8 h-8 object-cover rounded shrink-0 mt-0.5"
                      />
                    )}
                    <pre className="text-text whitespace-pre-wrap break-all flex-1 min-w-0">
{JSON.stringify(
  {
    type: "barcode",
    id: b.id,
    data: b.data,
    category_id: b.category_id,
    field_values: b.field_values,
    image: isImageLong ? "[图片]" : (b.image || null),
    created_at: b.created_at,
  },
  null,
  2,
)}
                    </pre>
                  </div>
                </div>
              );
            })
          )}
          <div ref={logEndRef} />
        </div>
      )}
    </div>
  );
}
