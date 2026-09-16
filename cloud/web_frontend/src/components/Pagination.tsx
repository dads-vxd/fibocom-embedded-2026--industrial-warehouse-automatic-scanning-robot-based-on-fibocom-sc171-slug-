interface PaginationProps {
  currentPage: number;
  totalPages: number;
  total: number;
  onPageChange: (page: number) => void;
}

export function Pagination({ currentPage, totalPages, total, onPageChange }: PaginationProps) {

  return (
    <div className="flex justify-between items-center mt-5 pt-5 border-t border-border">
      <div className="text-sm text-subtext">
        共 {total} 条记录，第 {currentPage}/{totalPages} 页
      </div>
      <div className="flex gap-2">
        <button
          className="px-4 py-2 border border-border rounded-lg glass-light text-sm text-text disabled:opacity-50 disabled:cursor-not-allowed hover:border-lavender hover:text-lavender transition-colors"
          disabled={currentPage === 1}
          onClick={() => onPageChange(currentPage - 1)}
        >
          上一页
        </button>
        {Array.from({ length: Math.min(5, totalPages) }, (_, i) => {
          let page: number;
          if (totalPages <= 5) {
            page = i + 1;
          } else if (currentPage <= 3) {
            page = i + 1;
          } else if (currentPage >= totalPages - 2) {
            page = totalPages - 4 + i;
          } else {
            page = currentPage - 2 + i;
          }
          return (
            <button
              key={page}
              className={`px-4 py-2 border rounded-lg text-sm transition-colors ${
                currentPage === page
                  ? 'bg-lavender border-lavender text-white'
                  : 'border-border glass-light text-text hover:border-lavender hover:text-lavender'
              }`}
              onClick={() => onPageChange(page)}
            >
              {page}
            </button>
          );
        })}
        <button
          className="px-4 py-2 border border-border rounded-lg glass-light text-sm text-text disabled:opacity-50 disabled:cursor-not-allowed hover:border-lavender hover:text-lavender transition-colors"
          disabled={currentPage === totalPages}
          onClick={() => onPageChange(currentPage + 1)}
        >
          下一页
        </button>
      </div>
    </div>
  );
}
