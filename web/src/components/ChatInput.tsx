import { useState, useRef, useEffect, type KeyboardEvent } from 'react';
import { useAppStore } from '../hooks/useAppStore';
import { useTranslation } from 'react-i18next';
import { Send, Square } from 'lucide-react';
import { Button } from './ui/button';
import { Textarea } from './ui/textarea';

export default function ChatInput() {
  const [text, setText] = useState('');
  const textareaRef = useRef<HTMLTextAreaElement>(null);
  const sendMessage = useAppStore((s) => s.sendMessage);
  const stopGeneration = useAppStore((s) => s.stopGeneration);
  const isProcessing = useAppStore((s) => s.isProcessing);
  const { t } = useTranslation();

  useEffect(() => {
    const el = textareaRef.current;
    if (!el) return;
    el.style.height = 'auto';
    el.style.height = `${Math.min(el.scrollHeight, 4 * 24)}px`;
  }, [text]);

  const handleSend = () => {
    if (!textareaRef.current) return;
    const trimmed = textareaRef.current.value.trim();
    if (!trimmed || isProcessing) return;
    sendMessage(trimmed);
    textareaRef.current.value = '';
    setText('');
    textareaRef.current.style.height = 'auto';
  };

  const handleKeyDown = (e: KeyboardEvent<HTMLTextAreaElement>) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      handleSend();
    }
  };

  return (
    <div className="relative z-10 border-t border-border bg-card p-4">
      <div className="flex items-end gap-2 rounded-lg border border-input bg-background p-2 shadow-sm">
        <Textarea
          ref={textareaRef}
          onChange={(e) => setText(e.target.value)}
          onKeyDown={handleKeyDown}
          placeholder={t('INPUT_PLACEHOLDER')}
          rows={1}
          className="!min-h-10 flex-1 resize-none border-0 px-2 py-2 shadow-none focus-visible:ring-0 focus-visible:ring-offset-0"
          spellCheck="false"
        />
        {isProcessing ? (
          <Button
            onClick={stopGeneration}
            variant="destructive"
            size="icon"
            aria-label={t('HALT')}
            title={t('HALT')}
          >
            <Square className="h-4 w-4" />
          </Button>
        ) : (
          <Button
            onClick={handleSend}
            disabled={!text.trim() || isProcessing}
            size="icon"
            aria-label={t('EXECUTE')}
            title={t('EXECUTE')}
          >
            <Send className="h-4 w-4" />
          </Button>
        )}
      </div>
    </div>
  );
}
