import { MessagesSquare, Plus, Trash2 } from 'lucide-react';
import { useAppStore } from '../hooks/useAppStore';
import { useTranslation } from 'react-i18next';
import { Badge } from './ui/badge';
import { Button } from './ui/button';
import { Card } from './ui/card';
import { Separator } from './ui/separator';

export default function HistoryPanel() {
  const sessions = useAppStore((s) => s.sessions);
  const currentSessionId = useAppStore((s) => s.currentSessionId);
  const createNewSession = useAppStore((s) => s.createNewSession);
  const switchSession = useAppStore((s) => s.switchSession);
  const deleteSession = useAppStore((s) => s.deleteSession);
  const isProcessing = useAppStore((s) => s.isProcessing);
  const { t } = useTranslation();

  // Sort sessions by updatedAt descending
  const sortedSessions = Object.values(sessions).sort((a, b) => b.updatedAt - a.updatedAt);

  return (
    <div className="w-full h-full flex flex-col gap-4 p-4 overflow-y-auto bg-slate-50/60">
      <div className="flex items-center justify-between gap-3">
        <div className="min-w-0">
          <div className="flex items-center gap-2 text-slate-900">
            <MessagesSquare className="h-5 w-5 text-blue-600" aria-hidden="true" />
            <h2 className="truncate text-sm font-semibold uppercase tracking-wide">{t('TAPE_ARCHIVE')}</h2>
          </div>
        </div>
        <Badge variant="secondary">{sortedSessions.length}</Badge>
      </div>

      <Button
        onClick={createNewSession}
        disabled={isProcessing}
        className="w-full justify-start"
      >
        <Plus className="h-4 w-4" aria-hidden="true" />
        {t('INSERT_TAPE')}
      </Button>

      <Separator />

      <Card className="flex-1 overflow-hidden p-1 shadow-none">
        <div className="flex h-full flex-col gap-1 overflow-y-auto">
          {sortedSessions.map((session) => {
            const isActive = session.id === currentSessionId;
            const dateStr = new Date(session.updatedAt).toLocaleDateString(undefined, { month: 'short', day: 'numeric', hour: '2-digit', minute: '2-digit' });

            return (
              <div
                key={session.id}
                className={`cursor-pointer rounded-md border p-3 transition-colors ${
                  isActive
                    ? 'border-blue-200 bg-blue-50 text-blue-950'
                    : 'border-transparent bg-transparent text-slate-700 hover:bg-slate-100'
                }`}
                onClick={() => !isProcessing && switchSession(session.id)}
              >
                <div className="flex items-start justify-between gap-2">
                  <div className="min-w-0">
                    <div className="truncate text-sm font-medium">
                      {session.title || t('UNTITLED')}
                    </div>
                    <div className="mt-1 flex items-center gap-2 text-xs text-slate-500">
                      <span>{dateStr}</span>
                      <span aria-hidden="true">/</span>
                      <Badge variant="outline" className="px-1.5 py-0 text-[10px]">
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
                    className="h-8 w-8 shrink-0 text-slate-400 hover:text-red-600"
                    aria-label={t('ERASE_TAPE')}
                    title={t('ERASE_TAPE')}
                  >
                    <Trash2 className="h-4 w-4" aria-hidden="true" />
                  </Button>
                </div>
              </div>
            );
          })}
        </div>
      </Card>
    </div>
  );
}
