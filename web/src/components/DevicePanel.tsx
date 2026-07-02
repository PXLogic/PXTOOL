import { Activity, Cpu, Plug, RefreshCcw, Usb, Wifi, WifiOff } from 'lucide-react';
import { useAppStore } from '../hooks/useAppStore';
import { useTranslation } from 'react-i18next';
import { Badge } from './ui/badge';
import { Button } from './ui/button';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from './ui/card';
import { Separator } from './ui/separator';

export default function DevicePanel() {
  const deviceInfo = useAppStore((s) => s.deviceInfo);
  const captureStatus = useAppStore((s) => s.captureStatus);
  const mcpConnected = useAppStore((s) => s.mcpConnected);
  const reconnectStatus = useAppStore((s) => s.reconnectStatus);
  const connectMcp = useAppStore((s) => s.connectMcp);
  const disconnectMcp = useAppStore((s) => s.disconnectMcp);
  const attemptReconnect = useAppStore((s) => s.attemptReconnect);
  const { t } = useTranslation();

  const isCapturing = captureStatus === 'capturing';
  const isReconnecting = reconnectStatus === 'reconnecting';
  const connectionVariant = reconnectStatus === 'failed' ? 'destructive' : mcpConnected ? 'success' : 'secondary';
  const connectionLabel = isReconnecting
    ? t('RECONNECTING')
    : reconnectStatus === 'failed'
      ? t('LINK_FAILED')
      : mcpConnected
        ? t('ONLINE')
        : t('OFFLINE');
  const ConnectionIcon = mcpConnected ? Wifi : WifiOff;
  const primaryAction = reconnectStatus === 'failed'
    ? { label: t('RESET_LINK'), onClick: attemptReconnect, icon: RefreshCcw, variant: 'default' as const }
    : mcpConnected
      ? { label: t('CUT_POWER'), onClick: disconnectMcp, icon: Plug, variant: 'destructive' as const }
      : { label: t('ENGAGE'), onClick: connectMcp, icon: Plug, variant: 'default' as const };
  const PrimaryActionIcon = primaryAction.icon;

  return (
    <div className="w-full h-full flex flex-col gap-4 p-4 overflow-y-auto bg-slate-50/60">
      <div className="flex items-center gap-2 text-slate-900">
        <Cpu className="h-5 w-5 text-blue-600" aria-hidden="true" />
        <h2 className="truncate text-sm font-semibold uppercase tracking-wide">{t('HW_DIAG_MOD')}</h2>
      </div>

      <Card className="shadow-none">
        <CardHeader className="p-4 pb-3">
          <div className="flex items-start justify-between gap-3">
            <div>
              <CardTitle className="text-sm">{t('MASTER_LINK')}</CardTitle>
              <CardDescription>{connectionLabel}</CardDescription>
            </div>
            <Badge variant={connectionVariant} className="gap-1">
              <ConnectionIcon className="h-3 w-3" aria-hidden="true" />
              {connectionLabel}
            </Badge>
          </div>
        </CardHeader>
        <CardContent className="p-4 pt-0">
          <Button
            onClick={primaryAction.onClick}
            disabled={isReconnecting}
            variant={primaryAction.variant}
            className="w-full justify-start"
          >
            <PrimaryActionIcon className="h-4 w-4" aria-hidden="true" />
            {primaryAction.label}
          </Button>
        </CardContent>
      </Card>

      <Card className="shadow-none">
        <CardHeader className="p-4 pb-3">
          <div className="flex items-center gap-2">
            <Usb className="h-4 w-4 text-slate-500" aria-hidden="true" />
            <CardTitle className="text-sm">{t('HARDWARE_ID')}</CardTitle>
          </div>
        </CardHeader>
        <CardContent className="p-4 pt-0">
          {deviceInfo ? (
            <div className="flex flex-col">
              <div className="flex items-start justify-between gap-4 py-2">
                <span className="text-xs font-medium uppercase tracking-wide text-slate-500">{t('UNIT')}</span>
                <span className="min-w-0 text-right text-sm font-medium text-slate-900 break-words">{deviceInfo.name}</span>
              </div>
              <Separator />
              <div className="flex items-start justify-between gap-4 py-2">
                <span className="text-xs font-medium uppercase tracking-wide text-slate-500">{t('BUS')}</span>
                <span className="text-right text-sm font-medium text-slate-900">{deviceInfo.usbType}</span>
              </div>
              <Separator />
              <div className="flex items-start justify-between gap-4 py-2">
                <span className="text-xs font-medium uppercase tracking-wide text-slate-500">{t('MODE')}</span>
                <span className="text-right text-sm font-medium uppercase text-slate-900">{deviceInfo.mode}</span>
              </div>
            </div>
          ) : (
            <div className="rounded-md border border-dashed border-slate-200 py-6 text-center text-sm font-medium uppercase tracking-wide text-slate-400">
              {t('AWAITING_SIGNAL')}
            </div>
          )}
        </CardContent>
      </Card>

      <Card className="shadow-none">
        <CardHeader className="p-4 pb-3">
          <div className="flex items-start justify-between gap-3">
            <div>
              <CardTitle className="text-sm">{t('CAPTURE_STATE')}</CardTitle>
              <CardDescription>{captureStatus.toUpperCase()}</CardDescription>
            </div>
            <Badge variant={isCapturing ? 'warning' : 'secondary'} className="gap-1">
              <Activity className={`h-3 w-3 ${isCapturing ? 'animate-pulse' : ''}`} aria-hidden="true" />
              {captureStatus.toUpperCase()}
            </Badge>
          </div>
        </CardHeader>
      </Card>
    </div>
  );
}
