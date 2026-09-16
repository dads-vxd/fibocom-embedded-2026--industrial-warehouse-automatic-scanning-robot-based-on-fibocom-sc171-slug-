import { Outlet } from 'react-router-dom';
import { Sidebar } from './Sidebar';
import { LogPanel } from './LogPanel';

export function Layout() {
  return (
    <div className="flex h-screen overflow-hidden">
      <Sidebar />
      <div className="flex-1 ml-60 flex flex-col min-h-0">
        <main className="flex-1 overflow-auto p-6">
          <Outlet />
        </main>
        <LogPanel />
      </div>
    </div>
  );
}
