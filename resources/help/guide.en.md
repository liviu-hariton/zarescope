<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- The user guide, shown in the app's Help tab (Help › Zarescope Help, ⌘?).
     Plain Markdown; src/ui/Guide.h lists what the app renders. Keep the
     Romanian guide (guide.ro.md) in step: same sections, same order, same ids. -->

# Zarescope Help

Zarescope turns an RTL-SDR stick into a window on the radio sky: the live spectrum, aircraft on a radar, and Meteor-M weather satellites that it records and decodes by itself. It only receives; it never transmits.

<!-- id: start -->
## Getting started

The Discover tab walks you through these steps, and ticks each one off when it's done.

1. Set your location. Open Settings (the cog at the top right, or ⌘,), then either type your latitude and longitude or click Use my current location. The location decides which satellite passes you see and how far away aircraft are.
2. Connect a receiver. Plug in the stick, click the receiver chip at the top right (No receiver), then Open next to it. No stick yet? Open a recording instead (⌘O).
3. Pick something to receive. In Discover, each entry says what you'll hear and which antenna helps: FM radio is the easiest first signal.

<!-- id: receiver -->
## The receiver

Any stick with an RTL2832U chip works: the RTL-SDR Blog V3 and V4, the Nooelec NESDR SMArt and generic R820T sticks. Zarescope reads which tuner the stick really has; if it doesn't match what the stick claims to be, the receiver panel says so, which is the usual sign of a counterfeit.

| Control | What it does |
|---|---|
| Gain | How strongly the tuner amplifies. More hears weaker signals, but strong nearby stations then overload it and spread across the spectrum. Auto lets the tuner decide; start there. |
| Sample rate | How wide a slice of the spectrum the stick captures, in million samples per second (MS/s). 2.4 MS/s shows 2.4 MHz at once. Aircraft need 2 MS/s; the ADS-B preset sets it. |
| Frequency correction (ppm) | Fixes a stick whose clock runs slightly fast or slow, so stations land where they should. Sticks with a TCXO (V3, V4) rarely need it. |
| Bias-tee | Sends power up the antenna cable for an amplifier at the antenna. Off unless you turn it on, after a warning: on an ordinary antenna it does nothing, and on some equipment it can do harm. |

<!-- id: spectrum -->
## Spectrum and waterfall

The top half is the spectrum: signal strength across the frequencies the stick captures, in dBFS (0 is the loudest the stick can measure, so real signals are negative numbers). The waterfall below it is the same thing over time: each new line is a moment, scrolling down, brighter where the signal is stronger. A station shows as a band; a short burst, like a radio call, as a short dash.

- Type a frequency: click the frequency at the top and type the digits, then press Return.
- Change one digit: scroll over it, or click it and use the ↑ and ↓ keys.
- Click the waterfall or the spectrum to tune there; scroll over them to step up and down.
- FFT size (the button above the spectrum, on the right) decides how finely the spectrum is split: more points show narrower signals, but update more slowly.
- Waterfall floor and ceiling set which levels show as dark and as bright. Fit to signal sets them from what is on screen.

A recording plays on the frequency it was made on: you can tune within it, but not away from it.

<!-- id: listening -->
## Listening

Choose how the signal becomes sound with the mode buttons at the top: WFM for broadcast FM radio, NFM for walkie-talkies and radio amateurs, AM for aircraft. Off silences it.

The listening frequency is the marker on the spectrum. Click the speaker button for the sound settings:

| Setting | What it does |
|---|---|
| Volume | How loud. |
| Squelch | Silence while the signal is weaker than this, so you hear only when someone talks. Slide it just above the noise. |
| Bandwidth | How wide a slice is turned into sound. Too narrow cuts the voice; too wide adds noise and the station next door. |
| De-emphasis | WFM only: 50 µs in Europe, 75 µs in the Americas. |

The presets (the arrow next to the frequency, or Receiver › Tune To) set frequency, mode and bandwidth in one go.

<!-- id: recording -->
## Recording and playback

Record (⇧⌘R) saves the raw signal the stick receives, as IQ samples: a .cu8 file and a small .json file beside it with the frequency, sample rate, time and gain. You can play it back later in Zarescope (⌘O) as if the stick were connected, or open it in other SDR software. A minute at 2.4 MS/s is about 290 MB, and the banner shows the size as it grows.

Recordings go to the data folder: Application Support › Zarescope › recordings, unless you chose another folder in Settings. Changing the frequency or the sample rate ends a recording, because a file holds only one of each.

<!-- id: aircraft -->
## Aircraft

Aircraft broadcast who they are, where, how high and how fast on 1090 MHz (ADS-B). Choose Aircraft (ADS-B) in the presets: the stick tunes to 1090 MHz at 2 MS/s and the Aircraft tab opens.

The table lists each aircraft: callsign, altitude, speed, distance from you, and how long ago it was last heard. The radar puts them around your location, with their tracks behind them; click one to select it in the table. Aircraft disappear a minute after their last message.

Range depends mostly on the antenna: high up and clear of buildings, a simple antenna sees a hundred kilometres or more.

<!-- id: satellites -->
## Weather satellites

The Satellites tab lists the Meteor-M passes over your location for the next two days: when each starts, how high it climbs (maximum elevation) and how long it lasts. The card shows a countdown to the next one; the sky plot shows the selected pass's path over your sky, north up. A notification arrives five minutes before each pass. Orbits come from CelesTrak and refresh at most twice a day; the age is shown under the list.

