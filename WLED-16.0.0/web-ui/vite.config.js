import { readFileSync } from 'node:fs';
import { defineConfig } from 'vite';
import { svelte } from '@sveltejs/vite-plugin-svelte';
import { viteSingleFile } from 'vite-plugin-singlefile';

const rootPackage = JSON.parse(readFileSync(new URL('../package.json', import.meta.url), 'utf8'));

// Single-file output: CSS + JS inlined into one index.html so the ESP32 only
// has to serve a single gzipped blob. The bundle-to-header.js post-build
// step converts dist/index.html into wled00/web_ui_bundle.h.
export default defineConfig({
  plugins: [svelte(), viteSingleFile()],
  define: {
    __LOOMX_FIRMWARE_VERSION__: JSON.stringify(`v${rootPackage.version}`),
  },
  build: {
    target: 'es2019',
    cssCodeSplit: false,
    assetsInlineLimit: 100000000,
    chunkSizeWarningLimit: 100000000,
    rollupOptions: {
      output: { inlineDynamicImports: true },
    },
  },
});
