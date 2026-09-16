import { useState } from 'react';
import type { Category, FieldDef } from '../types';
import { categoryApi } from '../api';

interface CategoryEditModalProps {
  category: Category;
  onClose: () => void;
  onSuccess: () => void;
}

export function CategoryEditModal({ category, onClose, onSuccess }: CategoryEditModalProps) {
  const [name, setName] = useState(category.name);
  const [fields, setFields] = useState<FieldDef[]>(() => [...(category.field_defs || [])]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');

  const addField = () => setFields(prev => [...prev, { key: '', label: '' }]);
  const removeField = (idx: number) => setFields(prev => prev.filter((_, i) => i !== idx));
  const updateField = (idx: number, f: 'key' | 'label', val: string) => {
    setFields(prev => prev.map((fd, i) => i === idx ? { ...fd, [f]: val } : fd));
  };

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!name) { setError('请填写类别名称'); return; }
    const valid = fields.every(f => f.key && f.label);
    if (!valid) { setError('所有字段必须填写 key 和 label'); return; }

    setLoading(true);
    setError('');
    try {
      await categoryApi.update(category.id, { name, field_defs: fields });
      onSuccess();
    } catch (err) {
      setError('更新失败，请重试');
      console.error(err);
    } finally { setLoading(false); }
  };

  const inputClass = 'w-full px-4 py-2 border border-border rounded-lg text-sm glass text-text focus:outline-none focus:border-lavender';

  return (
    <div className="fixed inset-0 flex items-center justify-center z-50 backdrop-blur-sm" style={{ backgroundColor: 'rgba(0,0,0,0.4)' }}>
      <div className="glass-strong rounded-xl p-6 w-full max-w-lg shadow-xl max-h-[90vh] overflow-y-auto">
        <h2 className="text-xl font-semibold mb-4 text-text">编辑类别</h2>
        <form onSubmit={handleSubmit} className="space-y-4">
          {error && <div className="p-3 bg-red/20 text-red rounded-lg text-sm">{error}</div>}
          <div>
            <label className="block text-sm font-medium text-text mb-2">类别名称 <span className="text-red">*</span></label>
            <input type="text" value={name} onChange={(e) => setName(e.target.value)} className={inputClass} required />
          </div>
          <div>
            <div className="flex justify-between items-center mb-3">
              <label className="block text-sm font-medium text-text">字段定义</label>
              <button type="button" onClick={addField} className="px-3 py-1.5 border border-border rounded-lg text-xs text-text hover:border-lavender transition-colors">+ 添加字段</button>
            </div>
            <div className="space-y-3">
              {fields.map((fd, idx) => (
                <div key={idx} className="flex gap-3 items-center">
                  <input type="text" value={fd.key} onChange={(e) => updateField(idx, 'key', e.target.value)} className={`${inputClass} flex-1`} placeholder="字段 key" />
                  <input type="text" value={fd.label} onChange={(e) => updateField(idx, 'label', e.target.value)} className={`${inputClass} flex-1`} placeholder="字段名称" />
                  <button type="button" onClick={() => removeField(idx)} className="text-red hover:text-peach text-sm px-2">删除</button>
                </div>
              ))}
              {fields.length === 0 && <p className="text-sm text-subtext">暂无字段</p>}
            </div>
          </div>
          <div className="flex gap-3 pt-4">
            <button type="submit" disabled={loading} className="flex-1 px-6 py-2 bg-lavender text-white rounded-lg text-sm hover:opacity-90 transition-colors font-medium disabled:opacity-50">
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
