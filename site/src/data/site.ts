// SPDX-License-Identifier: GPL-3.0-or-later
//
// Facts the whole site shares. The version is read from the app's CMakeLists.txt
// at build time, so the download button always names the version just released.
import { readFileSync } from "node:fs";
import { resolve } from "node:path";

/** The app's repository root. Builds run from site/ (npm run build), one level below it. */
export const repoRoot = resolve(process.cwd(), "..") + "/";

function appVersion(): string {
  const cmake = readFileSync(repoRoot + "CMakeLists.txt", "utf8");
  const m = cmake.match(/project\(Zarescope\s+VERSION\s+([0-9.]+)/);
  if (!m) throw new Error("Can't find the version in CMakeLists.txt");
  return m[1];
}

export const version = appVersion();
export const repoUrl = "https://github.com/liviu-hariton/zarescope";
/** The disk image of this version, attached to its GitHub release (tools/publish-release.sh). */
export const dmgName = `Zarescope-${version}-arm64.dmg`;
export const dmgUrl = `${repoUrl}/releases/download/v${version}/${dmgName}`;
export const releasesUrl = `${repoUrl}/releases`;
export const issuesUrl = `${repoUrl}/issues`;
/** The project's mailbox, shown in the footer of every page. */
export const contactEmail = "hello@zarescope.eu";
/** Where "Buy me a beer" leads (a plain link: nothing is loaded from there). */
export const beerUrl = "https://www.buymeacoffee.com/liviuhariton";
/** The RTL-SDR Blog's own page listing where to buy its sticks. */
export const stickUrl = "https://www.rtl-sdr.com/buy-rtl-sdr-dvb-t-dongles/";
export const firstPublished = 2026;
export const copyrightYears = (() => {
  const now = new Date().getFullYear();
  return now > firstPublished ? `${firstPublished}–${now}` : String(firstPublished);
})();

/** Zarescope as schema.org data (JSON-LD), for the home and download pages. */
export function softwareJsonLd(lang: "en" | "ro", pageUrl: string, description: string) {
  return {
    "@context": "https://schema.org",
    "@type": "SoftwareApplication",
    name: "Zarescope",
    url: pageUrl,
    description,
    applicationCategory: "UtilitiesApplication",
    operatingSystem: "macOS 14 or later (Apple Silicon)",
    softwareVersion: version,
    downloadUrl: dmgUrl,
    isAccessibleForFree: true,
    offers: { "@type": "Offer", price: "0", priceCurrency: "EUR" },
    license: "https://www.gnu.org/licenses/gpl-3.0.html",
    inLanguage: ["en", "ro"],
    image: `https://zarescope.eu/og-${lang}.png`,
    author: { "@type": "Person", name: "Liviu Hariton" },
    sameAs: [repoUrl],
  };
}
