import { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { categoryApi } from '../api';
import type { FieldDef } from '../types';

export function CategoryCreate() {
  const navigate = useNavigate();
  const [name, setName] = useState('');
  const [fields, setFields] = useState<FieldDef[]>([]);
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
      await categoryApi.create({ name, field_defs: fields });
      navigate('/categories');
    } catch (err) {
      setError('创建失败，请重试');
      console.error(err);
    } finally { setLoading(false); }
  };

  const inputClass = 'w-full px-4 py-2 border border-border rounded-lg text-sm glass-light text-text placeholder-subtext focus:outline-none focus:border-lavender';
  const labelClass = 'block text-sm font-medium text-text mb-2';

  return (
    <div className="min-h-screen p-6">
      <h1 className="text-2xl font-semibold mb-6 text-text">添加类别</h1>
      <div className="glass rounded-xl p-6 shadow-md max-w-2xl">
        <form onSubmit={handleSubmit} className="space-y-5">
          {error && <div className="p-3 bg-red/20 text-red rounded-lg text-sm">{error}</div>}
          <div>
            <label className={labelClass}>类别名称 <span className="text-red">*</span></label>
            <input type="text" value={name} onChange={(e) => setName(e.target.value)} className={inputClass} placeholder="请输入类别名称" required />
          </div>
          <div>
            <div className="flex justify-between items-center mb-3">
              <label className={`${labelClass} mb-0`}>字段定义</label>
              <button type="button" onClick={addField} className="px-3 py-1.5 border border-border rounded-lg text-xs text-text hover:border-lavender transition-colors">
                + 添加字段
              </button>
            </div>
            {fields.length === 0 && <p className="text-sm text-subtext">暂无字段，点击"添加字段"来定义该类别的数据字段</p>}
            <div className="space-y-3">
              {fields.map((fd, idx) => (
                <div key={idx} className="flex gap-3 items-center">
                  <input type="text" value={fd.key} onChange={(e) => updateField(idx, 'key', e.target.value)} className={`${inputClass} flex-1`} placeholder="字段 key（英文）" />
                  <input type="text" value={fd.label} onChange={(e) => updateField(idx, 'label', e.target.value)} className={`${inputClass} flex-1`} placeholder="字段名称（中文）" />
                  <button type="button" onClick={() => removeField(idx)} className="text-red hover:text-peach text-sm px-2">删除</button>
                </div>
              ))}
            </div>
          </div>
          <div className="flex gap-3 pt-4">
            <button type="submit" disabled={loading} className="px-6 py-2 bg-lavender text-white rounded-lg text-sm hover:opacity-90 transition-colors font-medium disabled:opacity-50">
              {loading ? '提交中...' : '提交'}
            </button>
            <button type="button" onClick={() => navigate('/categories')} className="px-6 py-2 border border-border rounded-lg text-sm text-text hover:border-lavender transition-colors">
              取消
            </button>
          </div>
        </form>
      </div>
    </div>
  );
}
