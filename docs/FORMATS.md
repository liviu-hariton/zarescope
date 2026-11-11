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

## IQ recordings (written and read)

A recording is two files with the same name, in `~/Library/Application Support/Zarescope/recordings/`:

```
2026-11-11T210705Z_100000000Hz.cu8    the samples
2026-11-11T210705Z_100000000Hz.json   the sidecar
```

The file name is the start time (UTC, to the second) and the centre frequency in hertz.

**`.cu8`.** Raw samples straight from the RTL2832U, with no header:

- interleaved I and Q, one unsigned byte each, I first;
- zero at 127.5, so a byte b means (b − 127.5) / 127.5;
- one complex sample per 2 bytes.

SatDump and most SDR tools read this format as "cu8" / "u8 IQ".

**Sidecar (`.json`).**

```json
{
  "zarescopeIq": 1,
  "format": "cu8",
  "centerFrequencyHz": 100000000,
  "sampleRateSps": 2400000,
  "startUtc": "2026-11-11T21:07:05.750Z",
  "gainTenthsDb": 297,
  "ppm": 0,
  "device": "RTL-SDR Blog V4 (Rafael Micro R828D)",
  "software": "Zarescope 0.2.0",
  "sampleCount": 24000000
}
```

| Key | Type | Meaning |
|---|---|---|
| `zarescopeIq` | integer | format version, currently 1 |
| `format` | string | always `"cu8"` |
| `centerFrequencyHz` | integer | the frequency the stick was tuned to |
| `sampleRateSps` | integer | complex samples per second, > 0 |
| `startUtc` | string | time of the first sample, ISO 8601 UTC with milliseconds and `Z` |
| `gainTenthsDb` | integer or `null` | tuner gain in tenths of a dB; `null` = automatic |
| `ppm` | integer | frequency correction applied |
| `device` | string | the stick (model and tuner), or what produced the file |
| `software` | string | the program and version that wrote it |
| `sampleCount` | integer | number of complex samples. Missing while recording; written when the recording ends. |

**Rules.**

- **Readers.** The required keys are `format`, `centerFrequencyHz`, `sampleRateSps` and `startUtc`; readers ignore keys they don't know.
- **No sidecar.** A recording without a sidecar is refused, because the bytes alone say nothing about frequency or rate.
- **One frequency per file.** A recording holds one frequency and one sample rate; retuning ends the recording.

## Presets (read)

`resources/presets.json`, copied into the app bundle:

```json
{
  "zarescopePresets": 1,
  "presets": [
    { "id": "fm-broadcast", "frequencyHz": 100000000, "mode": "wfm", "bandwidthHz": 180000 }
  ]
}
```

| Key | Type | Meaning |
|---|---|---|
| `zarescopePresets` | integer | format version, currently 1 |
| `id` | string | unique. The UI looks up the name by id in the string table, so names exist in every language. |
| `frequencyHz` | integer | the frequency to listen to, > 0 |
| `mode` | string | `wfm`, `nfm`, `am` or `off` (`off` for data signals such as ADS-B and Meteor, decoded in later milestones) |
| `bandwidthHz` | number, optional | channel width; missing or 0 = the mode's default |

## Coming next
- **Satellite catalog** (M4): `resources/sat_catalog.json`.
