import { useState, useEffect, useCallback } from "react";
import { videoApi } from "../api";
import type { Video } from "../types";

export function Videos() {
  const [videos, setVideos] = useState<Video[]>([]);
  const [total, setTotal] = useState(0);
  const [page, setPage] = useState(1);
  const [loading, setLoading] = useState(true);
  const [selected, setSelected] = useState<Video | null>(null);
  const pageSize = 12;

  useEffect(() => {
    loadVideos();
  }, [page]);

  const loadVideos = async () => {
    setLoading(true);
    try {
      const res = await videoApi.getList({ page, page_size: pageSize });
      setVideos(res.data);
      setTotal(res.total);
    } catch (e) {
      console.error("Failed to load videos:", e);
    } finally {
      setLoading(false);
    }
  };

  const totalPages = Math.ceil(total / pageSize);

  const handleDelete = async (id: number, e: React.MouseEvent) => {
    e.stopPropagation();
    if (!confirm("确认删除此视频？")) return;
    try {
      await videoApi.delete(id);
      loadVideos();
    } catch (e) {
      console.error("Failed to delete video:", e);
    }
  };

  const openModal = (video: Video) => setSelected(video);
  const closeModal = () => setSelected(null);

  const handleKeyDown = useCallback(
    (e: KeyboardEvent) => {
      if (e.key === "Escape") closeModal();
    },
    [],
  );

  useEffect(() => {
    document.addEventListener("keydown", handleKeyDown);
    return () => document.removeEventListener("keydown", handleKeyDown);
  }, [handleKeyDown]);

  const formatDate = (dateStr: string) => {
    const d = new Date(dateStr);
    return d.toLocaleString("zh-CN", {
      month: "2-digit",
      day: "2-digit",
      hour: "2-digit",
      minute: "2-digit",
      second: "2-digit",
    });
  };

  const formatSize = (bytes: number) => {
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB";
    return (bytes / (1024 * 1024)).toFixed(1) + " MB";
  };

  return (
    <div>
      <div className="flex items-center justify-between mb-6">
        <h1 className="text-2xl font-semibold text-text">视频记录</h1>
        <span className="text-sm text-subtext">共 {total} 个视频</span>
      </div>

      {loading && videos.length === 0 ? (
        <div className="glass rounded-xl p-8 text-center text-subtext">
          加载中...
        </div>
      ) : videos.length === 0 ? (
        <div className="glass rounded-xl p-8 text-center text-subtext">
          暂无视频记录
        </div>
      ) : (
        <>
          <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-4">
            {videos.map((video) => (
              <div
                key={video.id}
                onClick={() => openModal(video)}
                className="glass rounded-xl overflow-hidden cursor-pointer group transition-all hover:scale-[1.02] hover:shadow-lg"
              >
                <div className="relative aspect-video bg-black/20">
                  <video
                    src={video.url}
                    className="w-full h-full object-cover"
                    preload="metadata"
                    muted
                  />
                  <div className="absolute inset-0 flex items-center justify-center opacity-0 group-hover:opacity-100 transition-opacity bg-black/30">
                    <svg
                      className="w-12 h-12 text-white/80"
                      fill="currentColor"
                      viewBox="0 0 24 24"
                    >
                      <path d="M8 5v14l11-7z" />
                    </svg>
                  </div>
                  <span className="absolute bottom-1.5 right-1.5 px-1.5 py-0.5 bg-black/70 text-white text-xs rounded">
                    {video.duration}s
                  </span>
                </div>
                <div className="p-3">
                  <div className="text-xs text-subtext mb-1">
                    {formatDate(video.created_at)}
                  </div>
                  <div className="flex items-center justify-between">
                    <span className="text-xs text-subtext">
                      {formatSize(video.size)}
                    </span>
                    <button
                      onClick={(e) => handleDelete(video.id, e)}
                      className="text-xs text-subtext hover:text-red transition-colors"
                    >
                      删除
                    </button>
                  </div>
                </div>
              </div>
            ))}
          </div>

          {totalPages > 1 && (
            <div className="flex items-center justify-center gap-4 mt-6">
              <button
                onClick={() => setPage((p) => Math.max(1, p - 1))}
                disabled={page <= 1}
                className="px-4 py-2 text-sm glass rounded-lg text-text hover:bg-surface1/50 disabled:opacity-40 disabled:cursor-not-allowed transition-colors"
              >
                上一页
              </button>
              <span className="text-sm text-subtext">
                {page} / {totalPages}
              </span>
              <button
                onClick={() => setPage((p) => Math.min(totalPages, p + 1))}
                disabled={page >= totalPages}
                className="px-4 py-2 text-sm glass rounded-lg text-text hover:bg-surface1/50 disabled:opacity-40 disabled:cursor-not-allowed transition-colors"
              >
                下一页
              </button>
            </div>
          )}
        </>
      )}

      {selected && (
        <div
          className="fixed inset-0 z-50 flex items-center justify-center bg-black/80"
          onClick={closeModal}
        >
          <div
            className="relative w-full max-w-5xl mx-4"
            onClick={(e) => e.stopPropagation()}
          >
            <button
              onClick={closeModal}
              className="absolute -top-10 right-0 text-white/70 hover:text-white text-sm transition-colors"
            >
              关闭 (ESC)
            </button>
            <div className="glass rounded-xl overflow-hidden">
              <video
                key={selected.url}
                src={selected.url}
                className="w-full max-h-[80vh]"
                controls
                autoPlay
                playsInline
              />
              <div className="p-4 flex items-center justify-between">
                <div className="text-sm text-text">
                  {formatDate(selected.created_at)} &middot; {selected.duration}秒
                  &middot; {formatSize(selected.size)}
                </div>
                <button
                  onClick={() => {
                    handleDelete(selected.id, {
                      stopPropagation: () => {},
                    } as React.MouseEvent);
                    closeModal();
                  }}
                  className="px-3 py-1.5 text-sm border border-red/30 text-red rounded-lg hover:bg-red/10 transition-colors"
                >
                  删除
                </button>
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
