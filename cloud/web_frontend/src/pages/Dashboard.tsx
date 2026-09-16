import { useState, useEffect } from "react";
import { StatCard } from "../components/StatCard";
import { barcodeApi, categoryApi, userApi, videoApi } from "../api";
import type { GroupCountRow, User, Video, Barcode, Category } from "../types";
import { useTheme } from "../components/ThemeProvider";
import {
  BarChart,
  Bar,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  ResponsiveContainer,
  Cell,
  PieChart,
  Pie,
  Legend,
} from "recharts";

const darkChartColors = [
  "#cba6f7",
  "#89b4fa",
  "#a6e3a1",
  "#f9e2af",
  "#fab387",
  "#f38ba8",
];
const lightChartColors = [
  "#8839ef",
  "#1e66f5",
  "#40a02b",
  "#df8e1d",
  "#fe640b",
  "#d20f39",
];

function formatBytes(bytes: number) {
  if (!bytes) return "0 B";
  const units = ["B", "KB", "MB", "GB"];
  const i = Math.floor(Math.log(bytes) / Math.log(1024));
  return (bytes / Math.pow(1024, i)).toFixed(1) + " " + units[i];
}

function formatTimeAgo(dateStr: string) {
  if (!dateStr) return "--";
  const d = new Date(dateStr);
  const now = new Date();
  const diff = now.getTime() - d.getTime();
  if (diff < 60000) return "刚刚";
  if (diff < 3600000) return Math.floor(diff / 60000) + " 分钟前";
  if (diff < 86400000) return Math.floor(diff / 3600000) + " 小时前";
  return d.toLocaleDateString("zh-CN");
}

