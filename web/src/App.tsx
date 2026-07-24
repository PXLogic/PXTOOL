import { useState, useEffect } from 'react';
import { useAppStore } from './hooks/useAppStore';
import TopBar from './components/TopBar';
import ChatPanel from './components/ChatPanel';
import DevicePanel from './components/DevicePanel';
import SettingsDrawer from './components/SettingsDrawer';
import ErrorBoundary from './components/ErrorBoundary';
import HistoryPanel from './components/HistoryPanel';
import { useTranslation } from 'react-i18next';
import { Cpu, MessagesSquare } from 'lucide-react';
import { Button } from './components/ui/button';
import {
  Drawer,
  DrawerContent,
  DrawerDescription,
  DrawerHeader,
  DrawerTitle,
} from './components/ui/drawer';

export default function App() {
  const settings = useAppStore((s) => s.settings);
  const connectMcp = useAppStore((s) => s.connectMcp);
  const clearChat = useAppStore((s) => s.clearChat);
  const mcpConnected = useAppStore((s) => s.mcpConnected);
  const { t } = useTranslation();

  const [settingsOpen, setSettingsOpen] = useState(false);
  const [mobileDeviceOpen, setMobileDeviceOpen] = useState(false);
  const [mobileHistoryOpen, setMobileHistoryOpen] = useState(false);

  // Auto-connect on first load if settings exist (silently catch errors)
  useEffect(() => {
    if (settings?.mcpServerUrl && !mcpConnected) {
      connectMcp().catch(() => {});
    }
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  return (
    <ErrorBoundary fallback={
      <div className="flex h-screen items-center justify-center bg-background p-6 text-foreground">
        <div className="w-full max-w-sm rounded-lg border border-border bg-card p-6 text-center shadow-sm">
          <p className="text-lg font-semibold">{t('SYSTEM_ERROR')}</p>
          <Button onClick={() => window.location.reload()} className="mt-4">
            {t('REBOOT')}
          </Button>
        </div>
      </div>
    }>
    <div className="flex h-screen flex-col overflow-hidden bg-background text-foreground">
      <TopBar
        onSettingsClick={() => setSettingsOpen(true)}
        onNewChat={clearChat}
      />

      <main className="flex min-h-0 flex-1 flex-col lg:flex-row">
        <aside className="hidden min-h-0 w-72 shrink-0 overflow-hidden border-r border-border bg-sidebar lg:flex lg:flex-col">
          <HistoryPanel />
        </aside>

        <div className="flex shrink-0 gap-2 border-b border-border bg-background p-3 lg:hidden">
          <Button
            variant="outline"
            className="flex-1"
            onClick={() => setMobileHistoryOpen(true)}
            aria-label={t('TAPE_ARCHIVE')}
          >
            <MessagesSquare className="h-4 w-4" />
            {t('TAPE_ARCHIVE')}
          </Button>
          <Button
            variant="outline"
            className="flex-1"
            onClick={() => setMobileDeviceOpen(true)}
            aria-label={t('OPEN_DEVICE_PANEL')}
          >
            <Cpu className="h-4 w-4" />
            {t('HW_DIAG_MOD')}
          </Button>
        </div>

        <section className="flex min-h-0 min-w-0 flex-1 p-4">
          <ChatPanel />
        </section>

        <aside className="hidden min-h-0 w-80 shrink-0 overflow-hidden border-l border-border bg-sidebar lg:flex lg:flex-col">
          <DevicePanel />
        </aside>
      </main>

      <Drawer open={mobileHistoryOpen} onOpenChange={setMobileHistoryOpen} direction="bottom">
        <DrawerContent className="h-[85vh] lg:hidden">
          <DrawerHeader>
            <DrawerTitle>{t('TAPE_ARCHIVE')}</DrawerTitle>
            <DrawerDescription>{t('SESSION_COUNT')}</DrawerDescription>
          </DrawerHeader>
          <div className="min-h-0 flex-1 overflow-hidden">
            <HistoryPanel />
          </div>
        </DrawerContent>
      </Drawer>

      <Drawer open={mobileDeviceOpen} onOpenChange={setMobileDeviceOpen} direction="bottom">
        <DrawerContent className="h-[85vh] lg:hidden">
          <DrawerHeader>
            <DrawerTitle>{t('HW_CONTROL_DECK')}</DrawerTitle>
            <DrawerDescription>{t('DEVICE_PANEL_DESC')}</DrawerDescription>
          </DrawerHeader>
          <div className="min-h-0 flex-1 overflow-hidden">
            <DevicePanel />
          </div>
        </DrawerContent>
      </Drawer>

      <SettingsDrawer open={settingsOpen} onClose={() => setSettingsOpen(false)} />
    </div>
    </ErrorBoundary>
  );
}
