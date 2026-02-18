#!/usr/bin/env node
/**
 * Download prebuilt PDFium shared library from bblanchon/pdfium-binaries.
 * Extracts to native/pdfium/vendor/{include,lib,bin}.
 *
 * Usage: node scripts/download-pdfium.js
 */

'use strict';

const https = require('node:https');
const { execSync } = require('node:child_process');
const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');

const RELEASE_BASE_URL =
  'https://github.com/bblanchon/pdfium-binaries/releases/latest/download';

/** Map Node.js platform+arch → bblanchon tarball name (without .tgz). */
const PLATFORM_MAP = {
  'win32-x64': 'pdfium-win-x64',
  'darwin-x64': 'pdfium-mac-x64',
  'darwin-arm64': 'pdfium-mac-arm64',
  'linux-x64': 'pdfium-linux-x64',
  'linux-arm64': 'pdfium-linux-arm64',
};

const MAX_REDIRECTS = 10;
const VENDOR_DIR = path.resolve(__dirname, '..', 'native', 'pdfium', 'vendor');

/**
 * Follow-redirect HTTPS download to a file.
 * @param {string} url
 * @param {string} destPath
 * @returns {Promise<void>}
 */
function download(url, destPath) {
  return new Promise((resolve, reject) => {
    let redirects = 0;

    const follow = (currentUrl) => {
      https
        .get(currentUrl, (res) => {
          if (
            (res.statusCode === 301 || res.statusCode === 302) &&
            res.headers.location
          ) {
            redirects++;
            if (redirects > MAX_REDIRECTS) {
              reject(new Error('Too many redirects'));
              return;
            }
            follow(res.headers.location);
            return;
          }

          if (res.statusCode !== 200) {
            reject(
              new Error(`HTTP ${res.statusCode} downloading ${currentUrl}`),
            );
            return;
          }

          const total = parseInt(res.headers['content-length'] || '0', 10);
          let received = 0;
          const stream = fs.createWriteStream(destPath);

          res.on('data', (chunk) => {
            received += chunk.length;
            if (total > 0) {
              const pct = ((received / total) * 100).toFixed(1);
              process.stdout.write(`\r  ${pct}% (${received} / ${total})`);
            }
          });

          res.pipe(stream);
          stream.on('finish', () => {
            stream.close();
            if (total > 0) process.stdout.write('\n');
            resolve();
          });
          stream.on('error', reject);
        })
        .on('error', reject);
    };

    follow(url);
  });
}

async function main() {
  const key = `${process.platform}-${process.arch}`;
  const name = PLATFORM_MAP[key];

  if (!name) {
    console.error(`Unsupported platform: ${key}`);
    console.error('Supported:', Object.keys(PLATFORM_MAP).join(', '));
    process.exit(1);
  }

  const tarball = `${name}.tgz`;
  const url = `${RELEASE_BASE_URL}/${tarball}`;
  const tmpFile = path.join(os.tmpdir(), tarball);

  console.log(`Platform : ${key}`);
  console.log(`Tarball  : ${tarball}`);
  console.log(`URL      : ${url}`);
  console.log();

  // ── Download ────────────────────────────────────────────────────
  console.log('Downloading…');
  await download(url, tmpFile);
  console.log(`Saved to ${tmpFile}`);

  // ── Clean vendor dir ────────────────────────────────────────────
  if (fs.existsSync(VENDOR_DIR)) {
    fs.rmSync(VENDOR_DIR, { recursive: true, force: true });
  }
  fs.mkdirSync(VENDOR_DIR, { recursive: true });

  // ── Extract ─────────────────────────────────────────────────────
  console.log(`Extracting to ${VENDOR_DIR}…`);
  execSync(`tar -xzf "${tmpFile}" -C "${VENDOR_DIR}" --strip-components=1`, {
    stdio: 'inherit',
  });

  // ── Cleanup ─────────────────────────────────────────────────────
  fs.unlinkSync(tmpFile);

  // ── Verify ──────────────────────────────────────────────────────
  // bblanchon tarballs use a flat structure: headers + libs at root level
  const headerFile = path.join(VENDOR_DIR, 'fpdfview.h');

  if (!fs.existsSync(headerFile)) {
    console.error('ERROR: fpdfview.h not found after extraction.');
    console.error(
      'The tarball structure may have changed. Check contents and adjust --strip-components.',
    );
    process.exit(1);
  }

  console.log();
  console.log('PDFium binaries installed successfully.');
  console.log(`  Vendor dir: ${VENDOR_DIR}`);
  console.log();
  console.log('Next step: npm run build:native');
}

main().catch((err) => {
  console.error('Failed to download PDFium:', err.message);
  process.exit(1);
});
