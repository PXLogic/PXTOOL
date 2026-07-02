import type React from 'react';
import { CircleDot, Languages, MessageSquarePlus, Settings } from 'lucide-react';
import { useAppStore } from '../hooks/useAppStore';
import { useTranslation } from 'react-i18next';
import { Button } from './ui/button';
import { Select } from './ui/select';

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

  const handleLanguageChange = (event: React.ChangeEvent<HTMLSelectElement>) => {
    updateSettings({ language: event.target.value as 'en' | 'zh' | 'zh-TW' });
  };

  return (
    <header className="flex h-16 shrink-0 items-center justify-between border-b border-slate-200 bg-white px-4 md:px-6">
      <div className="flex min-w-0 items-center gap-3">
        <div className="flex h-9 w-9 shrink-0 items-center justify-center rounded-md bg-slate-950 text-sm font-semibold text-white">
          PX
        </div>
        <div className="min-w-0">
          <div className="truncate text-sm font-semibold text-slate-950 sm:text-base">
            {t('APP_TITLE')}
          </div>
          <div className="hidden truncate text-xs text-slate-500 sm:block">
            {t('APP_SUBTITLE')}
          </div>
        </div>
        <div
          className="hidden items-center gap-1.5 rounded-full border border-slate-200 px-2.5 py-1 text-xs font-medium text-slate-600 md:flex"
          title={mcpConnected ? t('ONLINE') : t('OFFLINE')}
        >
          <CircleDot className={mcpConnected ? 'h-3.5 w-3.5 text-emerald-500' : 'h-3.5 w-3.5 text-slate-400'} />
          <span>{mcpConnected ? t('ONLINE') : t('OFFLINE')}</span>
        </div>
      </div>

      <div className="flex items-center gap-2">
        <label className="relative">
          <span className="sr-only">{t('LANG_TOGGLE')}</span>
          <Languages className="pointer-events-none absolute left-3 top-1/2 h-4 w-4 -translate-y-1/2 text-slate-500" />
          <Select
            value={settings.language}
            onChange={handleLanguageChange}
            aria-label={t('LANG_TOGGLE')}
            title={t('LANG_TOGGLE')}
            className="w-32 pl-9 sm:w-40"
          >
            <option value="zh">{t('LANG_ZH')}</option>
            <option value="zh-TW">{t('LANG_ZH_TW')}</option>
            <option value="en">{t('LANG_EN')}</option>
          </Select>
        </label>
        <Button
          variant="outline"
          size="icon"
          onClick={onSettingsClick}
          aria-label={t('MAINTENANCE_PNL')}
          title={t('MAINTENANCE_PNL')}
        >
          <Settings className="h-4 w-4" />
        </Button>
        <Button
          variant="default"
          size="icon"
          onClick={onNewChat}
          aria-label={t('NEW_CHAT')}
          title={t('NEW_CHAT')}
        >
          <MessageSquarePlus className="h-4 w-4" />
        </Button>
      </div>
    </header>
  );
}
