// SPDX-License-Identifier: GPL-3.0-or-later
//
// The user guide as Markdown (/guide.md, /ro/ghid.md), for anyone (or any AI
// agent) who'd rather read the source: the app's resources/help files as they are.
import { readFileSync } from "node:fs";
import { repoRoot } from "./site";

export function guideMarkdown(lang: "en" | "ro"): Response {
  const text = readFileSync(`${repoRoot}resources/help/guide.${lang}.md`, "utf8");
  return new Response(text, { headers: { "Content-Type": "text/markdown; charset=utf-8" } });
}
