import { useState, useEffect } from 'react';
import { useAppStore } from './hooks/useAppStore';
import TopBar from './components/TopBar';
import ChatPanel from './components/ChatPanel';
import DevicePanel from './components/DevicePanel';
import SettingsDrawer from './components/SettingsDrawer';
import ErrorBoundary from './components/ErrorBoundary';
import HistoryPanel from './components/HistoryPanel';
import { useTranslation } from 'react-i18next';
import { PanelRightOpen, X } from 'lucide-react';
import { Button } from './components/ui/button';

export default function App() {
  const settings = useAppStore((s) => s.settings);
  const connectMcp = useAppStore((s) => s.connectMcp);
  const clearChat = useAppStore((s) => s.clearChat);
  const mcpConnected = useAppStore((s) => s.mcpConnected);
  const { t } = useTranslation();

  const [settingsOpen, setSettingsOpen] = useState(false);
  const [mobileDeviceOpen, setMobileDeviceOpen] = useState(false);

  // Auto-connect on first load if settings exist (silently catch errors)
  useEffect(() => {
    if (settings?.mcpServerUrl && !mcpConnected) {
      connectMcp().catch(() => {});
    }
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  return (
    <ErrorBoundary fallback={
      <div className="flex h-screen items-center justify-center bg-slate-50 p-6 text-slate-950">
        <div className="w-full max-w-sm rounded-lg border border-slate-200 bg-white p-6 text-center shadow-sm">
          <p className="text-lg font-semibold">{t('SYSTEM_ERROR')}</p>
          <Button onClick={() => window.location.reload()} className="mt-4">
            {t('REBOOT')}
          </Button>
        </div>
      </div>
    }>
    <div className="flex h-screen flex-col overflow-hidden bg-slate-50 text-slate-950">
      <TopBar
        onSettingsClick={() => setSettingsOpen(true)}
        onNewChat={clearChat}
      />

      <main className="grid min-h-0 flex-1 grid-cols-1 gap-4 p-4 lg:grid-cols-[280px_minmax(0,1fr)_320px]">
        <aside className="hidden min-h-0 overflow-hidden rounded-lg border border-slate-200 bg-white shadow-sm lg:flex lg:flex-col">
          <HistoryPanel />
        </aside>

        <section className="min-h-0 overflow-hidden rounded-lg border border-slate-200 bg-white shadow-sm">
          <ChatPanel />
        </section>

        <aside className="hidden min-h-0 overflow-hidden rounded-lg border border-slate-200 bg-white shadow-sm lg:flex lg:flex-col">
          <DevicePanel />
        </aside>
      </main>

      {mobileDeviceOpen && (
        <>
          <div
            className="fixed inset-0 z-30 bg-slate-950/40 backdrop-blur-sm lg:hidden"
            onClick={() => setMobileDeviceOpen(false)}
          />
          <div className="fixed inset-x-0 bottom-0 z-40 max-h-[78vh] overflow-hidden rounded-t-2xl border border-slate-200 bg-white shadow-2xl lg:hidden">
            <div className="flex items-center justify-between border-b border-slate-200 px-4 py-3">
              <div>
                <h2 className="text-sm font-semibold text-slate-950">{t('HW_CONTROL_DECK')}</h2>
                <p className="text-xs text-slate-500">{t('DEVICE_PANEL_DESC')}</p>
              </div>
              <Button
                variant="ghost"
                size="icon"
                onClick={() => setMobileDeviceOpen(false)}
                aria-label={t('CLOSE_PANEL')}
                title={t('CLOSE_PANEL')}
              >
                <X className="h-4 w-4" />
              </Button>
            </div>
            <div className="max-h-[calc(78vh-65px)] overflow-y-auto">
              <DevicePanel />
            </div>
          </div>
        </>
      )}

      {!mobileDeviceOpen && (
        <Button
          onClick={() => setMobileDeviceOpen(true)}
          className="fixed bottom-5 right-5 z-20 h-12 w-12 rounded-full shadow-lg lg:hidden"
          size="icon"
          aria-label={t('OPEN_DEVICE_PANEL')}
          title={t('OPEN_DEVICE_PANEL')}
        >
          <PanelRightOpen className="h-5 w-5" />
        </Button>
      )}

      <SettingsDrawer open={settingsOpen} onClose={() => setSettingsOpen(false)} />
    </div>
    </ErrorBoundary>
  );
}
