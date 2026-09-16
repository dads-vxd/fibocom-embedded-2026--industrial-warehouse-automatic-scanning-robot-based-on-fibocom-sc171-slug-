# 条码管理系统前端方案

## 1. 项目概述

基于 React 的条码信息管理系统，主要功能：
- 条码列表展示（分页）
- 统计仪表盘

## 2. API 对接

### 基础配置
- Base URL: `http://localhost:3000`
- 图片路径: `/images/`

### 条码 API

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/barcodes?page=1&page_size=10` | 获取条码列表（分页） |
| GET | `/barcodes/:id` | 获取单个条码 |
| POST | `/barcodes` | 创建条码（multipart） |
| PUT | `/barcodes/:id` | 更新条码 |
| DELETE | `/barcodes/:id` | 删除条码 |

### 分类 API

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/categories` | 获取分类列表 |

### 响应格式

```typescript
// PaginatedResponse<T>
interface PaginatedResponse<T> {
  data: T[];
  total: number;
  page: number;
  page_size: number;
  total_pages: number;
}

// BarcodeResponse
interface BarcodeResponse {
  id: number;
  data: string;
  image: string;
  category: {
    id: number;
    name: string;
  };
  created_at: string;
  updated_at: string;
}

// Category
interface Category {
  id: number;
  name: string;
  description: string;
}
```

## 3. 页面结构

```
src/
├── pages/
│   ├── Dashboard.tsx      # 统计仪表盘
│   └── BarcodeList.tsx  # 条码列表
├── components/
│   ├── BarcodeTable.tsx  # 条码表格
│   ├── Pagination.tsx    # 分页组件
│   ├── StatCard.tsx      # 统计卡片
│   └── Sidebar.tsx       # 侧边导航
├── api/
│   └── index.ts          # API 请求封装
├── types/
│   └── index.ts          # TypeScript 类型
└── App.tsx
```

## 4. 核心功能

### 4.1 统计仪表盘
- 总条码数量
- 分类统计（按分类名称统计数量）
- 最近7天新增趋势

### 4.2 条码列表
- 表格展示：ID、条码数据、图片、分类、创建时间
- 分页：支持 page/page_size 参数
- 搜索/筛选（可选）

## 5. 技术栈

- React 18 + TypeScript
- React Router（路由）
- Axios（HTTP 请求）
- CSS Modules 或 Styled Components
- 可选：Recharts（图表）
