import { useState, useEffect } from 'react';

export function ForbiddenModal() {
  const [message, setMessage] = useState('');
  const [visible, setVisible] = useState(false);

  useEffect(() => {
    const handler = (e: Event) => {
      const detail = (e as CustomEvent).detail;
      setMessage(detail || '无权限执行此操作');
      setVisible(true);
    };
    window.addEventListener('forbidden', handler);
    return () => window.removeEventListener('forbidden', handler);
  }, []);

  if (!visible) return null;

  return (
    <div
      className="fixed inset-0 flex items-center justify-center z-[100] backdrop-blur-sm"
      style={{ backgroundColor: 'rgba(0,0,0,0.4)' }}
    >
      <div className="glass-strong rounded-xl p-6 w-full max-w-sm shadow-xl text-center">
        <div className="text-4xl mb-4">🚫</div>
        <h3 className="text-lg font-semibold text-text mb-2">权限不足</h3>
        <p className="text-subtext text-sm mb-6">{message}</p>
        <button
          onClick={() => setVisible(false)}
          className="px-6 py-2 bg-lavender text-white rounded-lg text-sm hover:opacity-90 transition-colors font-medium"
        >
          知道了
        </button>
      </div>
    </div>
  );
}
