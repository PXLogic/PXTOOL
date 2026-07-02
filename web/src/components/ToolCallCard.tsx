import { useState, useEffect } from 'react';
import type { ToolCallStatus } from '../hooks/useAppStore';
import DecoderResultTable from './DecoderResultTable';
import { useTranslation } from 'react-i18next';
import { ChevronDown, ChevronRight, Clock } from 'lucide-react';
import { Badge } from './ui/badge';
import { Button } from './ui/button';
import { Card } from './ui/card';

function getFriendlyLabel(name: string, args: Record<string, unknown>): string | null {
  if (name === 'get_devices') return 'FETCHING_DEVICES';
  if (name === 'start_capture') return 'INITIATING_CAPTURE';
  if (name === 'add_analyzer') {
    const decoder = args.decoder ?? args.protocol ?? '';
    const ch = args.channel ?? args.ch ?? '';
    return `MOUNTING_${decoder}_CH${ch}`;
  }
  return null;
}

function statusVariant(status: ToolCallStatus['status']) {
  if (status === 'success') return 'success';
  if (status === 'running') return 'warning';
  if (status === 'error') return 'destructive';
  return 'secondary';
}

const StatusLabel = ({ status, t }: { status: ToolCallStatus['status'], t: any }) => {
  const label =
    status === 'pending' ? t('TOOL_PENDING') :
    status === 'running' ? t('TOOL_RUNNING') :
    status === 'success' ? t('TOOL_SUCCESS') :
    status === 'cancelled' ? t('TOOL_CANCELLED') :
    t('TOOL_ERROR');

  return (
    <Badge variant={statusVariant(status)} className={status === 'running' || status === 'pending' ? 'animate-pulse' : ''}>
      {label}
    </Badge>
  );
};

function DeviceCards({ data, t }: { data: string, t: any }) {
  try {
    const devices = JSON.parse(data);
    if (!Array.isArray(devices)) return <pre className="text-sm p-2 overflow-x-auto">{data}</pre>;
    return (
      <div className="space-y-2">
        {devices.map((d: any, i: number) => (
          <div key={i} className="flex flex-wrap items-center gap-x-4 gap-y-1 rounded-md bg-muted/60 px-3 py-2 text-sm">
            <span className="w-4 text-muted-foreground">{i}</span>
            <span className="font-medium">
              {d.display_name || d.name || d.modelName || t('UNKNOWN_DEVICE')}
              {d.is_active && <Badge variant="warning" className="ml-2">{t('ACTIVE')}</Badge>}
            </span>
            <span className="text-muted-foreground">
              {d.usb_speed === 4 ? 'USB 3.0' : d.usb_speed === 3 ? 'USB 2.0' : d.usb_speed === 2 ? 'USB 1.1' : d.is_virtual ? 'Virtual' : '---'}
            </span>
            <span className="text-muted-foreground">
              {d.is_hardware_dso ? 'DSO' : d.is_hardware_logic ? 'Logic' : d.is_file ? 'File' : '---'}
            </span>
          </div>
        ))}
      </div>
    );
  } catch {
    return <pre className="text-sm p-2 overflow-x-auto">{data}</pre>;
  }
}

export default function ToolCallCard({ toolCall }: { toolCall: ToolCallStatus }) {
  const [expanded, setExpanded] = useState(false);
  const [showFull, setShowFull] = useState(false);
  const [liveElapsed, setLiveElapsed] = useState<number | null>(null);
  const { t } = useTranslation();

  useEffect(() => {
    if (toolCall.status !== 'running') return;

    const update = () => {
      setLiveElapsed(Math.round((Date.now() - toolCall.startTime) / 1000));
    };
    update(); // initial tick
    const timer = setInterval(update, 1000);

    return () => clearInterval(timer);
  }, [toolCall.status, toolCall.startTime]);

  const args = (() => {
    try {
      return typeof toolCall.args === 'string' ? JSON.parse(toolCall.args) : toolCall.args ?? {};
    } catch {
      return {};
    }
  })();

  const friendly = getFriendlyLabel(toolCall.name, args);
  const resultText = toolCall.result ?? '';
  const truncated = !showFull && resultText.length > 500;
  const displayResult = truncated ? resultText.slice(0, 500) : resultText;

  return (
    <Card className="my-2 overflow-hidden border-dashed bg-muted/30 shadow-none">
      <Button
        variant="ghost"
        onClick={() => setExpanded(!expanded)}
        className="h-auto w-full justify-start rounded-none px-3 py-2 text-left"
        aria-expanded={expanded}
      >
        {expanded ? <ChevronDown className="h-4 w-4 shrink-0" /> : <ChevronRight className="h-4 w-4 shrink-0" />}
        <StatusLabel status={toolCall.status} t={t} />
        <span className="min-w-0 truncate font-mono text-sm font-semibold">{toolCall.name}</span>
        {friendly && <span className="hidden truncate text-sm text-muted-foreground md:inline">{t(friendly)}</span>}

        {toolCall.status === 'running' && liveElapsed != null && (
          <span className="ml-auto inline-flex items-center gap-1 text-sm text-warning">
            <Clock className="h-3.5 w-3.5" />
            {liveElapsed}s
          </span>
        )}
        {toolCall.status !== 'running' && toolCall.status !== 'pending' && toolCall.elapsed != null && (
          <span className="ml-auto inline-flex items-center gap-1 text-sm text-muted-foreground">
            <Clock className="h-3.5 w-3.5" />
            {toolCall.elapsed}{t('ELAPSED')}
          </span>
        )}
      </Button>

      {expanded && (
        <div className="space-y-3 border-t border-border p-3">
          {Object.keys(args).length > 0 && (
            <pre className="overflow-x-auto rounded-md bg-background p-3 text-xs text-muted-foreground">
              {JSON.stringify(args, null, 2)}
            </pre>
          )}
          {resultText && (
            <div className="border-t border-border pt-3">
              {toolCall.name === 'get_analyzer_results' ? (
                <DecoderResultTable data={resultText} />
              ) : toolCall.name === 'get_devices' ? (
                <DeviceCards data={resultText} t={t} />
              ) : (
                <>
                  <pre className="overflow-x-auto whitespace-pre-wrap rounded-md bg-background p-3 text-xs">
                    {displayResult}
                  </pre>
                  {truncated && (
                    <Button onClick={() => setShowFull(true)} variant="ghost" size="sm" className="mt-2">
                      {t('SHOW_MORE')}
                    </Button>
                  )}
                </>
              )}
            </div>
          )}
        </div>
      )}
    </Card>
  );
}
