---
name: karpathy-guidelines
description: Behavioral guidelines to reduce common LLM coding mistakes. Use when writing, reviewing, or refactoring code to avoid overcomplication, make surgical changes, surface assumptions, and define verifiable success criteria.
license: MIT
---

# Karpathy Guidelines

Behavioral guidelines to reduce common LLM coding mistakes, derived from [Andrej Karpathy's observations](https://x.com/karpathy/status/2015883857489522876) on LLM coding pitfalls.

**Tradeoff:** These guidelines bias toward caution over speed. For trivial tasks, use judgment.

## 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:

- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

## 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

## 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:

- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it - don't delete it.

When your changes create orphans:

- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: Every changed line should trace directly to the user's request.

## 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:

- "Add validation" -> "Write tests for invalid inputs, then make them pass"
- "Fix the bug" -> "Write a test that reproduces it, then make it pass"
- "Refactor X" -> "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:

```text
1. [Step] -> verify: [check]
2. [Step] -> verify: [check]
3. [Step] -> verify: [check]
```

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.

## 5. Qt CJK UI Text Rendering

**When Chinese/Japanese/Korean text looks blurry, uneven, clipped, or one glyph appears smaller, treat it as a font + metrics bug, not just a cosmetic color issue.**

For Qt widgets in this project:

- Use `QApplication::font()` as the source of truth for UI font family.
- On Windows, prefer the app-level CJK UI font already configured in `main.cpp` (`Microsoft YaHei UI`, then `Microsoft YaHei` fallback).
- Do not hardcode `"Monospace"` or English-only fonts for UI text that may contain Chinese.
- Avoid tiny fixed `10px` text for CJK controls unless the control is intentionally dense, such as waveform labels.
- Use at least `12px` for normal menus, toolbar buttons, log panels, search controls, and combo box popups.
- Use `font.setWeight(QFont::Normal)` and `font.setBold(false)` for normal CJK UI text.
- Prefer `QFont::PreferAntialias` when manually constructing fonts for small controls.
- Apply font both ways when Qt native/style-sheet rendering is involved: call `setFont(...)` on the widget/action and include `font-family`, `font-size`, and `font-weight` in local QSS.

For menus and popup lists:

- For `QMenu`, set font recursively on the menu, actions, and submenus.
- For `QMenu`, also set local QSS because Windows native menu rendering can ignore `QMenu::setFont`.
- Use popup item metrics similar to: `padding: 4px 12px`, `min-height: max(20, fontSize + 8)`.
- For `QComboBox` / `DsComboBox`, set the combo font, popup view font, popup item height, and `QAbstractItemView::item` QSS.
- If matching the channel header context menu style, use selected/hover background `#1185D1` and white selected text.

For buttons with translated CJK text:

- Do not rely on `sizeHint()` alone after language changes.
- Recompute button width after `UpdateLanguage()` changes text.
- Use `QFontMetrics::horizontalAdvance(...)` with the final font and add generous horizontal padding.
- Give CJK buttons enough height: use at least `28px`, or `max(28, fontSize + 16)`.
- If two nearby buttons should visually match, compute a shared minimum width from the widest translated text.
- Avoid `font-weight: 500` for small CJK button text; it can trigger synthesized weights and uneven glyphs.

Verification checklist:

- Test Chinese translations, not only English.
- Check the main control and its popup/submenu separately.
- Check text after `UpdateLanguage()`, `UpdateFont()`, and theme/style refresh paths.
- Build at least the touched object file; run full link after closing any running `PXTOOL.exe`.

---

**These guidelines are working if:** fewer unnecessary changes in diffs, fewer rewrites due to overcomplication, and clarifying questions come before implementation rather than after mistakes.
