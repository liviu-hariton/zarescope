// SPDX-License-Identifier: GPL-3.0-or-later
//
// /latest.json: the newest version, for the app's update check
// (src/app/UpdateCheck.h). Only the version number: the app opens its own
// download page, whose address is fixed in the app, so nothing here can send
// anyone elsewhere. Deploy the site only after the release exists
// (docs/RELEASE.md), or the app would announce a version nobody can download.
import { version } from "../data/site";

export function GET() {
  return new Response(JSON.stringify({ version }) + "\n", {
    headers: { "Content-Type": "application/json; charset=utf-8" },
  });
}
