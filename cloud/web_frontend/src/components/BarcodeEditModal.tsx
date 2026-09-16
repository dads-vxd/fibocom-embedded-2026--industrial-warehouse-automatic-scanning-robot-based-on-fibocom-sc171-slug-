import { useState, useRef } from 'react';
import type { Barcode, Category, FieldDef } from '../types';
import { barcodeApi } from '../api';

interface BarcodeEditModalProps {
  barcode: Barcode;
  category: Category;
  onClose: () => void;
  onSuccess: () => void;
}

export function BarcodeEditModal({ barcode, category, onClose, onSuccess }: BarcodeEditModalProps) {
  const [data, setData] = useState(barcode.data);
  const [fieldValues, setFieldValues] = useState<Record<string, string>>(() => {
    const vals: Record<string, string> = {};
    for (const fd of category.field_defs) {
      vals[fd.key] = (barcode.field_values as Record<string, string>)?.[fd.key] || '';
    }
    return vals;
  });
  const [image, setImage] = useState<File | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');
  const fileInputRef = useRef<HTMLInputElement>(null);
  const baseUrl = import.meta.env.VITE_API_BASE_URL || '/api';
  const previewUrl = image ? URL.createObjectURL(image) : (barcode.image ? `${baseUrl}${barcode.image}` : '');

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!data) { setError('请填写条码数据'); return; }

    setLoading(true);
    setError('');
    try {
      const formData = new FormData();
      formData.append('data', data);
      for (const fd of category.field_defs) {
        formData.append(fd.key, fieldValues[fd.key] || '');
      }
      if (image) formData.append('image', image);
      await barcodeApi.update(barcode.id, formData);
      onSuccess();
    } catch (err) {
      setError('更新失败，请重试');
      console.error(err);
    } finally {
      setLoading(false);
    }
  };

  const inputClass = 'w-full px-4 py-2 border border-border rounded-lg text-sm glass text-text focus:outline-none focus:border-lavender';
  const labelClass = 'block text-sm font-medium text-text mb-2';

  return (
    <div className="fixed inset-0 flex items-center justify-center z-50 backdrop-blur-sm" style={{ backgroundColor: 'rgba(0,0,0,0.4)' }}>
      <div className="glass-strong rounded-xl p-6 w-full max-w-2xl shadow-xl max-h-[90vh] overflow-y-auto">
        <h2 className="text-xl font-semibold mb-4 text-text">编辑条码</h2>
        <p className="text-sm text-subtext mb-4">类别：{category.name}</p>
        <form onSubmit={handleSubmit} className="space-y-4">
          {error && <div className="p-3 bg-red/20 text-red rounded-lg text-sm">{error}</div>}
          <div>
            <label className={labelClass}>条码数据 <span className="text-red">*</span></label>
            <input type="text" value={data} onChange={(e) => setData(e.target.value)} className={inputClass} required />
          </div>
          <div className="grid grid-cols-2 gap-4">
            {category.field_defs.map((fd: FieldDef) => (
              <div key={fd.key}>
                <label className={labelClass}>{fd.label}</label>
                <input
                  type="text"
                  value={fieldValues[fd.key] || ''}
                  onChange={(e) => setFieldValues(prev => ({ ...prev, [fd.key]: e.target.value }))}
                  className={inputClass}
                  placeholder={`请输入${fd.label}`}
                />
              </div>
            ))}
          </div>
          <div>
            <label className={labelClass}>图片</label>
            {previewUrl && (
              <div className="mb-2"><img src={previewUrl} alt="preview" className="w-20 h-20 object-cover rounded-lg glass-light" /></div>
            )}
            <input ref={fileInputRef} type="file" accept="image/*" onChange={(e) => setImage(e.target.files?.[0] || null)} className="hidden" />
            <div className="flex gap-2">
              <button type="button" onClick={() => fileInputRef.current?.click()} className="px-4 py-2 border border-border rounded-lg text-sm text-text hover:border-lavender transition-colors">
                {barcode.image ? '更换图片' : '选择图片'}
              </button>
              {image && (
                <button type="button" onClick={() => { setImage(null); if (fileInputRef.current) fileInputRef.current.value = ''; }} className="px-4 py-2 border border-border rounded-lg text-sm text-red hover:border-red transition-colors">
                  取消更换
                </button>
              )}
            </div>
          </div>
          <div className="flex gap-3 pt-4">
            <button type="submit" disabled={loading} className="flex-1 px-6 py-2 border border-border bg-lavender text-white rounded-lg text-sm hover:opacity-90 transition-colors font-medium disabled:opacity-50">
              {loading ? '保存中...' : '保存'}
            </button>
            <button type="button" onClick={onClose} className="flex-1 px-6 py-2 border border-border rounded-lg text-sm text-text hover:border-lavender transition-colors">
              取消
            </button>
          </div>
        </form>
      </div>
    </div>
  );
}
