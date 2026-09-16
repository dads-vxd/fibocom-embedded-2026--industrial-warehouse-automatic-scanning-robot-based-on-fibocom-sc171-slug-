import { useState, useEffect } from "react";
import { useNavigate, useSearchParams } from "react-router-dom";
import { barcodeApi, categoryApi } from "../api";
import type { Category, FieldDef } from "../types";

export function BarcodeCreate() {
  const navigate = useNavigate();
  const [searchParams] = useSearchParams();
  const preselectedCatId = searchParams.get("category_id");

  const [categories, setCategories] = useState<Category[]>([]);
  const [selectedCatId, setSelectedCatId] = useState<number>(
    preselectedCatId ? Number(preselectedCatId) : 0,
  );
  const [data, setData] = useState("");
  const [fieldValues, setFieldValues] = useState<Record<string, string>>({});
  const [image, setImage] = useState<File | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState("");

  useEffect(() => {
    categoryApi
      .getAll()
      .then((cats) => setCategories(Array.isArray(cats) ? cats : []))
      .catch(console.error);
  }, []);

  const currentCategory = categories.find((c) => c.id === selectedCatId);

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!data || !selectedCatId) {
      setError("请填写必填项");
      return;
    }

    setLoading(true);
    setError("");
    try {
      const formData = new FormData();
      formData.append("data", data);
      formData.append("category_id", String(selectedCatId));
      if (currentCategory) {
        for (const fd of currentCategory.field_defs) {
          formData.append(fd.key, fieldValues[fd.key] || "");
        }
      }
      if (image) formData.append("image", image);
      await barcodeApi.create(formData);
      navigate("/barcodes");
    } catch (err) {
      setError("创建失败，请重试");
      console.error(err);
    } finally {
      setLoading(false);
    }
  };

  const inputClass =
    "w-full px-4 py-2 border border-border rounded-lg text-sm glass-light text-text placeholder-subtext focus:outline-none focus:border-lavender";
  const labelClass = "block text-sm font-medium text-text mb-2";

  return (
    <div className="min-h-screen p-6">
      <h1 className="text-2xl font-semibold mb-6 text-text">添加条码</h1>
      <div className="glass rounded-xl p-6 shadow-md max-w-3xl">
        <form onSubmit={handleSubmit} className="space-y-5">
          {error && (
            <div className="p-3 bg-red/20 text-red rounded-lg text-sm">
              {error}
            </div>
          )}

          <div>
            <label className={labelClass}>
              类别 <span className="text-red">*</span>
            </label>
            <select
              value={selectedCatId}
              onChange={(e) => {
                setSelectedCatId(Number(e.target.value));
                setFieldValues({});
              }}
              className={inputClass}
              required
            >
              <option value={0}>请选择类别</option>
              {categories.map((c) => (
                <option key={c.id} value={c.id}>
                  {c.name}
                </option>
              ))}
            </select>
          </div>

          <div>
            <label className={labelClass}>
              条码数据 <span className="text-red">*</span>
            </label>
            <input
              type="text"
              value={data}
              onChange={(e) => setData(e.target.value)}
              className={inputClass}
              placeholder="请输入条码数据"
              required
            />
          </div>

          {currentCategory && currentCategory.field_defs.length > 0 && (
            <div className="grid grid-cols-2 gap-4">
              {currentCategory.field_defs.map((fd: FieldDef) => (
                <div key={fd.key}>
                  <label className={labelClass}>{fd.label}</label>
                  <input
                    type="text"
                    value={fieldValues[fd.key] || ""}
                    onChange={(e) =>
                      setFieldValues((prev) => ({
                        ...prev,
                        [fd.key]: e.target.value,
                      }))
                    }
                    className={inputClass}
                    placeholder={`请输入${fd.label}`}
                  />
                </div>
              ))}
            </div>
          )}

          <div>
            <label className={labelClass}>图片</label>
            <input
              type="file"
              accept="image/*"
              onChange={(e) => setImage(e.target.files?.[0] || null)}
              className={inputClass}
            />
          </div>

          <div className="flex gap-3 pt-4">
            <button
              type="submit"
              disabled={loading}
              className="px-6 py-2 bg-lavender text-white rounded-lg text-sm hover:opacity-90 transition-colors font-medium disabled:opacity-50"
            >
              {loading ? "提交中..." : "提交"}
            </button>
            <button
              type="button"
              onClick={() => navigate("/barcodes")}
              className="px-6 py-2 border border-border rounded-lg text-sm text-text hover:border-lavender transition-colors"
            >
              取消
            </button>
          </div>
        </form>
      </div>
    </div>
  );
}
