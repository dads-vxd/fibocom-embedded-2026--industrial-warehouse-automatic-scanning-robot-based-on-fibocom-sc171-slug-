import { useState, useEffect, useRef, useCallback } from "react";

interface AlarmLog {
  message: string;
  time: string;
}

type Command = "forward" | "backward" | "left" | "right" | "pause";

const buildWsUrl = (path: string) => {
  const proto = window.location.protocol === "https:" ? "wss:" : "ws:";
  const direct = `${proto}//${window.location.host}${path}`;
  const fallback = `${import.meta.env.VITE_WS_URL || "ws://localhost:3000"}${path}`;
  return window.location.hostname === "localhost" ? fallback : direct;
};

function ControlButton({
  children,
  onClick,
  variant = "default",
  disabled,
}: {
  children: React.ReactNode;
  onClick?: () => void;
  variant?: "default" | "accent" | "danger";
  disabled?: boolean;
}) {
  const variantClass =
    variant === "accent"
      ? "glass bg-lavender/30 hover:bg-lavender/40 text-text"
      : variant === "danger"
        ? "glass hover:bg-red/10 text-text hover:border-red"
        : "glass hover:bg-surface1/50 text-text";
  return (
    <button
      onClick={onClick}
      disabled={disabled}
      className={`h-16 rounded-xl border border-border font-medium text-base transition-colors cursor-pointer active:scale-[0.98] disabled:opacity-40 disabled:cursor-not-allowed ${variantClass}`}
    >
      {children}
    </button>
  );
}

export function ManualControl() {
  const [connected, setConnected] = useState(false);
  const [logs, setLogs] = useState<AlarmLog[]>([]);
  const wsRef = useRef<WebSocket | null>(null);

  const connectWs = useCallback(() => {
    const tryConnect = () => {
      const ws = new WebSocket(buildWsUrl("/ws/control"));
      wsRef.current = ws;

      ws.onopen = () => setConnected(true);

      ws.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data);
          if (data.log) {
            const time = new Date().toLocaleString("zh-CN");
            setLogs((prev) => [{ message: String(data.log), time }, ...prev]);
          }
        } catch (e) {
          console.error("Failed to parse control ws message:", e);
        }
      };

      ws.onclose = () => {
        setConnected(false);
        setTimeout(() => {
          if (wsRef.current === ws) tryConnect();
        }, 3000);
      };

      ws.onerror = () => {
        ws.close();
      };
    };
    tryConnect();
  }, []);

  useEffect(() => {
    connectWs();
    return () => {
      wsRef.current?.close();
    };
  }, [connectWs]);

  const sendCommand = (command: Command) => {
    const ws = wsRef.current;
    if (!ws || ws.readyState !== WebSocket.OPEN) return;
    ws.send(JSON.stringify({ command }));
  };

  const reconnect = () => {
    wsRef.current?.close();
    connectWs();
  };

  const clearLogs = () => setLogs([]);

  return (
    <div>
      <h1 className="text-2xl font-semibold mb-6 text-text">手动控制</h1>

      <div className="grid grid-cols-2 gap-4 mb-6">
        {/* ===== 连接状态 ===== */}
        <div className="glass rounded-xl p-6 shadow-md flex items-center gap-4">
          <span
            className={`w-3 h-3 rounded-full ${connected ? "bg-green animate-pulse" : "bg-red"}`}
          />
          <div>
            <div className="text-sm text-subtext">设备连接状态</div>
            <div
              className={`text-lg font-semibold ${connected ? "text-green" : "text-red"}`}
            >
              {connected ? "已连接" : "未连接"}
            </div>
          </div>
        </div>

        {/* ===== 状态摘要 ===== */}
        <div className="glass rounded-xl p-6 shadow-md flex items-center gap-4">
          <span className="w-3 h-3 rounded-full bg-blue animate-pulse" />
          <div>
            <div className="text-sm text-subtext">设备状态</div>
            <div className="text-lg font-semibold text-blue">
              {connected ? "就绪" : "等待连接"}
            </div>
          </div>
        </div>
      </div>

      <div className="grid grid-cols-2 gap-4">
        {/* ===== 控制面板 ===== */}
        <div className="glass rounded-xl p-6 shadow-md">
          <h3 className="text-lg font-semibold text-text mb-5">运动控制</h3>
          <div className="grid grid-cols-3 gap-3 max-w-md mx-auto">
            <ControlButton onClick={reconnect}>设备连接</ControlButton>
            <ControlButton
              variant="accent"
              onClick={() => sendCommand("forward")}
              disabled={!connected}
            >
              前进
            </ControlButton>
            <ControlButton variant="danger" onClick={clearLogs}>
              清除日志
            </ControlButton>

            <ControlButton
              variant="accent"
              onClick={() => sendCommand("left")}
              disabled={!connected}
            >
              左转
            </ControlButton>
            <ControlButton
              onClick={() => sendCommand("pause")}
              disabled={!connected}
            >
              暂停
            </ControlButton>
            <ControlButton
              variant="accent"
              onClick={() => sendCommand("right")}
              disabled={!connected}
            >
              右转
            </ControlButton>

            <div />
            <ControlButton
              variant="accent"
              onClick={() => sendCommand("backward")}
              disabled={!connected}
            >
              后退
            </ControlButton>
            <div />
          </div>
        </div>

        {/* ===== 故障报警日志 ===== */}
        <div className="glass rounded-xl p-6 shadow-md">
          <div className="flex items-center justify-between mb-4">
            <h3 className="text-lg font-semibold text-text">故障报警日志</h3>
            <span className="text-sm text-subtext">{logs.length} 条记录</span>
          </div>
          {logs.length > 0 ? (
            <table className="w-full text-sm">
              <thead>
                <tr className="text-xs text-subtext uppercase tracking-wide">
                  <th className="text-left pb-2 pr-2 font-medium">事件</th>
                  <th className="text-left pb-2 font-medium">时间</th>
                </tr>
              </thead>
              <tbody>
                {logs.map((log, idx) => (
                  <tr
                    key={idx}
                    className="border-b border-border/30 hover:bg-surface1/30"
                  >
                    <td className="py-2.5 pr-2 text-red">{log.message}</td>
                    <td className="py-2.5 text-xs text-subtext font-mono">
                      {log.time}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          ) : (
            <div className="flex items-center justify-center h-32 text-subtext text-sm">
              暂无报警记录
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
