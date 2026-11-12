// SPDX-License-Identifier: GPL-3.0-or-later
//
// The string table. See Strings.h.

#include "ui/Strings.h"

#include <array>

namespace zarescope::ui {

namespace {

struct Entry {
    StringId id;
    std::string_view english;
    std::string_view romanian;
};

// One row per StringId, in enum order. The static_assert below fails the
// build if a row is missing or out of place.
constexpr std::array<Entry, kStringCount> kStrings{{
    // Empty state
    {StringId::EmptyTitle, "Nothing to show yet", "Nimic de afișat încă"},
    {StringId::EmptyBody, "Connect an RTL-SDR stick to see the live spectrum, or open a recording.",
     "Conectează un stick RTL-SDR ca să vezi spectrul live sau deschide o înregistrare."},
    {StringId::ConnectReceiver, "Connect receiver", "Conectează receptorul"},
    {StringId::OpenRecording, "Open recording…", "Deschide o înregistrare…"},
    // Top bar
    {StringId::ReceiverChipNone, "No receiver", "Niciun receptor"},
    {StringId::ReceiverChipOpening, "Opening…", "Se deschide…"},
    {StringId::ChipLive, "LIVE", "LIVE"},
    {StringId::ChipReplay, "REPLAY", "REDARE"},
    {StringId::ThemeLabel, "Theme", "Temă"},
    {StringId::ThemeDark, "Dark", "Întunecată"},
    {StringId::ThemeLight, "Light", "Luminoasă"},
    {StringId::GainLabel, "Gain", "Câștig"},
    {StringId::GainAuto, "Auto", "Automat"},
    {StringId::GainAutomaticTuner, "Automatic (the tuner decides)", "Automat (decide tunerul)"},
    {StringId::Record, "Record", "Înregistrează"},
    {StringId::StopRecording, "Stop recording", "Oprește înregistrarea"},
    {StringId::SettingsLabel, "Settings", "Setări"},
    {StringId::LanguageLabel, "Language", "Limbă"},
    {StringId::ModeLabel, "How the signal becomes sound (demodulation)",
     "Cum devine semnalul sunet (demodulare)"},
    {StringId::ModeOff, "Off", "Oprit"},
    {StringId::AudioLabel, "Sound", "Sunet"},
    {StringId::VolumeLabel, "Volume", "Volum"},
    {StringId::SquelchLabel, "Squelch: silence below this signal level",
     "Squelch: liniște sub acest nivel de semnal"},
    {StringId::SquelchOffValue, "off", "oprit"},
    {StringId::SignalLevel, "Signal in the channel: {}", "Semnal în canal: {}"},
    {StringId::BandwidthLabel, "Channel width", "Lățimea canalului"},
    {StringId::DeemphasisLabel, "De-emphasis", "De-emfază"},
    {StringId::Deemphasis50, "50 µs (Europe)", "50 µs (Europa)"},
    {StringId::Deemphasis75, "75 µs (Americas)", "75 µs (Americi)"},
    {StringId::NoSoundOutput, "No sound: {}", "Fără sunet: {}"},
    {StringId::PresetsLabel, "Presets", "Presetări"},
    {StringId::PresetFmBroadcast, "FM radio", "Radio FM"},
    {StringId::PresetAirband, "Airband (AM)", "Bandă aeronautică (AM)"},
    {StringId::PresetAmateur2m, "2 m amateur band", "Banda de 2 m (radioamatori)"},
    {StringId::PresetPmr446, "PMR446 walkie-talkies", "Stații PMR446"},
    {StringId::PresetAdsb, "Aircraft (ADS-B)", "Avioane (ADS-B)"},
    {StringId::PresetMeteor, "Meteor-M weather satellites", "Sateliți meteo Meteor-M"},
    {StringId::FrequencyHelp,
     "Scroll over a digit, or click it and use ↑ ↓, to change that digit. Type digits to enter a "
     "frequency.",
     "Derulează peste o cifră sau dă clic pe ea și folosește ↑ ↓ ca s-o schimbi. Tastează cifre "
     "ca să introduci o frecvență."},
    {StringId::FrequencyFixedHelp, "A recording stays on the frequency it was made on.",
     "O înregistrare rămâne pe frecvența la care a fost făcută."},
    // Device panel
    {StringId::ReceiversTitle, "Receivers", "Receptoare"},
    {StringId::Searching, "Looking for RTL-SDR sticks…", "Se caută stickuri RTL-SDR…"},
    {StringId::NoReceiverFound, "No RTL-SDR stick found. Plug one in, then search again.",
     "Niciun stick RTL-SDR găsit. Conectează unul, apoi caută din nou."},
    {StringId::SearchAgain, "Search again", "Caută din nou"},
    {StringId::Open, "Open", "Deschide"},
    {StringId::SerialNumber, "Serial {}", "Serie {}"},
    {StringId::CloseReceiver, "Close receiver", "Închide receptorul"},
    {StringId::CloseRecording, "Close recording", "Închide înregistrarea"},
    {StringId::TunerLabel, "Tuner", "Tuner"},
    {StringId::IdentityConsistent, "The tuner matches the model the stick reports.",
     "Tunerul corespunde modelului raportat de stick."},
    {StringId::IdentityMismatch,
     "The stick reports itself as an {}, but its tuner isn't the one that model uses. It may be "
     "a counterfeit.",
     "Stickul se declară {}, dar tunerul nu este cel folosit de acest model. Poate fi o "
     "contrafacere."},
    {StringId::IdentityUnverifiable, "A generic stick: it reports no model to check.",
     "Stick generic: nu raportează un model de verificat."},
    {StringId::SampleRateLabel, "Sample rate", "Rată de eșantionare"},
    {StringId::PpmLabel, "Frequency correction (ppm)", "Corecție de frecvență (ppm)"},
    {StringId::BiasTeeLabel, "Bias-tee (antenna power)", "Bias-tee (alimentarea antenei)"},
    {StringId::BiasTeeWarning,
     "The bias-tee sends 4.5 V up the antenna cable to power an LNA or an active antenna. With a "
     "plain antenna, or one that shorts DC, it can damage the stick or the antenna.",
     "Bias-tee trimite 4,5 V pe cablul antenei, ca să alimenteze un LNA sau o antenă activă. Cu o "
     "antenă simplă sau una care scurtcircuitează curentul continuu poate strica stickul sau "
     "antena."},
    {StringId::BiasTeeTurnOn, "Turn on bias-tee", "Pornește bias-tee"},
    {StringId::Cancel, "Cancel", "Renunță"},
    {StringId::RecordingFromFile, "Recording · {} · {}", "Înregistrare · {} · {}"},
    // Banners
    {StringId::RecordingBanner, "Recording {} — {} · {}", "Înregistrare {} — {} · {}"},
    {StringId::RecordingEndedRetune, "Recording ended because the frequency changed. Saved {}.",
     "Înregistrarea s-a oprit pentru că s-a schimbat frecvența. Salvată: {}."},
    {StringId::RecordingFailed, "Recording failed: {}", "Înregistrarea a eșuat: {}"},
    {StringId::ReceiverFailed, "Couldn't open the receiver: {}",
     "Receptorul nu a putut fi deschis: {}"},
    {StringId::Dismiss, "Dismiss", "Închide"},
    // Spectrum
    {StringId::TabSpectrum, "Spectrum", "Spectru"},
    {StringId::FftInfo, "FFT {} · {}/bin", "FFT {} · {}/bin"},
    {StringId::FftSizeLabel, "FFT size", "Mărime FFT"},
    {StringId::SmoothingLabel, "Spectrum smoothing", "Netezirea spectrului"},
    {StringId::WaterfallFloorLabel, "Waterfall floor", "Pragul de jos al waterfall-ului"},
    {StringId::WaterfallCeilingLabel, "Waterfall ceiling", "Pragul de sus al waterfall-ului"},
    {StringId::FitToSignal, "Fit to signal", "Potrivește după semnal"},
    {StringId::TuneHint, "Click to listen here, scroll to step",
     "Clic ca să asculți aici, derulează ca să pășești"},
    // Aircraft
    {StringId::TabAircraft, "Aircraft", "Avioane"},
    {StringId::ColCallsign, "Callsign", "Indicativ"},
    {StringId::ColAltitude, "Altitude", "Altitudine"},
    {StringId::ColSpeed, "Speed", "Viteză"},
    {StringId::ColDistance, "Distance", "Distanță"},
    {StringId::ColSeen, "Seen", "Văzut"},
    {StringId::AdsbHint,
     "Aircraft appear when the receiver listens to 1090 MHz at 2 MS/s. The Aircraft (ADS-B) preset "
     "sets both.",
     "Avioanele apar când receptorul ascultă 1090 MHz la 2 MS/s. Presetarea Avioane (ADS-B) le "
     "setează pe amândouă."},
    {StringId::TuneAdsb, "Tune to ADS-B", "Acordează pe ADS-B"},
    {StringId::AdsbListening,
     "Listening on 1090 MHz. Aircraft show up here as soon as one is heard; an outdoor antenna "
     "helps a lot.",
     "Se ascultă pe 1090 MHz. Avioanele apar aici imediat ce unul e recepționat; o antenă afară "
     "ajută mult."},
    {StringId::NeedStation,
     "Set your station's location in Settings to place aircraft on the radar.",
     "Setează locația stației în Setări ca să apară avioanele pe radar."},
    {StringId::StationLabel, "Station location", "Locația stației"},
    {StringId::LatitudeLabel, "Latitude (° north)", "Latitudine (° nord)"},
    {StringId::LongitudeLabel, "Longitude (° east)", "Longitudine (° est)"},
    {StringId::UseLocation, "Use this location", "Folosește această locație"},
    {StringId::CompassNorth, "N", "N"},
    {StringId::CompassEast, "E", "E"},
    {StringId::CompassSouth, "S", "S"},
    {StringId::CompassWest, "W", "V"},
    // Satellites
    {StringId::TabSatellites, "Satellites", "Sateliți"},
    {StringId::NeedStationSatellites,
     "Set your station's location in Settings to see when the satellites pass over you.",
     "Setează locația stației în Setări ca să vezi când trec sateliții deasupra ta."},
    {StringId::WaitingForTle, "Getting the satellites' orbits (TLE) from CelesTrak…",
     "Se descarcă orbitele sateliților (TLE) de la CelesTrak…"},
    {StringId::TleError, "Couldn't update the orbits: {}",
     "Orbitele nu au putut fi actualizate: {}"},
    {StringId::TleAge, "Orbits from CelesTrak, {} old", "Orbite de la CelesTrak, vechi de {}"},
    {StringId::NextPass, "NEXT PASS", "URMĂTORUL PASAJ"},
    {StringId::PassInProgress, "Overhead, {} left", "Deasupra, mai are {}"},
    {StringId::MaxElevationShort, "Max el", "El. max"},
    {StringId::NoPasses, "No passes above {}° in the next 48 hours.",
     "Niciun pasaj peste {}° în următoarele 48 de ore."},
    {StringId::ColSatellite, "Satellite", "Satelit"},
    {StringId::ColStart, "Starts", "Începe"},
    {StringId::ColDuration, "Duration", "Durată"},
    {StringId::ColDirection, "Direction", "Direcție"},
    {StringId::Northbound, "northbound", "spre nord"},
    {StringId::Southbound, "southbound", "spre sud"},
    {StringId::MinElevationLabel, "Minimum pass elevation", "Elevația minimă a pasajului"},
    {StringId::AltitudeLabel, "Altitude (m)", "Altitudine (m)"},
    {StringId::NotifyTitle, "{} passes in 5 minutes", "{} trece în 5 minute"},
    {StringId::NotifyBody, "Up to {}°, rising at {} in the {}.",
     "Până la {}°, răsare la {} spre {}."},
    // Automatic pass recording (M5)
    {StringId::AutoRecordLabel, "Record and decode passes automatically",
     "Înregistrează și decodează automat pasajele"},
    {StringId::AutoRecordHelp, "At each pass the receiver tunes to the satellite, records until it sets, then SatDump turns the recording into images.",
     "La fiecare pasaj receptorul se acordează pe satelit, înregistrează până apune, apoi SatDump transformă înregistrarea în imagini."},
    {StringId::AutoRecordAwake, "Keep the Mac awake and the receiver connected: a sleeping Mac misses passes.",
     "Ține Mac-ul treaz și receptorul conectat: un Mac în repaus ratează pasajele."},
    {StringId::SatDumpReady, "SatDump is installed",
     "SatDump e instalat"},
    {StringId::SatDumpReadyVersion, "SatDump {} is installed",
     "SatDump {} e instalat"},
    {StringId::SatDumpMissing, "SatDump isn't installed, so recordings can't be turned into images. Install it in Terminal, then check again:",
     "SatDump nu e instalat, deci înregistrările nu pot fi transformate în imagini. Instalează-l din Terminal, apoi verifică din nou:"},
    {StringId::SatDumpInstallCommand, "brew install --cask satdump",
     "brew install --cask satdump"},
    {StringId::SatDumpLooking, "Looking for SatDump…",
     "Caut SatDump…"},
    {StringId::CheckAgain, "Check again",
     "Verifică din nou"},
    {StringId::WillRecord, "Recorded automatically",
     "Se înregistrează automat"},
    {StringId::PassPreparing, "Tuning to {} for {}…",
     "Acord pe {} pentru {}…"},
    {StringId::PassRecording, "Recording the {} pass — {} left · {}",
     "Înregistrez pasajul {} — {} rămase · {}"},
    {StringId::PassSaving, "Saving the {} recording…",
     "Salvez înregistrarea {}…"},
    {StringId::PassDecoding, "Decoding {} with SatDump…",
     "Decodez {} cu SatDump…"},
    {StringId::PassDecodingPercent, "Decoding {} with SatDump — {} %",
     "Decodez {} cu SatDump — {} %"},
    {StringId::PassImagesSaved, "{}: images saved ({})",
     "{}: imagini salvate ({})"},
    {StringId::ShowInFinder, "Show in Finder",
     "Arată în Finder"},
    {StringId::ShowLog, "Show log",
     "Arată logul"},
    {StringId::PassFailStopped, "{}: stopped before the recording began",
     "{}: oprit înainte să înceapă înregistrarea"},
    {StringId::PassFailEnded, "{}: the pass ended before the receiver was ready",
     "{}: pasajul s-a încheiat înainte ca receptorul să fie gata"},
    {StringId::PassFailOffFrequency, "{}: the recording being played isn't on the satellite's frequency",
     "{}: înregistrarea redată nu e pe frecvența satelitului"},
    {StringId::PassFailReceiver, "{}: the receiver couldn't be used ({})",
     "{}: receptorul nu a putut fi folosit ({})"},
    {StringId::PassFailTune, "{}: the receiver didn't tune in time",
     "{}: receptorul nu s-a acordat la timp"},
    {StringId::PassFailRecorder, "{}: the recording failed ({})",
     "{}: înregistrarea a eșuat ({})"},
    {StringId::PassFailTooShort, "{}: too little was recorded to decode",
     "{}: s-a înregistrat prea puțin pentru decodare"},
    {StringId::PassFailNoSatDump, "{}: recorded, but SatDump isn't installed to decode it",
     "{}: înregistrat, dar SatDump nu e instalat pentru decodare"},
    {StringId::PassFailSatDump, "{}: SatDump failed ({})",
     "{}: SatDump a eșuat ({})"},
    {StringId::PassFailNoImage, "{}: SatDump found no image; the signal was probably too weak",
     "{}: SatDump nu a găsit nicio imagine; semnalul a fost probabil prea slab"},
    {StringId::ControlsLocked, "A pass is being recorded: tuning is back when it ends.",
     "Se înregistrează un pasaj: acordul revine când se termină."},
    // Status bar
    {StringId::StatusVersion, "Zarescope", "Zarescope"},
    {StringId::StatusUtc, "UTC", "UTC"},
    {StringId::StatusLocal, "Local", "Local"},
    {StringId::StatusNoReceiver, "No receiver", "Niciun receptor"},
    {StringId::StatusReplaying, "Replaying", "Redare"},
    {StringId::StatusDropped, "Dropped {} blocks", "{} blocuri pierdute"},
}};

constexpr bool rowsMatchIds() {
    for (size_t i = 0; i < kStrings.size(); ++i) {
        if (static_cast<size_t>(kStrings[i].id) != i || kStrings[i].english.empty() ||
            kStrings[i].romanian.empty()) {
            return false;
        }
    }
    return true;
}
static_assert(rowsMatchIds(), "kStrings needs one row per StringId, in enum order, both languages");

}  // namespace

std::string_view text(StringId id, Language language) {
    const Entry& entry = kStrings[static_cast<size_t>(id)];
    return language == Language::Romanian ? entry.romanian : entry.english;
}

}  // namespace zarescope::ui
