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

## Satellite catalog (read)

`resources/sat_catalog.json`, copied into the app bundle. Frequencies live here and never in code.

```json
{
  "zarescopeCatalog": 1,
  "satellites": [
    {
      "noradId": 59051,
      "name": "Meteor-M N2-4",
      "active": true,
      "downlinks": [ { "frequencyHz": 137900000, "mode": "lrpt", "bandwidthHz": 150000 } ],
      "satdumpPipeline": "meteor_m2-x_lrpt",
      "satdumpArgs": ["--dc_block"],
      "recordSampleRateSps": 1024000
    }
  ]
}
```

| Key | Meaning |
|---|---|
| `noradId` | NORAD catalogue number, unique; the TLE is fetched by it |
| `name` | display name |
| `active` | false = known but not scheduled |
| `downlinks` | frequency (Hz), mode (`lrpt`, …) and channel width (Hz) of each transmitter |
| `satdumpPipeline` | the SatDump pipeline that decodes a recording (checked against SatDump 1.2.2: `meteor_m2-x_lrpt`). Empty = not recorded. |
| `satdumpArgs` | extra SatDump options, appended after `--samplerate N --baseband_format cu8` (optional) |
| `recordSampleRateSps` | sample rate for pass recordings: 1024000 (docs/DECISIONS.md, 2026-11-12) |

A satellite is recorded automatically when it is `active`, has a downlink with mode `lrpt`, a `satdumpPipeline` and a `recordSampleRateSps`.

## TLE cache (written and read)

`~/Library/Application Support/Zarescope/tle_cache/<noradId>.json`, one per satellite:

```json
{
  "fetchedUtc": "2026-11-11T21:42:43.824Z",
  "tle": "METEOR-M2 4\n1 59051U 24039A   26315.61954905 …\n2 59051  98.7240 …\n"
}
```

- **`fetchedUtc`.** When it was downloaded. It decides the 12-hour refresh and the age the UI shows.
- **`tle`.** CelesTrak's text, as received.

A damaged file is ignored, and the TLE is fetched again.

## Pass products (written)

One folder per pass in `~/Library/Application Support/Zarescope/products/`, named by AOS (UTC) and satellite: `2026-11-12_0330Z_Meteor-M_N2-4/`.

- **Images.** SatDump's PNGs with their sub-folders (`MSU-MR/…png`).
- **`satdump.log`.** SatDump's output with the ANSI colours removed.
- **`pass.json`.** What was recorded and what came out of it:

```json
{
  "satellite": "Meteor-M N2-3",
  "noradId": 57166,
  "aosUtc": "2026-11-12T07:05:30.712Z",
  "losUtc": "2026-11-12T07:06:30.712Z",
  "maxElevationDeg": 60.0,
  "frequencyHz": 137900000,
  "sampleRateSps": 1024000,
  "recordedFromUtc": "2026-11-12T07:05:30.736Z",
  "recordedUntilUtc": "2026-11-12T07:06:30.759Z",
  "recordingBytes": 122814464,
  "stoppedByUser": false,
  "recording": "2026-11-12T070530Z_137900000Hz.cu8",
  "decode": {
    "satdumpVersion": "1.2.2",
    "pipeline": "meteor_m2-x_lrpt",
    "exitCode": 0,
    "timedOut": false,
    "error": "",
    "images": ["MSU-MR/msu_mr_rgb_221.png"]
  }
}
```

The baseband recording (in `recordings/`, the usual `.cu8` + sidecar) is deleted when `images` isn't empty, and kept otherwise. The journal (M6) will index these folders.

## Settings (written and read)

`~/Library/Application Support/Zarescope/settings.json`, written by the app (atomically: a `.tmp` file renamed over it).

```json
{
  "zarescopeSettings": 1,
  "theme": "dark",
  "language": "en",
  "station": { "latDeg": 44.4268, "lonDeg": 26.1025, "altitudeM": 80.0 },
  "minElevationDeg": 20.0,
  "autoRecord": false,
  "keepBaseband": false,
  "defaultSampleRateSps": 2400000,
  "ppm": 0,
  "dataFolder": "",
  "discoverySeen": true
}
```

| Key | Meaning |
|---|---|
| `theme` | `dark` or `light` |
| `language` | `en` or `ro` |
| `station` | the station, or `null` when not set; latitude ±90, longitude ±180, altitude −500…9000 m |
| `minElevationDeg` | passes below this are not listed or recorded (0–60) |
| `autoRecord` | the pass scheduler is on at start-up |
| `keepBaseband` | keep pass recordings after a successful decode |
| `defaultSampleRateSps` | the rate a stick opens with; one of the rates in `RtlSdrSource.h` |
| `ppm` | frequency correction, −200…200 |
| `dataFolder` | where recordings, products, the TLE cache and the journal go; empty = Application Support. Applies after a restart. |
| `discoverySeen` | false on the first run, which opens on Discover |

Unknown keys are ignored; a missing or invalid value keeps its default.

## Journal (written and read)

`journal.sqlite` in the data folder (SQLite 3, WAL mode). The schema version is `PRAGMA user_version`, currently 1:

```sql
CREATE TABLE events (
    id       INTEGER PRIMARY KEY,
    time_utc TEXT NOT NULL,              -- ISO 8601 UTC with milliseconds; sorts as text
    kind     TEXT NOT NULL,              -- pass, images, aircraft, recording
    title    TEXT NOT NULL,
    detail   TEXT NOT NULL DEFAULT '',
    path     TEXT NOT NULL DEFAULT '',   -- a file or folder, if any
    data     TEXT NOT NULL DEFAULT '{}', -- JSON object
    key      TEXT UNIQUE                 -- one row per key when set
);
CREATE INDEX events_time ON events (time_utc);
CREATE INDEX events_kind_time ON events (kind, time_utc);
```

| Kind | Title | Detail | Path | Data | Key |
|---|---|---|---|---|---|
| `pass` | satellite | the outcome, in the UI's language | the SatDump log when it failed | `ok`, `images`, `failure`, `error`, `folder` | — |
| `images` | satellite | `N PNG` | the products folder | as for the pass | — |
| `aircraft` | callsign, or ICAO | `ICAO 4B1806` | — | `icao`, `callsign` | `aircraft:<UTC day>:<ICAO>` |
| `recording` | file name | size | the `.cu8` | — | — |

**Exports.** CSV has the columns `time_utc,kind,title,detail,path,data`, every field quoted, CRLF line ends (RFC 4180). JSON is an array of objects with the same fields, with `data` embedded as an object. Both follow the filter shown, newest first.

## Coming next
