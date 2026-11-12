// SPDX-License-Identifier: GPL-3.0-or-later
// /guide.md: the user guide as Markdown.
import { guideMarkdown } from "../data/guideMarkdown";

export const GET = () => guideMarkdown("en");
