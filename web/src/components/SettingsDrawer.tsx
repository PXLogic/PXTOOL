import { useState, useEffect } from 'react';
import { useAppStore } from '../hooks/useAppStore';
import { useTranslation } from 'react-i18next';
import { Button } from './ui/button';
import { Input } from './ui/input';
import { Sheet, SheetBody, SheetFooter, SheetHeader } from './ui/sheet';
import { Textarea } from './ui/textarea';

interface Settings {
  mcpServerUrl: string;
  llmBaseUrl: string;
  llmApiKey: string;
  llmModel: string;
  systemPrompt: string;
}

export default function SettingsDrawer({ open, onClose }: { open: boolean; onClose: () => void }) {
  const settings = useAppStore((s) => s.settings);
  const updateSettings = useAppStore((s) => s.updateSettings);
  const { t } = useTranslation();

  const [form, setForm] = useState<Settings>({
    mcpServerUrl: '',
    llmBaseUrl: '',
    llmApiKey: '',
    llmModel: '',
    systemPrompt: '',
  });

  useEffect(() => {
    if (open) {
      setForm({
        mcpServerUrl: settings?.mcpServerUrl ?? '',
        llmBaseUrl: settings?.llmBaseUrl ?? '',
        llmApiKey: settings?.llmApiKey ?? '',
        llmModel: settings?.llmModel ?? '',
        systemPrompt: settings?.systemPrompt ?? '',
      });
    }
  }, [open, settings]);

  const handleSave = () => {
    updateSettings(form);
    onClose();
  };

  const field = (label: string, key: keyof Settings, type: string = 'text') => (
    <div className="flex flex-col gap-2">
      <label htmlFor={`settings-${key}`} className="text-sm font-medium text-foreground">{label}</label>
      {type === 'textarea' ? (
        <Textarea
          id={`settings-${key}`}
          ref={(el) => {
            if (el) {
              el.style.height = 'auto';
              el.style.height = el.scrollHeight + 'px';
            }
          }}
          value={form[key] as string}
          onChange={(e) => {
            setForm({ ...form, [key]: e.target.value });
            e.target.style.height = 'auto';
            e.target.style.height = e.target.scrollHeight + 'px';
          }}
          rows={4}
          className="min-h-32 overflow-hidden resize-none font-mono text-xs"
        />
      ) : (
        <Input
          id={`settings-${key}`}
          type={type}
          value={form[key] as string}
          onChange={(e) => setForm({ ...form, [key]: e.target.value })}
          className="font-mono text-sm"
        />
      )}
    </div>
  );

  return (
    <Sheet
      open={open}
      onOpenChange={(nextOpen) => {
        if (!nextOpen) onClose();
      }}
      side="right"
      aria-label={t('MAINTENANCE_PNL')}
    >
      <SheetHeader>
        <h2 className="text-lg font-semibold text-foreground">{t('MAINTENANCE_PNL')}</h2>
      </SheetHeader>

      <SheetBody className="space-y-5">
        {field(t('SYS_CONN'), 'mcpServerUrl')}
        {field(t('AI_CORE'), 'llmBaseUrl')}
        {field(t('AI_AUTH'), 'llmApiKey', 'password')}
        {field(t('AI_MDL'), 'llmModel')}
        {field(t('SYS_PROMPT'), 'systemPrompt', 'textarea')}
      </SheetBody>

      <SheetFooter>
        <Button onClick={onClose} variant="outline">
          {t('CANCEL')}
        </Button>
        <Button onClick={handleSave}>
          {t('WRITE_ROM')}
        </Button>
      </SheetFooter>
    </Sheet>
  );
}
