import { createContext, useContext, useState, useEffect, type ReactNode } from 'react';

type Theme = 'dark' | 'light';

interface ThemeContextType {
  theme: Theme;
  toggleTheme: () => void;
}

const ThemeContext = createContext<ThemeContextType | undefined>(undefined);

const darkVars: Record<string, string> = {
  '--color-bg': '#1e1e2e',
  '--color-surface': '#313244',
  '--color-surface1': '#45475a',
  '--color-text': '#cdd6f4',
  '--color-subtext': '#a6adc8',
  '--color-lavender': '#cba6f7',
  '--color-blue': '#89b4fa',
  '--color-green': '#a6e3a1',
  '--color-red': '#f38ba8',
  '--color-yellow': '#f9e2af',
  '--color-peach': '#fab387',
  '--color-border': '#45475a',
  '--color-overlay': 'rgba(0,0,0,0.7)',
  '--color-glass-border': 'rgba(255,255,255,0.1)',
};

const lightVars: Record<string, string> = {
  '--color-bg': '#eff1f5',
  '--color-surface': '#ffffff',
  '--color-surface1': '#e6e9ef',
  '--color-text': '#4c4f69',
  '--color-subtext': '#6c6f85',
  '--color-lavender': '#8839ef',
  '--color-blue': '#1e66f5',
  '--color-green': '#40a02b',
  '--color-red': '#d20f39',
  '--color-yellow': '#df8e1d',
  '--color-peach': '#fe640b',
  '--color-border': '#ccd0da',
  '--color-overlay': 'rgba(0,0,0,0.3)',
  '--color-glass-border': 'rgba(0,0,0,0.08)',
};

export function ThemeProvider({ children }: { children: ReactNode }) {
  const [theme, setTheme] = useState<Theme>(() => {
    const saved = localStorage.getItem('theme');
    return (saved as Theme) || 'dark';
  });

  useEffect(() => {
    localStorage.setItem('theme', theme);
    const vars = theme === 'dark' ? darkVars : lightVars;
    const root = document.documentElement;
    Object.entries(vars).forEach(([key, value]) => {
      root.style.setProperty(key, value);
    });
    document.body.setAttribute('data-theme', theme);
  }, [theme]);

  const toggleTheme = () => {
    setTheme((prev) => (prev === 'dark' ? 'light' : 'dark'));
  };

  return (
    <ThemeContext.Provider value={{ theme, toggleTheme }}>
      {children}
      <ThemeToggle />
    </ThemeContext.Provider>
  );
}

export function useTheme() {
  const context = useContext(ThemeContext);
  if (!context) {
    throw new Error('useTheme must be used within ThemeProvider');
  }
  return context;
}

function ThemeToggle() {
  const { theme, toggleTheme } = useTheme();

  return (
    <button
      onClick={toggleTheme}
      className="fixed bottom-6 right-6 w-12 h-12 rounded-full flex items-center justify-center text-xl shadow-lg hover:scale-110 transition-transform z-50 glass bg-lavender/60 text-white"
      title={theme === 'dark' ? '切换亮色模式' : '切换暗色模式'}
    >
      {theme === 'dark' ? '☀️' : '🌙'}
    </button>
  );
}
