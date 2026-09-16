interface StatCardProps {
  label: string;
  value: string | number;
  accent?: boolean;
}

export function StatCard({ label, value, accent }: StatCardProps) {
  return (
    <div
      className={`rounded-xl p-6 shadow-md ${
        accent ? "glass bg-lavender/30" : "glass"
      }`}
    >
      <div
        className={`text-sm ${accent ? "text-text/80" : "text-subtext"} mb-2`}
      >
        {label}
      </div>
      <div className="text-3xl font-bold text-text">
        {value}
      </div>
    </div>
  );
}
