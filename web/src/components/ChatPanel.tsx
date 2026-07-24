import { useEffect, useRef } from 'react';
import { useAppStore } from '../hooks/useAppStore';
import ChatMessage from './ChatMessage';
import ChatInput from './ChatInput';
import ErrorBoundary from './ErrorBoundary';
import { useTranslation } from 'react-i18next';
import { Download, Eraser, MessageSquare } from 'lucide-react';
import { Badge } from './ui/badge';
import { Button } from './ui/button';

export default function ChatPanel() {
  const messages = useAppStore((s) => s.messages);
  const clearChat = useAppStore((s) => s.clearChat);
  const isProcessing = useAppStore((s) => s.isProcessing);
  const bottomRef = useRef<HTMLDivElement>(null);
  const { t } = useTranslation();

  useEffect(() => {
    bottomRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [messages]);

  const handleExport = () => {
    if (messages.length === 0) return;
    const content = messages.map(m => {
      const prefix = m.role === 'user' ? 'USER>' : m.role === 'tool' ? 'OUT >' : 'SYS >';
      return `${prefix}\n${m.content}\n`;
    }).join('\n');
    
    const blob = new Blob([content], { type: 'text/markdown' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `pxtool_diag_${new Date().getTime()}.md`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  };

  return (
    <section className="flex min-w-0 flex-1 flex-col overflow-hidden rounded-xl border border-border bg-card">
      {/* Messages area */}
      <div className="border-b border-border px-5 py-4">
        <div className="flex flex-col gap-3 sm:flex-row sm:items-center sm:justify-between">
          <div className="flex min-w-0 items-center gap-3">
            <div className="flex size-10 shrink-0 items-center justify-center rounded-lg bg-primary text-primary-foreground">
              <MessageSquare className="h-5 w-5" />
            </div>
            <div className="flex min-w-0 flex-col leading-tight">
              <h1 className="truncate text-base font-semibold">{t('CHAT_HEADER')}</h1>
              <div className="mt-1 flex items-center gap-2 text-xs text-muted-foreground">
                <span>{t('APP_TITLE')} v1.0</span>
                <Badge variant="secondary" className="rounded-md">{t('TERMINAL_MODE')}</Badge>
              </div>
            </div>
          </div>
          <div className="flex shrink-0 items-center gap-2">
            <Button variant="ghost" size="sm" onClick={handleExport} disabled={messages.length === 0}>
              <Download className="h-4 w-4" />
              {t('EXPORT_LOG')}
            </Button>
            <Button variant="ghost" size="sm" onClick={clearChat} disabled={isProcessing}>
              <Eraser className="h-4 w-4" />
              {t('CLEAR_LOG')}
            </Button>
          </div>
        </div>
      </div>

      <div className="min-h-0 flex-1 overflow-y-auto">
        <div className="flex min-h-full flex-col gap-4 p-5">
          {messages.length === 0 ? (
            <div className="flex flex-1 items-center justify-center p-6 text-center">
              <div>
                <div className="mx-auto mb-4 flex size-8 items-center justify-center rounded-lg bg-muted text-muted-foreground">
                  <MessageSquare className="h-4 w-4" />
                </div>
                <p className="text-sm font-semibold text-foreground">{t('AWAITING_INPUT')}</p>
                <p className="mt-1 text-xs text-muted-foreground">{t('READY_MSG')}</p>
              </div>
            </div>
          ) : (
            messages.map((msg) => (
              <ErrorBoundary key={msg.id}>
                <ChatMessage message={msg} />
              </ErrorBoundary>
            ))
          )}
          <div ref={bottomRef} />
        </div>
      </div>

      {/* Input area */}
      <ChatInput />
    </section>
  );
}
