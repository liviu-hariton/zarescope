// SPDX-License-Identifier: GPL-3.0-or-later
//
// zarescope.eu: a static site, built on the developer's Mac and copied to the
// server with tools/deploy-site.sh. English at /, Romanian at /ro/.
import { defineConfig } from "astro/config";
import sitemap from "@astrojs/sitemap";

export default defineConfig({
  site: "https://zarescope.eu",
  trailingSlash: "always",
  build: { format: "directory", inlineStylesheets: "never" },
  integrations: [
    sitemap({
      i18n: { defaultLocale: "en", locales: { en: "en", ro: "ro" } },
      filter: (page) => !/\/404\/$/.test(page),
    }),
  ],
  vite: {
    // The guide and the version live in the app's repo, one level up.
    server: { fs: { allow: [".."] } },
  },
});
