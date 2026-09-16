import { useState, useEffect } from "react";
import { userApi } from "../api";
import type { User } from "../types";
import { UserEditModal } from "../components/UserEditModal";
import { UserCreateModal } from "../components/UserCreateModal";

const roleMap: Record<number, { label: string; cls: string }> = {
  1: { label: "管理员", cls: "bg-lavender/20 text-lavender" },
  0: { label: "普通用户", cls: "bg-blue/20 text-blue" },
};

export function UserList() {
  const [users, setUsers] = useState<User[]>([]);
  const [loading, setLoading] = useState(true);
  const [search, setSearch] = useState("");
  const [editingUser, setEditingUser] = useState<User | null>(null);
  const [showCreate, setShowCreate] = useState(false);

  const loadData = async () => {
    setLoading(true);
    try {
      const data = await userApi.getList();
      setUsers(Array.isArray(data) ? data : (data as any)?.list ?? []);
    } catch (error) {
      console.error("Failed to load users:", error);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    loadData();
  }, []);

  const handleDelete = async (id: string) => {
    if (!confirm("确定要删除该用户吗？")) return;
    try {
      await userApi.delete(id);
      loadData();
    } catch (error) {
      alert("删除失败");
      console.error(error);
    }
  };

  const handleEditSuccess = () => {
    setEditingUser(null);
    loadData();
  };

  const handleCreateSuccess = () => {
    setShowCreate(false);
    loadData();
  };

  const filteredUsers = search
    ? users.filter(
        (u) =>
          u.username.toLowerCase().includes(search.toLowerCase()) ||
          u.email.toLowerCase().includes(search.toLowerCase()),
      )
    : users;

  const formatDate = (dateStr: string) => {
    return new Date(dateStr).toLocaleString("zh-CN");
  };

  return (
    <div>
      <h1 className="text-2xl font-semibold mb-6 text-text">用户管理</h1>

      <div className="glass rounded-xl p-6 shadow-md">
        <div className="flex justify-between items-center mb-5">
          <h3 className="text-lg font-semibold text-text">用户列表</h3>
          <div className="flex gap-3">
            <input
              type="text"
              placeholder="搜索用户..."
              value={search}
              onChange={(e) => setSearch(e.target.value)}
              className="px-4 py-2 border border-border rounded-lg w-60 text-sm glass-light text-text placeholder-subtext focus:outline-none focus:border-lavender"
            />
            <button
              onClick={() => setShowCreate(true)}
              className="px-4 py-2 border border-border bg-lavender text-white rounded-lg text-sm hover:opacity-90 transition-colors font-medium"
            >
              新增用户
            </button>
          </div>
        </div>

        {loading ? (
          <div className="p-8 text-center text-subtext">加载中...</div>
        ) : (
          <table className="w-full">
            <thead>
              <tr>
                <th className="text-left p-4 glass-light font-semibold text-sm text-subtext border-b-2 border-border">
                  用户名
                </th>
                <th className="text-left p-4 glass-light font-semibold text-sm text-subtext border-b-2 border-border">
                  邮箱
                </th>
                <th className="text-left p-4 glass-light font-semibold text-sm text-subtext border-b-2 border-border">
                  角色
                </th>
                <th className="text-left p-4 glass-light font-semibold text-sm text-subtext border-b-2 border-border">
                  创建时间
                </th>
                <th className="text-left p-4 glass-light font-semibold text-sm text-subtext border-b-2 border-border">
                  操作
                </th>
              </tr>
            </thead>
            <tbody>
              {filteredUsers.map((user) => {
                const role = roleMap[user.role] || {
                  label: "未知",
                  cls: "glass-light text-text",
                };
                return (
                  <tr key={user.id} className="hover:bg-surface1/30">
                    <td className="p-4 border-b border-border text-sm text-text">
                      <div className="flex items-center gap-3">
                        <div className="w-8 h-8 rounded-full bg-lavender/30 flex items-center justify-center text-sm">
                          {user.username.charAt(0).toUpperCase()}
                        </div>
                        {user.username}
                      </div>
                    </td>
                    <td className="p-4 border-b border-border text-sm text-text">
                      {user.email}
                    </td>
                    <td className="p-4 border-b border-border">
                      <span
                        className={`inline-block px-3 py-1 rounded-full text-xs font-medium ${role.cls}`}
                      >
                        {role.label}
                      </span>
                    </td>
                    <td className="p-4 border-b border-border text-sm text-subtext">
                      {formatDate(user.created_at)}
                    </td>
                    <td className="p-4 border-b border-border text-sm">
                      <button
                        onClick={() => setEditingUser(user)}
                        className="text-blue hover:text-lavender mr-3"
                      >
                        编辑
                      </button>
                      <button
                        onClick={() => handleDelete(user.id)}
                        className="text-red hover:text-peach"
                      >
                        删除
                      </button>
                    </td>
                  </tr>
                );
              })}
              {filteredUsers.length === 0 && (
                <tr>
                  <td colSpan={5} className="p-8 text-center text-subtext">
                    暂无数据
                  </td>
                </tr>
              )}
            </tbody>
          </table>
        )}
      </div>

      {editingUser && (
        <UserEditModal
          user={editingUser}
          onClose={() => setEditingUser(null)}
          onSuccess={handleEditSuccess}
        />
      )}

      {showCreate && (
        <UserCreateModal
          onClose={() => setShowCreate(false)}
          onSuccess={handleCreateSuccess}
        />
      )}
    </div>
  );
}
