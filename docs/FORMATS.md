# File formats

Zarescope's own file formats. Each format is documented here when the code that reads or writes it lands.

## Design tokens (read)

`design/tokens/zarescope.tokens.json` belongs to the design package (`design/README.md`), not to Zarescope. The app reads it from `Zarescope.app/Contents/Resources/` and requires:

- `color.dark` and `color.light` with the same token names, each `#RRGGBB`;
- `logo.dark` and `logo.light`, each with `rings` and `pass`;
- `typography.scale` entries with `font` (`ui` or `mono`), `weight`, `size` and `lineHeight` (optional `tracking`, `uppercase`);
- `size`, `spacing`, `radius` and `border` as named numbers;
- `imgui.style`, where each value is a number or a pair of numbers, keyed by `ImGuiStyle` field name;
- `imgui.colors`, keyed by `ImGuiCol` name without the prefix, with one of three values:
  - a token name;
  - `transparent`;
  - `#RRGGBB @NN%`, a colour with an opacity;
- `waterfall.lut256`: exactly 256 `#RRGGBB` entries, from the noise floor to the ceiling.

## Coming next

- **IQ recordings** (M1): raw `cu8`, with a JSON sidecar holding the centre frequency, sample rate, UTC timestamp and gain.
- **Presets** (M2): `resources/presets.json`.
- **Satellite catalog** (M4): `resources/sat_catalog.json`.
