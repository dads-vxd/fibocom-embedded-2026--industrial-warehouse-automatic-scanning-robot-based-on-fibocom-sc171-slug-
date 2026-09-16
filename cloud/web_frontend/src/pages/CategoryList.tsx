import { useState, useEffect } from "react";
import { Link } from "react-router-dom";
import { categoryApi } from "../api";
import type { Category } from "../types";
import { CategoryEditModal } from "../components/CategoryEditModal";

export function CategoryList() {
  const [categories, setCategories] = useState<Category[]>([]);
  const [loading, setLoading] = useState(true);
  const [editingCategory, setEditingCategory] = useState<Category | null>(null);

  useEffect(() => {
    loadData();
  }, []);

  const loadData = async () => {
    setLoading(true);
    try {
      const res = await categoryApi.getList();
      setCategories(res.data ?? []);
    } catch (error) {
      console.error("Failed to load categories:", error);
    } finally {
      setLoading(false);
    }
  };

  const handleDelete = async (id: number) => {
    if (!confirm("确定要删除这个类别吗？如果有条码属于此类别，将无法删除。"))
      return;
    try {
      await categoryApi.delete(id);
      loadData();
    } catch (error) {
      alert("删除失败，可能存在关联条码");
      console.error(error);
    }
  };

  const handleEditSuccess = () => {
    setEditingCategory(null);
    loadData();
  };

  return (
    <div>
      <h1 className="text-2xl font-semibold mb-6 text-text">类别管理</h1>
      <div className="glass rounded-xl p-6 shadow-md">
        <div className="flex justify-between items-center mb-5">
          <h3 className="text-lg font-semibold text-text">类别列表</h3>
          <Link
            to="/categories/new"
            className="px-4 py-2 bg-lavender text-white rounded-lg text-sm hover:opacity-90 transition-colors font-medium"
          >
            新增类别
          </Link>
        </div>
        {loading ? (
          <div className="p-8 text-center text-subtext">加载中...</div>
        ) : (
          <table className="w-full">
            <thead>
              <tr>
                <th className="text-left p-4 glass-light font-semibold text-sm text-subtext border-b-2 border-border">
                  ID
                </th>
                <th className="text-left p-4 glass-light font-semibold text-sm text-subtext border-b-2 border-border">
                  类别名称
                </th>
                <th className="text-left p-4 glass-light font-semibold text-sm text-subtext border-b-2 border-border">
                  字段定义
                </th>
                <th className="text-left p-4 glass-light font-semibold text-sm text-subtext border-b-2 border-border">
                  操作
                </th>
              </tr>
            </thead>
            <tbody>
              {categories.map((cat) => (
                <tr key={cat.id} className="hover:bg-surface1/30">
                  <td className="p-4 border-b border-border text-sm text-text">
                    {cat.id}
                  </td>
                  <td className="p-4 border-b border-border text-sm text-text">
                    {cat.name}
                  </td>
                  <td className="p-4 border-b border-border text-sm text-text">
                    <div className="flex flex-wrap gap-1">
                      {(cat.field_defs || []).map((fd) => (
                        <span
                          key={fd.key}
                          className="inline-block px-2 py-0.5 glass-light rounded text-xs"
                        >
                          {fd.label}({fd.key})
                        </span>
                      ))}
                      {(!cat.field_defs || cat.field_defs.length === 0) && (
                        <span className="text-subtext text-xs">无字段</span>
                      )}
                    </div>
                  </td>
                  <td className="p-4 border-b border-border text-sm">
                    <button
                      onClick={() => setEditingCategory(cat)}
                      className="text-blue hover:text-lavender mr-3"
                    >
                      编辑
                    </button>
                    <button
                      onClick={() => handleDelete(cat.id)}
                      className="text-red hover:text-peach"
                    >
                      删除
                    </button>
                  </td>
                </tr>
              ))}
              {categories.length === 0 && (
                <tr>
                  <td colSpan={4} className="p-8 text-center text-subtext">
                    暂无数据
                  </td>
                </tr>
              )}
            </tbody>
          </table>
        )}
      </div>
      {editingCategory && (
        <CategoryEditModal
          category={editingCategory}
          onClose={() => setEditingCategory(null)}
          onSuccess={handleEditSuccess}
        />
      )}
    </div>
  );
}
