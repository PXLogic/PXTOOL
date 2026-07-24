import { forwardRef } from 'react';
import type { HTMLAttributes } from 'react';

import { cn } from '../../lib/utils';

export interface BadgeProps extends HTMLAttributes<HTMLDivElement> {
  variant?: 'default' | 'secondary' | 'success' | 'warning' | 'destructive' | 'outline';
}

const variants: Record<NonNullable<BadgeProps['variant']>, string> = {
  default: 'bg-primary text-primary-foreground',
  secondary: 'bg-secondary text-secondary-foreground',
  success: 'bg-success/10 text-success ring-1 ring-success/20',
  warning: 'bg-warning/10 text-warning ring-1 ring-warning/20',
  destructive: 'bg-destructive/10 text-destructive ring-1 ring-destructive/20',
  outline: 'border border-border text-foreground',
};

export const Badge = forwardRef<HTMLDivElement, BadgeProps>(({ className, variant = 'default', ...props }, ref) => (
  <div
    ref={ref}
    className={cn(
      'inline-flex items-center rounded-full px-2.5 py-0.5 text-xs font-semibold transition-colors',
      variants[variant],
      className,
    )}
    {...props}
  />
));
Badge.displayName = 'Badge';
