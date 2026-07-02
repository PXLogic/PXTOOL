import type { HTMLAttributes, ReactNode } from 'react';
import { X } from 'lucide-react';

import { cn } from '../../lib/utils';
import { Button } from './button';

export interface SheetProps extends HTMLAttributes<HTMLDivElement> {
  open: boolean;
  onOpenChange: (open: boolean) => void;
  side?: 'left' | 'right';
  children: ReactNode;
}

export function Sheet({
  open,
  onOpenChange,
  side = 'right',
  children,
  className,
  ...props
}: SheetProps) {
  if (!open) {
    return null;
  }

  return (
    <div className="fixed inset-0 z-50" role="dialog" aria-modal="true">
      <div className="absolute inset-0 bg-slate-950/40" onClick={() => onOpenChange(false)} />
      <div
        className={cn(
          'fixed top-0 z-50 flex h-full w-full max-w-md flex-col border-slate-200 bg-white text-slate-950 shadow-lg',
          side === 'left' ? 'left-0 border-r' : 'right-0 border-l',
          className,
        )}
        {...props}
      >
        <Button
          aria-label="Close"
          className="absolute right-4 top-4"
          size="icon"
          variant="ghost"
          onClick={() => onOpenChange(false)}
        >
          <X className="h-4 w-4" />
        </Button>
        {children}
      </div>
    </div>
  );
}

export function SheetHeader({ className, ...props }: HTMLAttributes<HTMLDivElement>) {
  return <div className={cn('space-y-1.5 p-6 pr-14', className)} {...props} />;
}

export function SheetBody({ className, ...props }: HTMLAttributes<HTMLDivElement>) {
  return <div className={cn('flex-1 overflow-y-auto p-6 pt-0', className)} {...props} />;
}

export function SheetFooter({ className, ...props }: HTMLAttributes<HTMLDivElement>) {
  return <div className={cn('flex items-center justify-end gap-2 border-t border-slate-200 p-6', className)} {...props} />;
}
