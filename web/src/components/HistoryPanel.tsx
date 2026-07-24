import { MessagesSquare, Plus, Trash2 } from 'lucide-react';
import { useAppStore } from '../hooks/useAppStore';
import { useTranslation } from 'react-i18next';
import { Badge } from './ui/badge';
import { Button } from './ui/button';

function formatSessionDate(timestamp: number, language: string) {
  const date = new Date(timestamp);
  if (language.startsWith('zh')) {
    return {
      date: `${date.getMonth() + 1}月${date.getDate()}日`,
      time: `${String(date.getHours()).padStart(2, '0')}:${String(date.getMinutes()).padStart(2, '0')}`,
    };
  }

  return {
    date: date.toLocaleDateString('en-US', { month: 'short', day: 'numeric' }),
    time: `${String(date.getHours()).padStart(2, '0')}:${String(date.getMinutes()).padStart(2, '0')}`,
  };
}

export default function HistoryPanel() {
  const sessions = useAppStore((s) => s.sessions);
  const currentSessionId = useAppStore((s) => s.currentSessionId);
  const createNewSession = useAppStore((s) => s.createNewSession);
  const switchSession = useAppStore((s) => s.switchSession);
  const deleteSession = useAppStore((s) => s.deleteSession);
  const isProcessing = useAppStore((s) => s.isProcessing);
  const { t, i18n } = useTranslation();

  // Sort sessions by updatedAt descending
  const sortedSessions = Object.values(sessions).sort((a, b) => b.updatedAt - a.updatedAt);

  return (
    <div className="flex h-full w-full flex-col bg-sidebar text-sidebar-foreground">
      <div className="flex items-center justify-between gap-2 px-4 py-4">
        <div className="flex min-w-0 items-center gap-2">
          <MessagesSquare className="h-4 w-4 shrink-0 text-primary" aria-hidden="true" />
          <h2 className="truncate text-sm font-semibold uppercase tracking-wide">{t('TAPE_ARCHIVE')}</h2>
        </div>
        <Badge variant="secondary" className="rounded-full">{sortedSessions.length}</Badge>
      </div>

      <div className="px-4 pb-3">
        <Button
          onClick={createNewSession}
          disabled={isProcessing}
          className="w-full justify-center"
        >
          <Plus className="h-4 w-4" aria-hidden="true" />
          {t('INSERT_TAPE')}
        </Button>
      </div>

      <div className="min-h-0 flex-1 overflow-y-auto px-3">
        <ul className="flex flex-col gap-2 pb-4">
          {sortedSessions.map((session) => {
            const isActive = session.id === currentSessionId;
            const { date, time } = formatSessionDate(session.updatedAt, i18n.language);

            return (
              <li
                key={session.id}
                className={`group cursor-pointer rounded-lg border p-3 text-left transition-colors ${
                  isActive
                    ? 'border-primary/40 bg-primary/5'
                    : 'border-border bg-card hover:bg-accent'
                }`}
                onClick={() => !isProcessing && switchSession(session.id)}
              >
                <div className="flex items-start justify-between gap-2">
                  <div className="min-w-0">
                    <div className="truncate text-sm font-medium">
                      {session.title || t('UNTITLED')}
                    </div>
                    <div className="mt-3 flex items-center gap-2 text-xs text-muted-foreground">
                      <span className="flex flex-col leading-tight">
                        <span>{date}</span>
                        <span>{time}</span>
                      </span>
                      <span aria-hidden="true">/</span>
                      <Badge variant="outline" className="rounded-full px-2 py-0 text-[11px] font-normal">
                        {t('MSGS')}: {session.messages.length}
                      </Badge>
                    </div>
                  </div>
                  <Button
                    variant="ghost"
                    size="icon"
                    onClick={(e) => {
                      e.stopPropagation();
                      if (!isProcessing) deleteSession(session.id);
                    }}
                    disabled={isProcessing}
                    className="h-8 w-8 shrink-0 text-muted-foreground opacity-100 hover:text-destructive sm:opacity-0 sm:group-hover:opacity-100"
                    aria-label={t('ERASE_TAPE')}
                    title={t('ERASE_TAPE')}
                  >
                    <Trash2 className="h-4 w-4" aria-hidden="true" />
                  </Button>
                </div>
              </li>
            );
          })}
        </ul>
      </div>
    </div>
  );
}
