import type React from 'react';
import { useEffect, useState } from 'react';
import { Circle, CircleDot, Languages, MessageSquarePlus, Moon, Settings, Sun } from 'lucide-react';
import { useAppStore } from '../hooks/useAppStore';
import { useTranslation } from 'react-i18next';
import { Badge } from './ui/badge';
import { Button } from './ui/button';
import { Select } from './ui/select';

const languageOptions = [
  { value: 'zh', label: '简体中文' },
  { value: 'zh-TW', label: '繁體中文' },
  { value: 'en', label: 'English' },
] as const;

export default function TopBar({
  onSettingsClick,
  onNewChat,
}: {
  onSettingsClick: () => void;
  onNewChat: () => void;
}) {
  const mcpConnected = useAppStore((s) => s.mcpConnected);
  const settings = useAppStore((s) => s.settings);
  const updateSettings = useAppStore((s) => s.updateSettings);
  const { t } = useTranslation();
  const [theme, setTheme] = useState<'light' | 'dark'>('light');

  useEffect(() => {
    const saved = localStorage.getItem('pxtool-theme');
    const initial = saved === 'dark' || saved === 'light' ? saved : 'light';
    setTheme(initial);
    document.documentElement.classList.toggle('dark', initial === 'dark');
  }, []);

  const handleLanguageChange = (event: React.ChangeEvent<HTMLSelectElement>) => {
    updateSettings({ language: event.target.value as 'en' | 'zh' | 'zh-TW' });
  };

  const toggleTheme = () => {
    const next = theme === 'dark' ? 'light' : 'dark';
    setTheme(next);
    localStorage.setItem('pxtool-theme', next);
    document.documentElement.classList.toggle('dark', next === 'dark');
  };

  const StatusIcon = mcpConnected ? CircleDot : Circle;

  return (
    <header className="flex h-16 shrink-0 items-center justify-between gap-4 border-b border-border bg-card px-3 sm:px-4">
      <div className="flex min-w-0 items-center gap-3">
        <img
          src="/app-logo.png"
          alt=""
          aria-hidden="true"
          className="size-10 shrink-0 rounded-xl object-contain"
        />
        <div className="flex min-w-0 flex-col leading-tight">
          <div className="truncate text-base font-semibold tracking-tight text-foreground">
            {t('APP_TITLE')}
          </div>
          <div className="hidden truncate text-xs text-muted-foreground sm:block">
            {t('APP_SUBTITLE')}
          </div>
        </div>
        <Badge
          variant="outline"
          className={`ml-1 gap-1.5 rounded-full max-md:hidden md:inline-flex ${
            mcpConnected
              ? 'border-emerald-500/30 bg-emerald-500/10 text-emerald-600 dark:text-emerald-400'
              : 'text-muted-foreground'
          }`}
          title={mcpConnected ? t('ONLINE') : t('OFFLINE')}
        >
          <StatusIcon className="h-3 w-3" />
          <span>{mcpConnected ? t('ONLINE') : t('OFFLINE')}</span>
        </Badge>
      </div>

      <div className="flex shrink-0 items-center gap-2">
        <label className="relative shrink-0">
          <span className="sr-only">{t('LANG_TOGGLE')}</span>
          <Languages className="pointer-events-none absolute left-3 top-1/2 h-4 w-4 -translate-y-1/2 text-muted-foreground" />
          <Select
            value={settings.language}
            onChange={handleLanguageChange}
            aria-label={t('LANG_TOGGLE')}
            title={t('LANG_TOGGLE')}
            className="!w-11 shrink-0 pl-9 pr-3 text-transparent sm:!w-40 sm:text-foreground"
          >
            {languageOptions.map((option) => (
              <option key={option.value} value={option.value}>
                {option.label}
              </option>
            ))}
          </Select>
        </label>
        <Button
          variant="outline"
          size="icon"
          onClick={onSettingsClick}
          className="shrink-0"
          aria-label={t('MAINTENANCE_PNL')}
          title={t('MAINTENANCE_PNL')}
        >
          <Settings className="h-4 w-4" />
        </Button>
        <Button
          variant="default"
          size="icon"
          onClick={toggleTheme}
          className="shrink-0"
          aria-label={t('TOGGLE_THEME')}
          title={t('TOGGLE_THEME')}
        >
          {theme === 'dark' ? <Sun className="h-4 w-4" /> : <Moon className="h-4 w-4" />}
        </Button>
        <Button
          variant="default"
          size="icon"
          onClick={onNewChat}
          aria-label={t('NEW_CHAT')}
          title={t('NEW_CHAT')}
          className="shrink-0 max-sm:hidden"
        >
          <MessageSquarePlus className="h-4 w-4" />
        </Button>
      </div>
    </header>
  );
}
