# Shadcn UI and Traditional Chinese Support Design

## Summary

Modernize the MCP server host web UI in `web/` from the current retro industrial console style to a shadcn/ui-inspired application shell while preserving existing behavior. Add Traditional Chinese (`zh-TW`) alongside Simplified Chinese (`zh`) and English (`en`). The redesign may reorganize the layout into a modern sidebar, central chat console, and right-side device status card group.

## Goals

- Preserve all current MCP, LLM, chat, session, device polling, reconnect, export, stop, regenerate, and settings behavior.
- Replace custom hard-edged visual treatments with shadcn/ui-style components and Tailwind design tokens.
- Use `lucide-react` icons consistently for actions, status, and navigation.
- Add Traditional Chinese translations and expose all three languages through a shadcn-style language control.
- Keep the implementation scoped to the React web app under `web/`.

## Non-Goals

- Do not change MCP server APIs or LLM request behavior.
- Do not add new product features beyond the UI modernization and language support.
- Do not rewrite the application state model unless required for the new language key.
- Do not introduce dark mode in this pass.

## Current State

The web app is a Vite React application in `web/`. It uses Zustand for application state, i18next/react-i18next for translations, Tailwind CSS v4 for styling, and `lucide-react` for some icons. The main functional components are:

- `web/src/App.tsx`: application shell and responsive layout.
- `web/src/components/TopBar.tsx`: title, settings button, new chat button, and two-language toggle.
- `web/src/components/HistoryPanel.tsx`: session list and session actions.
- `web/src/components/ChatPanel.tsx`: message list, export, clear, and input placement.
- `web/src/components/ChatInput.tsx`: textarea, send, and stop controls.
- `web/src/components/ChatMessage.tsx`: user, assistant, and tool message cards.
- `web/src/components/ToolCallCard.tsx`: tool-call status and expandable details.
- `web/src/components/DecoderResultTable.tsx`: analyzer result table.
- `web/src/components/DevicePanel.tsx`: connection, device information, capture state, and connect/disconnect controls.
- `web/src/components/SettingsDrawer.tsx`: settings form.
- `web/src/hooks/useAppStore.ts`: state, persisted settings, language setting, sessions, MCP connection, and LLM flow.
- `web/src/i18n/index.ts`: i18next setup for `en` and `zh`.
- `web/src/i18n/locales/en.json` and `web/src/i18n/locales/zh.json`: translation dictionaries.

The current UI has a thick-border retro hardware style. The requested result should feel like a modern shadcn/ui application: restrained neutral surfaces, rounded cards, clear spacing, semantic buttons, badges, inputs, sheet/dialog patterns, and icon-based action controls.

## Proposed Approach

Use a modern control-console layout:

- Left sidebar: session history, new-chat action, active-session state, delete controls.
- Center workspace: chat header, scrollable message stream, tool-call cards, and sticky composer.
- Right rail: device status cards grouped by connection, device information, capture state, and primary connection action.
- Settings panel: sheet-style side panel with labeled inputs and action footer.
- Top application header: product identity, connection indicator, language selector, settings button, and new-chat button where appropriate.

This keeps the existing app mental model while allowing a cleaner modern dashboard structure.

## Component Design

### Shared UI Primitives

Add small local shadcn-style primitives under `web/src/components/ui/` instead of relying on the shadcn CLI. This avoids network-dependent scaffolding and keeps the app self-contained.

Initial primitives:

- `button.tsx`: variants such as `default`, `secondary`, `outline`, `ghost`, `destructive`.
- `card.tsx`: `Card`, `CardHeader`, `CardTitle`, `CardDescription`, `CardContent`, `CardFooter`.
- `input.tsx`: text input.
- `textarea.tsx`: textarea.
- `badge.tsx`: status labels.
- `separator.tsx`: horizontal divider.
- `sheet.tsx`: lightweight sheet composition for the settings drawer.
- `select.tsx`: native select styled like shadcn for the language picker.

These primitives should be simple React components using Tailwind classes and `forwardRef` where useful. They should not introduce extra dependencies.

### Layout

`App.tsx` should become a full-height application shell:

- Root background: neutral gray, standard sans-serif font.
- Header: height around 56-64px, border bottom, compact product identity.
- Main area: CSS grid with columns similar to `280px minmax(0, 1fr) 320px` on desktop.
- History sidebar hidden or collapsible on small screens.
- Device panel hidden behind a mobile sheet or floating button on small screens.
- Chat workspace remains the primary focus.

### History Sidebar

`HistoryPanel.tsx` should render as a modern sidebar:

- Header row with `History`/localized title and `Plus` button.
- Session items as rounded list buttons.
- Active session shown with accent background and border.
- Delete action uses `Trash2` icon button.
- Session metadata remains date and message count.

### Chat Workspace

