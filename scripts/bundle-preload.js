/**
 * Bundle the preload script with esbuild.
 *
 * Electron's sandboxed preload cannot use Node `require()` for local files.
 * This script inlines all local dependencies (../shared/ipc-schema, etc.)
 * into a single dist/preload/index.js file while keeping `electron` external.
 */

const { buildSync } = require('esbuild');
const path = require('path');

buildSync({
  entryPoints: [path.join(__dirname, '..', 'dist', 'preload', 'index.js')],
  bundle: true,
  platform: 'node',
  target: 'node20',
  format: 'cjs',
  outfile: path.join(__dirname, '..', 'dist', 'preload', 'index.js'),
  allowOverwrite: true,
  external: ['electron'],
  sourcemap: true,
});

console.log('[bundle-preload] Preload script bundled successfully.');
