import { useState } from 'react';
import type { ConversationMessage } from '../hooks/useAppStore';
import ToolCallCard from './ToolCallCard';
import ReactMarkdown from 'react-markdown';
import remarkGfm from 'remark-gfm';
import { useAppStore } from '../hooks/useAppStore';
import { useTranslation } from 'react-i18next';
import { Copy, RefreshCw } from 'lucide-react';
import { Badge } from './ui/badge';
import { Button } from './ui/button';
import { Card } from './ui/card';

export default function ChatMessage({ message }: { message: ConversationMessage }) {
  const isUser = message.role === 'user';
  const isAssistant = message.role === 'assistant';
  const isStreaming = message.isStreaming;
  const isToolRunning = message.isToolRunning;
  const isStopped = message.isStopped;
  const hasToolCalls = message.toolCallStatuses && message.toolCallStatuses.length > 0;
  const showThinking = isStreaming && !message.content && (!hasToolCalls || message.toolCallStatuses!.every(tc => tc.status === 'pending'));

  const regenerateMessage = useAppStore(s => s.regenerateMessage);
  const isProcessing = useAppStore(s => s.isProcessing);
  const { t } = useTranslation();

  const [copied, setCopied] = useState(false);

  const handleCopy = () => {
    navigator.clipboard.writeText(message.content);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  const handleRegenerate = () => {
    if (!isProcessing) {
      regenerateMessage(message.id);
    }
  };

  const alignClass = isUser ? 'items-end' : 'items-start';
  const cardColor = isUser ? 'border-blue-200 bg-blue-50 text-blue-950' : 'bg-card text-card-foreground';
  const headerText = isUser ? t('USER_INPUT') : message.role === 'tool' ? t('SYS_OUTPUT') : t('RESPONSE_DECK');

  return (
    <div className={`flex w-full flex-col ${alignClass} group`}>
      <Card className={`flex max-w-[min(85%,52rem)] flex-col gap-3 p-4 ${cardColor}`}>
        {/* Card Header */}
        <div className="flex items-center justify-between gap-3 border-b border-border/70 pb-2">
          <div className="flex items-center gap-2">
            <Badge variant={isUser ? 'outline' : message.role === 'tool' ? 'secondary' : 'default'}>{headerText}</Badge>
            {isStreaming && <Badge variant="warning">{t('PROCESSING')}</Badge>}
          </div>

          {/* Action buttons (Copy, Regenerate) shown on hover */}
          <div className="flex gap-1 opacity-100 transition-opacity sm:opacity-0 sm:group-hover:opacity-100 sm:focus-within:opacity-100">
            {message.content && (
              <Button
                onClick={handleCopy}
                variant="ghost"
                size="icon"
                aria-label={copied ? t('COPIED') : t('COPY')}
                title={copied ? t('COPIED') : t('COPY')}
              >
                <Copy className="h-4 w-4" />
              </Button>
            )}
            {isAssistant && !isProcessing && (
              <Button
                onClick={handleRegenerate}
                variant="ghost"
                size="icon"
                aria-label={t('REGENERATE')}
                title={`${t('REGENERATE')} - discard everything after this response`}
              >
                <RefreshCw className="h-4 w-4" />
              </Button>
            )}
          </div>
        </div>

        {/* Thinking indicator */}
        {showThinking && (
          <div className="animate-pulse text-sm font-medium text-muted-foreground">{t('PROCESSING')}</div>
        )}

        {/* Text content with Markdown support */}
        {message.content && (
          <div className="markdown-body max-w-full overflow-hidden text-sm leading-relaxed">
            <ReactMarkdown remarkPlugins={[remarkGfm]}>
              {message.content}
            </ReactMarkdown>
            {isStreaming && (
              <span className="ml-1 inline-block h-4 w-2 animate-pulse bg-foreground align-text-bottom" />
            )}
          </div>
        )}

        {/* Stopped indicator */}
        {isStopped && (
          <Badge variant="destructive" className="w-fit">{t('HALTED_USER')}</Badge>
        )}

        {/* Tool calls */}
        {hasToolCalls && (
          <div className="mt-4 space-y-2">
            {message.toolCallStatuses!.map((tc) => (
              <ToolCallCard key={tc.id} toolCall={tc} />
            ))}
          </div>
        )}

        {/* Tool running indicator */}
        {isToolRunning && hasToolCalls && message.toolCallStatuses!.some(tc => tc.status === 'running') && (
          <div className="mt-2 animate-pulse text-sm font-medium text-warning">
            {t('EXECUTING_SUBROUTINE')}
          </div>
        )}
      </Card>
    </div>
  );
}