`ChatPanel.tsx`, `ChatInput.tsx`, and `ChatMessage.tsx` should shift from terminal cards to modern chat cards:

- Chat header shows current mode, ready state, export and clear actions.
- Empty state remains a simple centered status.
- User messages align right with primary-tinted surface.
- Assistant and tool messages align left with card surface.
- Message actions use `Copy`, `RefreshCw`, and related lucide icons.
- Composer uses a rounded textarea and icon send button; stop uses a destructive button.

### Tool Calls and Tables

`ToolCallCard.tsx` should render as compact nested cards:

- Status uses `Badge` variants.
- Expand/collapse uses `ChevronDown` or `ChevronRight`.
- Running elapsed time remains visible.
- Tool results preserve current parsing logic.

`DecoderResultTable.tsx` should become a shadcn-style table:

- Rounded card container.
- Sticky or visually distinct header row if simple to implement.
- Copy CSV uses a `Copy` icon button.
- Row count and show-all behavior stay unchanged.

### Device Status Cards

`DevicePanel.tsx` should become a right-side group of cards:

- Connection card with `Wifi`, `WifiOff`, or `Circle` status indicator.
- Device information card with `Cpu`, `Usb`, and `Activity` icons.
- Capture status card with badge and pulse only while capturing.
- Primary action button switches among reconnect, disconnect, and connect based on current state.

### Settings Sheet

`SettingsDrawer.tsx` should use the local `Sheet`, `Input`, `Textarea`, and `Button` primitives:

- Backdrop and right sheet remain.
- Fields remain the same: MCP URL, LLM base URL, API key, model, system prompt.
- Save/cancel behavior remains unchanged.
- The sheet header uses a settings icon and localized title.

## Internationalization Design

### Language Keys

Extend the app language type from:

```ts
language: 'en' | 'zh';
```

to:

```ts
language: 'en' | 'zh' | 'zh-TW';
```

`zh` remains Simplified Chinese and the default language. `zh-TW` is Traditional Chinese.

### i18next Resources

Update `web/src/i18n/index.ts` to import and register:

- `en`
- `zh`
- `zh-TW`

Add `web/src/i18n/locales/zh-TW.json` with the same key structure as `zh.json`. The Traditional Chinese file should translate existing strings and any new UI strings used by the modernized controls.

### Language Control

Replace the current two-state toggle in `TopBar.tsx` with a shadcn-style select/dropdown. It should display:

- `简体中文`
- `繁體中文`
- `English`

Changing language should call `updateSettings({ language: value })`, which persists the setting and calls `i18n.changeLanguage`.

### Persisted Settings Compatibility

Existing saved settings may contain `language: 'zh'` or `language: 'en'`. These remain valid. If a missing or unsupported language is encountered, the app should use `zh`.

## Data Flow

No MCP or LLM data flow changes are planned.

UI interaction flow:

1. User selects a language in the header control.
2. `TopBar` calls `updateSettings({ language })`.
3. `useAppStore.updateSettings` persists settings and calls `i18n.changeLanguage`.
4. Components re-render with `useTranslation`.

Chat and device flow remain as implemented:

- `ChatInput` sends through `useAppStore.sendMessage`.
- `ChatPanel` reads `messages` and processing state.
- `DevicePanel` reads connection, reconnect, device, and capture state.
- `SettingsDrawer` edits persisted settings.

## Error Handling

- Keep `ErrorBoundary` around the full application and per-message rendering.
- Language selection should tolerate unsupported persisted values by falling back to `zh`.
- Settings save should preserve the existing localStorage behavior.
- UI controls should keep disabled states during processing to avoid existing race conditions.

## Accessibility

- Icon-only buttons need accessible labels through `aria-label` and `title` where appropriate.
- Language selection needs a visible or screen-reader label.
- Buttons must keep visible focus states.
- Status badges should include localized text rather than color-only meaning.

## Testing and Verification

Use existing project checks:

- `npm run build` from `web/`.
- Manual browser verification of the redesigned shell.

Manual verification should cover:

- Default Simplified Chinese UI.
- Switching to English.
- Switching to Traditional Chinese.
- Creating, switching, and deleting sessions.
- Opening, editing, canceling, and saving settings.
- Sending chat messages, stopping generation, copying, regenerating, exporting, and clearing where available.
- Device connect/disconnect/reconnect states with existing demo/MCP behavior.
- Desktop layout and narrow/mobile layout.

## Implementation Notes

- Prefer local UI primitives over adding third-party dependencies.
- Continue using `lucide-react`; it is already installed.
- Keep text sizing appropriate for app controls, not landing-page scale.
- Use neutral surfaces, subtle borders, `rounded-md`/`rounded-lg`, and restrained shadows.
- Avoid nested cards unless they represent individual repeated items or tool result containers.
- Do not remove existing parsing logic in `DecoderResultTable` or `ToolCallCard`.