export function Dashboard() {
  const [total, setTotal] = useState(0);
  const [categoryData, setCategoryData] = useState<GroupCountRow[]>([]);
  const [categories, setCategories] = useState<Category[]>([]);
  const [users, setUsers] = useState<User[]>([]);
  const [videos, setVideos] = useState<Video[]>([]);
  const [recentBarcodes, setRecentBarcodes] = useState<Barcode[]>([]);
  const [loading, setLoading] = useState(true);
  const { theme } = useTheme();

  const chartColors = theme === "dark" ? darkChartColors : lightChartColors;
  const gridStroke = theme === "dark" ? "#45475a" : "#ccd0da";
  const tickFill = theme === "dark" ? "#a6adc8" : "#6c6f85";
  const tooltipBg = theme === "dark" ? "#313244" : "#ffffff";
  const tooltipText = theme === "dark" ? "#cdd6f4" : "#4c4f69";
  const legendColor = theme === "dark" ? "#a6adc8" : "#6c6f85";
  const chartBorderColor = theme === "dark" ? "#1e1e2e" : "#ffffff";

  useEffect(() => {
    loadData();
  }, []);

  const loadData = async () => {
    setLoading(true);
    try {
      const [catRes, cats, usersRes, videosRes] = await Promise.all([
        barcodeApi.statsByCategory(),
        categoryApi.getAll(),
        userApi.getList(),
        videoApi.getList({ page: 1, page_size: 100 }),
      ]);

      const t = catRes.reduce((sum, r) => sum + r.count, 0);
      setTotal(t);
      setCategoryData(catRes);
      setCategories(cats);
      setUsers(usersRes);
      setVideos(videosRes.data || []);

      const allBarcodes: Barcode[] = [];
      for (const cat of cats) {
        const res = await barcodeApi.getList({
          category_id: cat.id,
          page: 1,
          page_size: 5,
        });
        allBarcodes.push(...res.data);
      }
      allBarcodes.sort(
        (a, b) =>
          new Date(b.created_at).getTime() -
          new Date(a.created_at).getTime(),
      );
      setRecentBarcodes(allBarcodes.slice(0, 5));
    } catch (error) {
      console.error("Failed to load data:", error);
    } finally {
      setLoading(false);
    }
  };

  const adminCount = users.filter((u) => u.role === 1).length;
  const normalCount = users.filter((u) => u.role === 0).length;
  const roleData = [
    { name: "管理员", value: adminCount },
    { name: "普通用户", value: normalCount },
  ];

  const totalVideoSize = videos.reduce((sum, v) => sum + (v.size || 0), 0);
  const avgVideoSize =
    videos.length > 0 ? Math.round(totalVideoSize / videos.length) : 0;
  const maxStorage = 100 * 1024 * 1024;
  const storagePercent = Math.min((totalVideoSize / maxStorage) * 100, 100);

  const categoryMap = new Map(categories.map((c) => [c.id, c.name]));

  if (loading) {
    return (
      <div className="flex items-center justify-center h-64">
        <div className="text-subtext">加载中...</div>
      </div>
    );
  }

  const tooltipStyle = {
    backgroundColor: tooltipBg,
    border: `1px solid ${gridStroke}`,
    borderRadius: 8,
    color: tooltipText,
    padding: 10,
  };

  return (
    <div>
      <h1 className="text-2xl font-semibold mb-6 text-text">仪表盘</h1>

      {/* ===== 统计卡片 ===== */}
      <div className="grid grid-cols-4 gap-4 mb-6">
        <StatCard label="总条码数" value={total} />
        <StatCard label="产品分类" value={categories.length} />
        <StatCard label="系统用户" value={users.length} />
        <StatCard label="视频记录" value={videos.length} />
      </div>

      {/* ===== 第一行：按类别统计 + 用户角色 ===== */}
      <div className="grid grid-cols-2 gap-4 mb-6">
        {/* 按类别统计 */}
        <div className="glass rounded-xl p-6 shadow-md">
          <h3 className="text-lg font-semibold text-text mb-4">按类别统计</h3>
          {categoryData.length > 0 ? (
            <ResponsiveContainer width="100%" height={300}>
              <BarChart
                data={categoryData}
                margin={{ top: 5, right: 20, left: 0, bottom: 5 }}
              >
                <CartesianGrid strokeDasharray="3 3" stroke={gridStroke} />
                <XAxis
                  dataKey="name"
                  tick={{ fill: tickFill, fontSize: 12 }}
                  axisLine={{ stroke: gridStroke }}
                />
                <YAxis
                  tick={{ fill: tickFill, fontSize: 12 }}
                  axisLine={{ stroke: gridStroke }}
                  allowDecimals={false}
                />
                <Tooltip
                  contentStyle={tooltipStyle}
                  formatter={(value) => [`${value} 条`, "数量"]}
                />
                <Bar dataKey="count" radius={[6, 6, 0, 0]}>
                  {categoryData.map((_, index) => (
                    <Cell
                      key={index}
                      fill={chartColors[index % chartColors.length]}
                    />
                  ))}
                </Bar>
              </BarChart>
            </ResponsiveContainer>
          ) : (
            <div className="h-[300px] flex items-center justify-center text-subtext">
              暂无数据
            </div>
          )}
        </div>

        {/* 用户角色分布 */}
        <div className="glass rounded-xl p-6 shadow-md">
          <h3 className="text-lg font-semibold text-text mb-4">
            用户角色分布
          </h3>
          {users.length > 0 ? (
            <>
              <ResponsiveContainer width="100%" height={220}>
                <PieChart>
                  <Pie
                    data={roleData}
                    dataKey="value"
                    nameKey="name"
                    cx="50%"
                    cy="50%"
                    outerRadius={80}
                  >
                    <Cell
                      fill={
                        theme === "dark" ? "#cba6f7" : "#8839ef"
                      }
                    />
                    <Cell
                      fill={
                        theme === "dark" ? "#a6e3a1" : "#40a02b"
                      }
                    />
                  </Pie>
                  <Tooltip
                    contentStyle={tooltipStyle}
                    formatter={(value) => [`${value} 人`, ""]}
                  />
                </PieChart>
              </ResponsiveContainer>
              <div className="flex gap-5 mt-3">
                <div className="flex-1 text-center">
                  <div
                    className="text-xl font-bold"
                    style={{
                      color:
                        theme === "dark" ? "#cba6f7" : "#8839ef",
                    }}
                  >
                    {adminCount}
                  </div>
                  <div className="text-xs text-subtext mt-0.5">
                    管理员
                  </div>
                </div>
                <div className="flex-1 text-center">
                  <div
                    className="text-xl font-bold"
                    style={{
                      color:
                        theme === "dark" ? "#a6e3a1" : "#40a02b",
                    }}
                  >
                    {normalCount}
                  </div>
                  <div className="text-xs text-subtext mt-0.5">
                    普通用户
                  </div>
                </div>
              </div>
            </>
          ) : (
            <div className="h-[220px] flex items-center justify-center text-subtext">
              暂无数据
            </div>
          )}
        </div>
      </div>

      {/* ===== 第二行：视频存储 + 类别分布 ===== */}
      <div className="grid grid-cols-2 gap-4 mb-6">
        {/* 视频存储统计 */}
        <div className="glass rounded-xl p-6 shadow-md">
          <h3 className="text-lg font-semibold text-text mb-4">
            视频存储统计
          </h3>
          <div className="flex flex-col gap-3 mt-4">
            <div className="flex justify-between text-sm text-subtext">
              <span>已用存储</span>
              <span className="text-text font-semibold">
                {formatBytes(totalVideoSize)}
              </span>
            </div>
            <div className="w-full h-2 bg-surface1 rounded-full overflow-hidden">
              <div
                className="h-full rounded-full transition-all duration-400"
                style={{
                  width: `${storagePercent}%`,
                  background: `linear-gradient(90deg, ${chartColors[0]}, ${chartColors[1]})`,
                }}
              />
            </div>
            <div className="flex justify-between text-sm text-subtext mt-1">
              <span>
                文件数:{" "}
                <span className="text-text font-semibold">
                  {videos.length}
                </span>{" "}
                个
              </span>
              <span>
                平均大小:{" "}
                <span className="text-text font-semibold">
                  {formatBytes(avgVideoSize)}
                </span>
              </span>
            </div>
          </div>
        </div>

        {/* 类别分布占比 */}
        <div className="glass rounded-xl p-6 shadow-md">
          <h3 className="text-lg font-semibold text-text mb-4">
            类别分布占比
          </h3>
          {categoryData.length > 0 ? (
            <ResponsiveContainer width="100%" height={260}>
              <PieChart>
                <Pie
                  data={categoryData}
                  dataKey="count"
                  nameKey="name"
                  cx="50%"
                  cy="45%"
                  outerRadius={90}
                  innerRadius={50}
                >
                  {categoryData.map((_, index) => (
                    <Cell
                      key={index}
                      fill={chartColors[index % chartColors.length]}
                      stroke={chartBorderColor}
                      strokeWidth={2}
                    />
                  ))}
                </Pie>
                <Tooltip
                  contentStyle={tooltipStyle}
                  formatter={(_value: any, _name: any, props: any) => {
                    const val = props.payload.count;
                    const pct = ((val / total) * 100).toFixed(1);
                    return [`${val} 条 (${pct}%)`, props.payload.name];
                  }}
                />
                <Legend
                  wrapperStyle={{
                    fontSize: 12,
                    paddingTop: 12,
                    color: legendColor,
                  }}
                  iconType="circle"
                  iconSize={8}
                />
              </PieChart>
            </ResponsiveContainer>
          ) : (
            <div className="h-[260px] flex items-center justify-center text-subtext">
              暂无数据
            </div>
          )}
        </div>
      </div>

      {/* ===== 最近记录 ===== */}
      <div className="grid grid-cols-2 gap-4 mb-6">
        {/* 最近添加的条形码 */}
        <div className="glass rounded-xl p-6 shadow-md">
          <h3 className="text-lg font-semibold text-text mb-4">
            最近添加的条形码
          </h3>
          {recentBarcodes.length > 0 ? (
            <table className="w-full text-sm">
              <thead>
                <tr className="text-xs text-subtext uppercase tracking-wide">
                  <th className="text-left pb-2 pr-2 font-medium">
                    数据内容
                  </th>
                  <th className="text-left pb-2 pr-2 font-medium">
                    所属分类
                  </th>
                  <th className="text-left pb-2 font-medium">时间</th>
                </tr>
              </thead>
              <tbody>
                {recentBarcodes.map((b) => (
                  <tr
                    key={b.id}
                    className="border-b border-border/30 hover:bg-surface1/30"
                  >
                    <td className="py-2.5 pr-2 text-xs font-mono">
                      {b.data.substring(0, 20)}...
                    </td>
                    <td className="py-2.5 pr-2">
                      {categoryMap.get(b.category_id) || "-"}
                    </td>
                    <td className="py-2.5 text-xs text-subtext">
                      {formatTimeAgo(b.created_at)}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          ) : (
            <div className="flex items-center justify-center h-32 text-subtext text-sm">
              暂无数据
            </div>
          )}
        </div>

        {/* 最近上传的视频 */}
        <div className="glass rounded-xl p-6 shadow-md">
          <h3 className="text-lg font-semibold text-text mb-4">
            最近上传的视频
          </h3>
          {videos.length > 0 ? (
            <table className="w-full text-sm">
              <thead>
                <tr className="text-xs text-subtext uppercase tracking-wide">
                  <th className="text-left pb-2 pr-2 font-medium">
                    文件名
                  </th>
                  <th className="text-left pb-2 pr-2 font-medium">大小</th>
                  <th className="text-left pb-2 font-medium">时间</th>
                </tr>
              </thead>
              <tbody>
                {videos.slice(0, 5).map((v) => (
                  <tr
                    key={v.id}
                    className="border-b border-border/30 hover:bg-surface1/30"
                  >
                    <td className="py-2.5 pr-2 text-xs font-mono truncate max-w-[160px]">
                      {v.filename}
                    </td>
                    <td className="py-2.5 pr-2">
                      {formatBytes(v.size)}
                    </td>
                    <td className="py-2.5 text-xs text-subtext">
                      {formatTimeAgo(v.created_at)}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          ) : (
            <div className="flex items-center justify-center h-32 text-subtext text-sm">
              暂无数据
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
