/**
 * Generate application icons from the source PNG.
 * Produces:
 *   - assets/icon.png       (1024x1024 master PNG)
 *   - assets/icon.ico        (Windows multi-size ICO: 16,32,48,64,128,256)
 *   - assets/icon.icns        (macOS — built from individual PNGs via iconutil if on Mac,
 *                              otherwise we create the 512x512 PNG for electron-builder)
 *   - assets/favicon.png     (32x32 for HTML)
 *   - assets/icon-256.png    (256x256 for Linux / electron-builder)
 *
 * Usage: node scripts/generate-icons.js
 */

const sharp = require('sharp');
const fs = require('fs');
const path = require('path');

const SOURCE = path.join(__dirname, '..', 'assets', 'PDF Chisel Logo.png');
const ASSETS = path.join(__dirname, '..', 'assets');

const ICO_SIZES = [16, 32, 48, 64, 128, 256];
const FAVICON_SIZE = 32;
const MASTER_SIZE = 1024;
const LINUX_SIZE = 512;

/**
 * Build an ICO file from an array of { size, buf } PNG entries.
 * ICO format: 6-byte header, 16-byte directory entry per image, then raw PNG data.
 */
function buildIco(entries) {
  const HEADER_SIZE = 6;
  const DIR_ENTRY_SIZE = 16;
  const headerBuf = Buffer.alloc(HEADER_SIZE);
  headerBuf.writeUInt16LE(0, 0);              // reserved
  headerBuf.writeUInt16LE(1, 2);              // type = ICO
  headerBuf.writeUInt16LE(entries.length, 4); // image count

  let dataOffset = HEADER_SIZE + DIR_ENTRY_SIZE * entries.length;
  const dirEntries = [];
  const imageBuffers = [];

  for (const { size, buf } of entries) {
    const dir = Buffer.alloc(DIR_ENTRY_SIZE);
    dir.writeUInt8(size >= 256 ? 0 : size, 0);   // width  (0 = 256)
    dir.writeUInt8(size >= 256 ? 0 : size, 1);   // height (0 = 256)
    dir.writeUInt8(0, 2);                          // color palette
    dir.writeUInt8(0, 3);                          // reserved
    dir.writeUInt16LE(1, 4);                       // color planes
    dir.writeUInt16LE(32, 6);                      // bits per pixel
    dir.writeUInt32LE(buf.length, 8);              // image data size
    dir.writeUInt32LE(dataOffset, 12);             // offset to image data
    dirEntries.push(dir);
    imageBuffers.push(buf);
    dataOffset += buf.length;
  }

  return Buffer.concat([headerBuf, ...dirEntries, ...imageBuffers]);
}

async function main() {
  console.log('Source:', SOURCE);

  // 1. Master 1024x1024 PNG (electron-builder uses this for macOS)
  console.log('Generating master 1024x1024 icon.png ...');
  await sharp(SOURCE)
    .resize(MASTER_SIZE, MASTER_SIZE, { fit: 'contain', background: { r: 0, g: 0, b: 0, alpha: 0 } })
    .png()
    .toFile(path.join(ASSETS, 'icon.png'));

  // 2. Generate individual PNGs for ICO
  const icoPngBuffers = [];
  for (const size of ICO_SIZES) {
    const buf = await sharp(SOURCE)
      .resize(size, size, { fit: 'contain', background: { r: 0, g: 0, b: 0, alpha: 0 } })
      .png()
      .toBuffer();
    icoPngBuffers.push({ size, buf });
    console.log(`  Generated ${size}x${size} PNG for ICO`);
  }

  // 3. Build ICO (manually — ICO is a simple container of PNGs)
  console.log('Building icon.ico ...');
  const icoBuffer = buildIco(icoPngBuffers);
  fs.writeFileSync(path.join(ASSETS, 'icon.ico'), icoBuffer);

  // 4. Favicon (32x32 PNG)
  console.log('Generating favicon.png (32x32) ...');
  await sharp(SOURCE)
    .resize(FAVICON_SIZE, FAVICON_SIZE, { fit: 'contain', background: { r: 0, g: 0, b: 0, alpha: 0 } })
    .png()
    .toFile(path.join(ASSETS, 'favicon.png'));

  // 5. Linux icon (512x512)
  console.log('Generating 512x512 for Linux ...');
  await sharp(SOURCE)
    .resize(LINUX_SIZE, LINUX_SIZE, { fit: 'contain', background: { r: 0, g: 0, b: 0, alpha: 0 } })
    .png()
    .toFile(path.join(ASSETS, 'icon-512.png'));

  console.log('Done! Icons written to assets/');
}

main().catch(err => { console.error(err); process.exit(1); });
