import { NavLink, useNavigate } from 'react-router-dom';
import { useAuth } from '../contexts/AuthContext';

const navItems = [
  { to: '/', icon: '📊', label: '仪表盘' },
  { to: '/barcodes', icon: '🏷️', label: '条码列表' },
  { to: '/categories', icon: '📁', label: '类别管理' },
  { to: '/users', icon: '👥', label: '用户管理' },
  { to: '/videos', icon: '🎬', label: '视频记录' },
  { to: '/manual-control', icon: '🎮', label: '手动控制' },
];

export function Sidebar() {
  const { user, logout } = useAuth();
  const navigate = useNavigate();

  const handleLogout = () => {
    logout();
    navigate('/login');
  };

  return (
    <aside className="w-60 glass-strong text-text fixed h-screen flex flex-col">
      <div className="py-5 px-5 border-b border-border mb-5">
        <h1 className="text-xl font-semibold">📦 条码管理</h1>
      </div>
      <nav className="flex-1">
        {navItems.map((item) => (
          <NavLink
            key={item.to}
            to={item.to}
            end={item.to === '/'}
            className={({ isActive }) =>
              `flex items-center gap-2.5 px-5 py-3 cursor-pointer transition-colors hover:bg-surface1/50 ${
                isActive ? 'bg-surface1/50 border-l-4 border-lavender' : ''
              }`
            }
          >
            <span>{item.icon}</span>
            <span>{item.label}</span>
          </NavLink>
        ))}
      </nav>
      <div className="p-4 border-t border-border">
        <div className="flex items-center gap-3 mb-3">
          <div className="w-8 h-8 rounded-full bg-lavender/30 flex items-center justify-center text-sm">
            {user?.username?.charAt(0)?.toUpperCase() || '?'}
          </div>
          <div className="flex-1 min-w-0">
            <div className="text-sm font-medium truncate">{user?.username || '用户'}</div>
            <div className="text-xs text-subtext truncate">{user?.email || ''}</div>
          </div>
        </div>
        <button
          onClick={handleLogout}
          className="w-full px-4 py-2 border border-border rounded-lg text-sm text-text hover:border-red hover:text-red transition-colors"
        >
          退出登录
        </button>
      </div>
    </aside>
  );
}
