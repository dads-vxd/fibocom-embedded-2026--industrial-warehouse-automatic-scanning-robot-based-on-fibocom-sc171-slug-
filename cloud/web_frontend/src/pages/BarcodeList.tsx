import { useState, useEffect, useCallback } from "react";
import { Link } from "react-router-dom";
import { BarcodeTable } from "../components/BarcodeTable";
import { Pagination } from "../components/Pagination";
import { barcodeApi, categoryApi } from "../api";
import type { Barcode, Category, FieldDef } from "../types";
import { BarcodeEditModal } from "../components/BarcodeEditModal";

export function BarcodeList() {
  const [categories, setCategories] = useState<Category[]>([]);
  const [selectedCatId, setSelectedCatId] = useState<number | null>(null);
  const [barcodes, setBarcodes] = useState<Barcode[]>([]);
  const [page, setPage] = useState(1);
  const pageSize = 10;
  const [total, setTotal] = useState(0);
  const [loading, setLoading] = useState(true);
  const [editingBarcode, setEditingBarcode] = useState<Barcode | null>(null);
  const [showFilter, setShowFilter] = useState(false);
  const [filter, setFilter] = useState<Record<string, string>>({});
  const [appliedFilter, setAppliedFilter] = useState<Record<string, string>>(
    {},
  );

  useEffect(() => {
    categoryApi
      .getAll()
      .then((cats) => {
        const list = Array.isArray(cats) ? cats : [];
        setCategories(list);
        if (list.length > 0) {
          setSelectedCatId(list[0].id);
        }
      })
      .catch(console.error);
  }, []);

  const currentCategory =
    categories.find((c) => c.id === selectedCatId) || null;

  const loadData = useCallback(async () => {
    if (!selectedCatId) return;
    setLoading(true);
    try {
      const res = await barcodeApi.getList({
        category_id: selectedCatId,
        page,
        page_size: pageSize,
        ...appliedFilter,
      });
      setBarcodes(res.data);
      setTotal(res.total);
    } catch (error) {
      console.error("Failed to load barcodes:", error);
    } finally {
      setLoading(false);
    }
  }, [selectedCatId, page, appliedFilter]);

  useEffect(() => {
    loadData();
  }, [loadData]);

  useEffect(() => {
    setPage(1);
    setFilter({});
    setAppliedFilter({});
  }, [selectedCatId]);

  const handleDelete = async (id: number) => {
    if (!confirm("确定要删除这条条码吗？")) return;
    try {
      await barcodeApi.delete(id);
      loadData();
    } catch (error) {
      alert("删除失败");
      console.error(error);
    }
  };

  const handleEditSuccess = () => {
    setEditingBarcode(null);
    loadData();
  };

  const applyFilter = () => {
    setAppliedFilter(filter);
    setPage(1);
  };
  const clearFilter = () => {
    setFilter({});
    setAppliedFilter({});
    setPage(1);
  };

  const totalPages = Math.ceil(total / pageSize);
  const inputClass =
    "w-full px-3 py-2 border border-border rounded-lg text-sm glass-light text-text placeholder-subtext focus:outline-none focus:border-lavender";

  return (
    <div>
      <h1 className="text-2xl font-semibold mb-6 text-text">条码列表</h1>
      <div className="glass rounded-xl p-6 shadow-md">
        <div className="flex justify-between items-center mb-5">
          <div className="flex items-center gap-3">
            <h3 className="text-lg font-semibold text-text">条码数据</h3>
            <select
              value={selectedCatId || ""}
              onChange={(e) => setSelectedCatId(Number(e.target.value))}
              className="px-4 py-2 border border-border rounded-lg text-sm glass-light text-text focus:outline-none focus:border-lavender"
            >
              {categories.map((c) => (
                <option key={c.id} value={c.id}>
                  {c.name}
                </option>
              ))}
            </select>
          </div>
          <div className="flex gap-3">
            {currentCategory && (
              <button
                onClick={() => setShowFilter(!showFilter)}
                className={`px-4 py-2 border border-border rounded-lg text-sm transition-colors ${showFilter ? "border-lavender text-lavender" : "text-text hover:border-lavender"}`}
              >
                {showFilter ? "收起筛选" : "筛选"}
              </button>
            )}
            <Link
              to={`/barcodes/new${selectedCatId ? `?category_id=${selectedCatId}` : ""}`}
              className="px-4 py-2 border border-border bg-lavender text-white rounded-lg text-sm hover:opacity-90 transition-colors font-medium"
            >
              新增条码
            </Link>
          </div>
        </div>

        {showFilter && currentCategory && (
          <div className="mb-5 glass-light rounded-lg p-4">
            <div
              className={`grid gap-3 ${currentCategory.field_defs.length <= 3 ? "grid-cols-3" : "grid-cols-4"}`}
            >
              {currentCategory.field_defs.map((fd: FieldDef) => (
                <div key={fd.key}>
                  <label className="block text-xs text-subtext mb-1">
                    {fd.label}
                  </label>
                  <input
                    type="text"
                    value={filter[fd.key] || ""}
                    onChange={(e) =>
                      setFilter((prev) => ({
                        ...prev,
                        [fd.key]: e.target.value,
                      }))
                    }
                    className={inputClass}
                    placeholder={fd.label}
                  />
                </div>
              ))}
              <div className="flex items-end gap-2">
                <button
                  onClick={applyFilter}
                  className="px-4 py-2 bg-lavender text-white rounded-lg text-sm hover:opacity-90 transition-colors font-medium"
                >
                  查询
                </button>
                <button
                  onClick={clearFilter}
                  className="px-4 py-2 border border-border rounded-lg text-sm text-text hover:border-lavender transition-colors"
                >
                  重置
                </button>
              </div>
            </div>
          </div>
        )}

        {loading ? (
          <div className="p-8 text-center text-subtext">加载中...</div>
        ) : !currentCategory ? (
          <div className="p-8 text-center text-subtext">请先创建类别</div>
        ) : (
          <>
            <BarcodeTable
              barcodes={barcodes}
              fieldDefs={currentCategory.field_defs}
              onEdit={setEditingBarcode}
              onDelete={handleDelete}
            />
            <Pagination
              currentPage={page}
              totalPages={totalPages}
              total={total}
              onPageChange={setPage}
            />
          </>
        )}
      </div>

      {editingBarcode && currentCategory && (
        <BarcodeEditModal
          barcode={editingBarcode}
          category={currentCategory}
          onClose={() => setEditingBarcode(null)}
          onSuccess={handleEditSuccess}
        />
      )}
    </div>
  );
}
