import { useState } from 'react';
import type { User } from '../types';
import { userApi } from '../api';

interface UserEditModalProps {
  user: User;
  onClose: () => void;
  onSuccess: () => void;
}

export function UserEditModal({ user, onClose, onSuccess }: UserEditModalProps) {
  const [username, setUsername] = useState(user.username);
  const [email, setEmail] = useState(user.email);
  const [password, setPassword] = useState('');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!username || !email) {
      setError('请填写必填项');
      return;
    }

    setLoading(true);
    setError('');

    try {
      const payload: Record<string, string> = { username, email };
      if (password) payload.password = password;
      await userApi.update(user.id, payload);
      onSuccess();
    } catch (err) {
      setError('更新失败，请重试');
      console.error(err);
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="fixed inset-0 flex items-center justify-center z-50 backdrop-blur-sm" style={{ backgroundColor: 'rgba(0,0,0,0.4)' }}>
      <div className="glass-strong rounded-xl p-6 w-full max-w-md shadow-xl">
        <h2 className="text-xl font-semibold mb-4 text-text">编辑用户</h2>

        <form onSubmit={handleSubmit} className="space-y-4">
          {error && (
            <div className="p-3 bg-red/20 text-red rounded-lg text-sm">{error}</div>
          )}

          <div>
            <label className="block text-sm font-medium text-text mb-2">
              用户名 <span className="text-red">*</span>
            </label>
            <input
              type="text"
              value={username}
              onChange={(e) => setUsername(e.target.value)}
              className="w-full px-4 py-2 border border-border rounded-lg text-sm glass text-text focus:outline-none focus:border-lavender"
              required
            />
          </div>

          <div>
            <label className="block text-sm font-medium text-text mb-2">
              邮箱 <span className="text-red">*</span>
            </label>
            <input
              type="email"
              value={email}
              onChange={(e) => setEmail(e.target.value)}
              className="w-full px-4 py-2 border border-border rounded-lg text-sm glass text-text focus:outline-none focus:border-lavender"
              required
            />
          </div>

          <div>
            <label className="block text-sm font-medium text-text mb-2">
              新密码 <span className="text-subtext text-xs">（留空则不修改）</span>
            </label>
            <input
              type="password"
              value={password}
              onChange={(e) => setPassword(e.target.value)}
              className="w-full px-4 py-2 border border-border rounded-lg text-sm glass text-text focus:outline-none focus:border-lavender"
              placeholder="输入新密码"
            />
          </div>

          <div className="flex gap-3 pt-4">
            <button
              type="submit"
              disabled={loading}
              className="flex-1 px-6 py-2 bg-lavender text-white rounded-lg text-sm hover:opacity-90 transition-colors font-medium disabled:opacity-50"
            >
              {loading ? '保存中...' : '保存'}
            </button>
            <button
              type="button"
              onClick={onClose}
              className="flex-1 px-6 py-2 border border-border rounded-lg text-sm text-text hover:border-lavender transition-colors"
            >
              取消
            </button>
          </div>
        </form>
      </div>
    </div>
  );
}
