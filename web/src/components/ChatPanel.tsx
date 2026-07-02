import { useEffect, useRef } from 'react';
import { useAppStore } from '../hooks/useAppStore';
import ChatMessage from './ChatMessage';
import ChatInput from './ChatInput';
import ErrorBoundary from './ErrorBoundary';
import { useTranslation } from 'react-i18next';
import { Download, Eraser, MessageSquare } from 'lucide-react';
import { Badge } from './ui/badge';
import { Button } from './ui/button';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from './ui/card';

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
    <Card className="flex h-full w-full flex-col overflow-hidden border-0 shadow-none">
      {/* Messages area */}
      <CardHeader className="border-b border-border p-4">
        <div className="flex flex-col gap-3 sm:flex-row sm:items-center sm:justify-between">
          <div className="flex min-w-0 items-center gap-3">
            <div className="flex h-10 w-10 shrink-0 items-center justify-center rounded-md bg-primary text-primary-foreground">
              <MessageSquare className="h-5 w-5" />
            </div>
            <div className="min-w-0">
              <CardTitle className="truncate text-base">{t('CHAT_HEADER')}</CardTitle>
              <CardDescription className="mt-1 flex items-center gap-2">
                <span>{t('APP_TITLE')} v1.0</span>
                <Badge variant="secondary">{t('TERMINAL_MODE')}</Badge>
              </CardDescription>
            </div>
          </div>
          <div className="flex shrink-0 items-center gap-2">
            <Button variant="outline" size="sm" onClick={handleExport} disabled={messages.length === 0}>
              <Download className="h-4 w-4" />
              {t('EXPORT_LOG')}
            </Button>
            <Button variant="ghost" size="sm" onClick={clearChat} disabled={isProcessing}>
              <Eraser className="h-4 w-4" />
              {t('CLEAR_LOG')}
            </Button>
          </div>
        </div>
      </CardHeader>

      <CardContent className="min-h-0 flex-1 overflow-y-auto p-4">
        <div className="flex min-h-full flex-col gap-4">
          {messages.length === 0 ? (
            <div className="flex flex-1 items-center justify-center py-16 text-center">
              <div>
                <div className="mx-auto mb-3 flex h-12 w-12 items-center justify-center rounded-full bg-muted text-muted-foreground">
                  <MessageSquare className="h-5 w-5" />
                </div>
                <p className="text-sm font-medium text-muted-foreground">{t('AWAITING_INPUT')}</p>
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
      </CardContent>

      {/* Input area */}
      <ChatInput />
    </Card>
  );
}
