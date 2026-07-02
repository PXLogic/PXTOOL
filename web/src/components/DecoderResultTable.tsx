import { useState } from 'react';
import { Copy } from 'lucide-react';
import { useTranslation } from 'react-i18next';
import { Button } from './ui/button';
import { Card } from './ui/card';

interface Row {
  [key: string]: string | number;
}

function parseData(data: string): { rows: Row[]; columns: string[] } | null {
  try {
    const parsed = JSON.parse(data);
    if (Array.isArray(parsed)) {
      if (parsed.length === 0) return { rows: [], columns: [] };
      const columns = Object.keys(parsed[0]);
      return { rows: parsed, columns };
    }
    if (parsed.rows && Array.isArray(parsed.rows)) {
      const columns = parsed.columns ?? (parsed.rows.length > 0 ? Object.keys(parsed.rows[0]) : []);
      return { rows: parsed.rows, columns };
    }
    return null;
  } catch {
    return null;
  }
}

function isPwmResult(columns: string[]): boolean {
  const lower = columns.map((c) => c.toLowerCase());
  return lower.some((c) => c.includes('duty')) && lower.some((c) => c.includes('period'));
}

function pwmColumns(columns: string[]): string[] {
  return columns.map((c) => {
    const l = c.toLowerCase();
    if (l.includes('duty')) return 'Duty Cycle';
    if (l.includes('period')) return 'Period';
    if (l.includes('time')) return 'Time';
    return c;
  });
}

function genericColumns(columns: string[]): string[] {
  return columns.map((c) => {
    const l = c.toLowerCase();
    if (l === 'start_sample' || l === 'startsample') return 'Start Sample';
    if (l === 'end_sample' || l === 'endsample') return 'End Sample';
    if (l === 'type') return 'Type';
    if (l === 'value') return 'Value';
    return c;
  });
}

export default function DecoderResultTable({ data }: { data: string }) {
  const [showAll, setShowAll] = useState(false);
  const { t } = useTranslation();
  const result = parseData(data);

  if (!result) {
    return (
      <div className="whitespace-pre-wrap rounded-md bg-muted p-3 font-mono text-xs text-muted-foreground">
        {data}
      </div>
    );
  }

  const { rows, columns } = result;
  const pwm = isPwmResult(columns);
  const headers = pwm ? pwmColumns(columns) : genericColumns(columns);
  const maxRows = 100;
  const display = showAll ? rows : rows.slice(0, maxRows);
  const truncated = rows.length > maxRows && !showAll;
  const rowSummary = truncated
    ? t('SHOWING_ROWS', { shown: maxRows, total: rows.length })
    : `${rows.length} ${t('ROWS')}`;

  const copyCsv = () => {
    const header = headers.join(',');
    const body = display.map((r) => columns.map((c) => r[c] ?? '').join(',')).join('\n');
    navigator.clipboard.writeText(`${header}\n${body}`);
  };

  return (
    <Card className="overflow-hidden shadow-none">
      <div className="flex items-center justify-between gap-3 border-b border-border px-3 py-2">
        <span className="text-xs text-muted-foreground">{rowSummary}</span>
        <Button onClick={copyCsv} variant="ghost" size="sm">
          <Copy className="h-3.5 w-3.5" />
          {t('COPY_CSV')}
        </Button>
      </div>
      <div className="max-h-64 overflow-x-auto">
        <table className="w-full text-xs">
          <thead className="bg-muted/70">
            <tr className="border-b border-border">
              <th className="px-2 py-2 text-left font-medium text-muted-foreground">#</th>
              {headers.map((h) => (
                <th key={h} className="px-2 py-2 text-left font-medium text-muted-foreground">{h}</th>
              ))}
            </tr>
          </thead>
          <tbody>
            {display.map((row, i) => (
              <tr key={i} className="border-b border-border/60 hover:bg-muted/50">
                <td className="px-2 py-1.5 text-muted-foreground">{i + 1}</td>
                {columns.map((c) => (
                  <td key={c} className="px-2 py-1.5 text-foreground">{row[c] ?? ''}</td>
                ))}
              </tr>
            ))}
          </tbody>
        </table>
      </div>
      {truncated && (
        <Button onClick={() => setShowAll(true)} variant="ghost" size="sm" className="w-full rounded-none">
          {t('SHOW_ALL_ROWS', { total: rows.length })}
        </Button>
      )}
    </Card>
  );
}
