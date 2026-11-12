# Zarescope design package v1.0

Zarescope's design system: the specification, the tokens the app and the website read, the brand and the icons.

```
design/
  docs/DESIGN.md             full spec (colour, type, spacing, components, layout, i18n, a11y)
  docs/contrast.md           measured WCAG ratios for every token pair
  tokens/zarescope.tokens.json   all tokens, both themes, ImGuiStyle + ImGuiCol mapping, waterfall LUT
  tokens/waterfall-lut.csv   256-entry "Zare Deep" LUT (OKLab interpolated)
  brand/logo/                symbol (64 grid), symbol-small (16 grid), horizontal + stacked lockups
                             variants: color-dark, color-light, mono-white, mono-black (text outlined)
  brand/app-icon/            app-icon-1024.svg/.png, Zarescope.iconset (run: iconutil -c icns Zarescope.iconset)
  brand/favicon/             favicon.svg (adapts to light/dark), favicon.ico (16/32/48), PNGs, apple-touch-icon
  icons/                     12 line icons, 24 grid, currentColor
```

Fonts are not part of this package: the app ships the official static TTFs (OFL) of Inter and JetBrains Mono in `resources/fonts/`.

---

## The rules in brief

```markdown
## Design system
- The UI follows `design/docs/DESIGN.md`. All colours, sizes and ImGui style values come from
  `design/tokens/zarescope.tokens.json`. Never hard-code a hex value or px size that isn't a token.
- Two themes: dark (default) and light. Theme switching swaps the whole token set; widget code
  references token names (e.g. accent, bg/surface), never raw colours.
- Accent = Horizon Amber (#FFB547 dark / #9A5B00 light). Status colours: live, recording,
  pass-imminent (violet), success, warning, error — always paired with an icon or label.
- Flat style only (Dear ImGui): no gradients, blur, translucent layering or elaborate animation.
- Fonts: Inter (UI, 13/18 body) + JetBrains Mono (frequencies, coordinates, timers). Load the
  Romanian comma-below glyphs U+0218–U+021B. Frequencies render as `137.900 000 MHz`, countdowns as `T−04:32`.
- Waterfall uses the "Zare Deep" LUT from `design/tokens/waterfall-lut.csv` in both themes.
- UI strings exist in EN and RO; leave ~30% room for longer Romanian text.
- Logo/app icon/favicon live in `design/brand/`; icons in `design/icons/` (24 grid, currentColor).
```
