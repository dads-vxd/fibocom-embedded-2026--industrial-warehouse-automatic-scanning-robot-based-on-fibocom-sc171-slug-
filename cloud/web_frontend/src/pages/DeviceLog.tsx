import { useState, useEffect, useRef, useCallback } from "react";
import { barcodeApi, categoryApi } from "../api";
import type { Barcode, Category } from "../types";

export function DeviceLog() {
  const [categories, setCategories] = useState<Category[]>([]);
  const [logs, setLogs] = useState<Barcode[]>([]);
  const [connected, setConnected] = useState(false);
  const [selectedCategoryId, setSelectedCategoryId] = useState<number | null>(null);
  const [page, setPage] = useState(1);
  const [hasMore, setHasMore] = useState(true);
  const [loading, setLoading] = useState(false);
  const logContainerRef = useRef<HTMLDivElement>(null);
  const wsRef = useRef<WebSocket | null>(null);

  useEffect(() => {
    loadCategories();
    return () => {
      wsRef.current?.close();
    };
  }, []);

  useEffect(() => {
    connectWebSocket();
  }, []);

  useEffect(() => {
    loadHistory();
  }, [selectedCategoryId, page]);

  const loadCategories = async () => {
    try {
      const cats = await categoryApi.getAll();
      setCategories(cats);
    } catch (e) {
      console.error("Failed to load categories:", e);
    }
  };

  const loadHistory = async () => {
    setLoading(true);
    try {
      if (selectedCategoryId) {
        const res = await barcodeApi.getList({
          category_id: selectedCategoryId,
          page,
          page_size: 20,
        });
        if (page === 1) {
          setLogs(res.data);
        } else {
          setLogs((prev) => [...prev, ...res.data]);
        }
        setHasMore(res.data.length >= 20);
      } else {
        const cats = await categoryApi.getAll();
        const allBarcodes: Barcode[] = [];
        for (const cat of cats) {
          const res = await barcodeApi.getList({
            category_id: cat.id,
            page: 1,
            page_size: 50,
          });
          allBarcodes.push(...res.data);
        }
        allBarcodes.sort(
          (a, b) =>
            new Date(b.created_at).getTime() - new Date(a.created_at).getTime(),
        );
        setLogs(allBarcodes.slice(0, 100));
        setHasMore(false);
      }
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
          const broadcast = JSON.parse(event.data);
          if (broadcast.event === "barcode") {
            const barcode: Barcode = broadcast.data;
            setLogs((prev) => [barcode, ...prev]);
            if (logContainerRef.current) {
              logContainerRef.current.scrollTop = 0;
            }
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

  const getCategoryName = (categoryId: number) => {
    return categories.find((c) => c.id === categoryId)?.name || `#${categoryId}`;
  };

  const getCategoryFieldDefs = (categoryId: number) => {
    return categories.find((c) => c.id === categoryId)?.field_defs || [];
  };

  const handleCategoryChange = (id: number | null) => {
    setSelectedCategoryId(id);
    setPage(1);
    setLogs([]);
  };

  const loadMore = () => {
    setPage((p) => p + 1);
  };

  return (
    <div>
      <div className="flex items-center justify-between mb-6">
        <h1 className="text-2xl font-semibold text-text">设备日志</h1>
        <div className="flex items-center gap-4">
          <select
            value={selectedCategoryId ?? ""}
            onChange={(e) =>
              handleCategoryChange(
                e.target.value ? Number(e.target.value) : null,
              )
            }
            className="px-3 py-2 border border-border rounded-lg bg-surface0 text-text text-sm"
          >
            <option value="">全部分类</option>
            {categories.map((cat) => (
              <option key={cat.id} value={cat.id}>
                {cat.name}
              </option>
            ))}
          </select>
          <div className="flex items-center gap-2 text-sm">
            <span
              className={`w-2.5 h-2.5 rounded-full ${connected ? "bg-green animate-pulse" : "bg-red"}`}
            />
            <span className={connected ? "text-green" : "text-red"}>
              {connected ? "实时连接" : "未连接"}
            </span>
          </div>
        </div>
      </div>

      <div
        ref={logContainerRef}
        className="glass rounded-xl shadow-md overflow-hidden"
        style={{ maxHeight: "calc(100vh - 200px)", overflowY: "auto" }}
      >
        {loading && logs.length === 0 ? (
          <div className="flex items-center justify-center h-32 text-subtext">
            加载中...
          </div>
        ) : logs.length === 0 ? (
          <div className="flex items-center justify-center h-32 text-subtext">
            暂无日志记录
          </div>
        ) : (
          <table className="w-full">
            <thead className="sticky top-0 z-10">
              <tr className="bg-surface0/90 backdrop-blur text-subtext text-xs uppercase">
                <th className="px-4 py-3 text-left">时间</th>
                <th className="px-4 py-3 text-left">条码数据</th>
                <th className="px-4 py-3 text-left">分类</th>
                <th className="px-4 py-3 text-left">图片</th>
                <th className="px-4 py-3 text-left">字段信息</th>
              </tr>
            </thead>
            <tbody>
              {logs.map((log, idx) => {
                const isNew =
                  Date.now() - new Date(log.created_at).getTime() < 5000;
                return (
                  <tr
                    key={`${log.id}-${idx}`}
                    className={`border-b border-border/50 transition-colors ${
                      isNew
                        ? "bg-lavender/10 animate-pulse"
                        : "hover:bg-surface1/30"
                    }`}
                  >
                    <td className="px-4 py-3 text-sm text-subtext whitespace-nowrap">
                      {new Date(log.created_at).toLocaleString("zh-CN")}
                    </td>
                    <td className="px-4 py-3 text-sm text-text font-mono max-w-xs truncate">
                      {log.data}
                    </td>
                    <td className="px-4 py-3">
                      <span className="px-2 py-1 rounded-md bg-lavender/20 text-lavender text-xs">
                        {getCategoryName(log.category_id)}
                      </span>
                    </td>
                    <td className="px-4 py-3">
                      {log.image ? (
                        <img
                          src={log.image}
                          alt=""
                          className="w-10 h-10 object-cover rounded"
                        />
                      ) : (
                        <span className="text-subtext text-xs">无</span>
                      )}
                    </td>
                    <td className="px-4 py-3 text-sm">
                      {log.field_values &&
                      Object.keys(log.field_values).length > 0 ? (
                        <div className="flex flex-wrap gap-1">
                          {getCategoryFieldDefs(log.category_id).map((fd) =>
                            log.field_values[fd.key] ? (
                              <span
                                key={fd.key}
                                className="px-2 py-0.5 rounded bg-surface1/50 text-text text-xs"
                              >
                                {fd.label}: {log.field_values[fd.key]}
                              </span>
                            ) : null,
                          )}
                        </div>
                      ) : (
                        <span className="text-subtext text-xs">-</span>
                      )}
                    </td>
                  </tr>
                );
              })}
            </tbody>
          </table>
        )}

        {hasMore && !selectedCategoryId && (
          <div className="flex justify-center py-4">
            <button
              onClick={loadMore}
              disabled={loading}
              className="px-4 py-2 text-sm text-lavender hover:bg-surface1/50 rounded-lg transition-colors disabled:opacity-50"
            >
              {loading ? "加载中..." : "加载更多"}
            </button>
          </div>
        )}
      </div>
    </div>
  );
}