Passes below the minimum elevation (Settings, 20° to start with) aren't listed: low passes rarely give a usable picture.

### Automatic recording

Turn on Record and decode passes automatically, and Zarescope handles each pass on its own:

1. At the start of the pass it takes the receiver, tunes to the satellite and records until the satellite sets. A banner shows the time left; Stop recording ends it early.
2. Then it gives the receiver back exactly as you had it.
3. SatDump turns the recording into images, in Application Support › Zarescope › products, one folder per pass. The banner and a notification say how it went; Show in Finder opens the folder.

This needs SatDump, installed separately: in Terminal, brew install --cask satdump, then Look for SatDump Again (Receiver menu). The Mac must stay awake and the receiver connected; a sleeping Mac misses passes. After a successful decode the recording is deleted (it is about 2 MB a second), unless Settings says to keep it.

<!-- id: journal -->
## Journal

The Journal tab keeps what was received: every satellite pass and its outcome, the images, each aircraft once a day, and the recordings you made. Filter by kind or search by name; Show opens a file or folder in Finder. Export CSV and Export JSON save what the filter shows, for a spreadsheet or another program.

<!-- id: settings -->
## Settings

| Setting | What it does |
|---|---|
| Theme | Dark or light. |
| Text size | Standard, Large or Larger: the whole interface grows, text and controls together. Also ⌘+, ⌘- and ⌘0. |
| Station location | Latitude, longitude and altitude; Use my current location asks macOS (it asks your permission the first time). |
| Minimum pass elevation | Passes lower than this aren't listed or recorded. |
| Record and decode passes automatically | The same switch as in the Satellites tab. |
| Keep pass recordings after decoding | Keeps the raw recording of a decoded pass, about 2 MB a second. |
| Receiver | The sample rate a stick opens with, and its frequency correction. |
| Data folder | Where recordings, images, the orbit cache and the journal go. Takes effect when Zarescope restarts. |
| Language | English or Romanian. |
| Updates | Check for a new version once a day: off until you turn it on. Check now asks at once, as does Zarescope › Check for Updates…. Zarescope asks zarescope.eu only for the newest version number; nothing about you is sent, and nothing is downloaded or installed. When a newer version exists, the status bar says so; click it to open the download page. |

Settings are saved as you change them.

<!-- id: shortcuts -->
## Keyboard shortcuts

| Keys | Action |
|---|---|
| ⌘, | Settings |
| ⇧⌘K | Connect a receiver |
| ⌘O | Open a recording |
| ⇧⌘R | Start or stop recording |
| ⌘1 to ⌘6 | Discover, Spectrum, Aircraft, Satellites, Journal, Help |
| ⌘+ and ⌘- | Bigger or smaller text |
| ⌘0 | Standard text size |
| ⌃⌘F | Full screen |
| ⌘? | This guide |
| ⌘W | Close the About window, or Zarescope |
| ⌘Q | Quit |
| ↑ and ↓ | On a frequency digit: change it. In this guide: scroll. |
| Esc | Close a panel or the About window |

<!-- id: troubleshooting -->
## Troubleshooting

| Problem | What to try |
|---|---|
| No receiver found | Unplug the stick and plug it into another port, without a hub, then Search again. Close other SDR programs: only one program can use the stick at a time. |
| Tuner doesn't match | The stick claims to be one model but has another model's tuner: it isn't what it says it is. It may still work, but with that tuner's limits. |
| No sound | Check that a mode is chosen (not Off), the volume, and the squelch: slide it to the left. The speaker panel says if macOS has no sound output. |
| The spectrum is full of copies of one station | The gain is too high: lower it, or set it to Auto. |
| No aircraft | Use the ADS-B preset, so the stick runs at 2 MS/s on 1090 MHz. Put the antenna by a window, as high as you can. |
| A pass gave no image | The signal was too weak: wait for a higher pass (above 40°), and use the V-dipole horizontally, outdoors, under open sky. The pass's log is in its folder (Show log). |
| Use my current location fails | Turn on Location Services for Zarescope in System Settings › Privacy & Security, or type the coordinates. |
| macOS won't open the app | The first time, open System Settings › Privacy & Security and click Open Anyway. |

<!-- id: glossary -->
## Words used here

| Word | Meaning |
|---|---|
| SDR | Software-defined radio: the stick only digitises the radio signal; the computer does the rest. |
| IQ samples | The raw digitised signal, two numbers per moment (I and Q). What Record saves. |
| dBFS | Decibels below full scale: 0 is the strongest the stick can measure; −60 is a million times weaker in power. |
| FFT | The calculation that turns the signal into a spectrum. |
| Squelch | A gate that keeps the sound silent while no one transmits. |
| ADS-B | The signal aircraft broadcast with their position, altitude and speed. |
| AOS and LOS | Acquisition and loss of signal: when a satellite rises above your horizon and when it sets. |
| Elevation | How high above the horizon: 0° at the horizon, 90° straight overhead. |
| LRPT | The digital picture signal of the Meteor-M satellites, at 137.9 MHz. |
| TLE | Two-line elements: the orbit description from which passes are predicted. |
| ppm | Parts per million: how far the stick's clock is off. |
