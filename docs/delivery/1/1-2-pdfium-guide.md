# PDFium N-API Addon — External Package Guide

**Date**: 2026-02-18
**Task**: 1-2 Native PDFium addon
**Packages**: PDFium (C library via bblanchon/pdfium-binaries), node-addon-api (N-API C++ wrapper)

## References

- PDFium source: https://pdfium.googlesource.com/pdfium/
- Prebuilt binaries: https://github.com/bblanchon/pdfium-binaries
- API docs: https://developers.foxit.com/resources/pdf-sdk/c_api_reference_pdfium/
- node-addon-api: https://github.com/nodejs/node-addon-api

## PDFium C API — Key Functions

### Library Lifecycle

```c
#include "fpdfview.h"

// Initialise (call once at process startup)
FPDF_LIBRARY_CONFIG config;
config.version = 2;
config.m_pUserFontPaths = NULL;
config.m_pIsolate = NULL;
config.m_v8EmbedderSlot = 0;
FPDF_InitLibraryWithConfig(&config);

// Destroy (call once at process shutdown)
FPDF_DestroyLibrary();
```

### Document Lifecycle

```c
#include "fpdfview.h"

// Open from memory buffer
FPDF_DOCUMENT doc = FPDF_LoadMemDocument(data, size, password);
// Returns NULL on failure; use FPDF_GetLastError() for error code

// Error codes:
// FPDF_ERR_SUCCESS  (0) — no error
// FPDF_ERR_UNKNOWN  (1) — unknown error
// FPDF_ERR_FILE     (2) — file not found
// FPDF_ERR_FORMAT   (3) — not a PDF or corrupted
// FPDF_ERR_PASSWORD (4) — password required or incorrect
// FPDF_ERR_SECURITY (5) — unsupported security scheme
// FPDF_ERR_PAGE     (6) — page not found or content error

int pageCount = FPDF_GetPageCount(doc);

FPDF_CloseDocument(doc);
```

### Page Rendering

```c
#include "fpdfview.h"

FPDF_PAGE page = FPDF_LoadPage(doc, pageIndex); // 0-based
double width  = FPDF_GetPageWidthF(page);   // in points (1pt = 1/72 inch)
double height = FPDF_GetPageHeightF(page);

int pixelW = (int)(width * scale);
int pixelH = (int)(height * scale);

// Create BGRA bitmap with alpha
FPDF_BITMAP bmp = FPDFBitmap_Create(pixelW, pixelH, /*alpha=*/1);
FPDFBitmap_FillRect(bmp, 0, 0, pixelW, pixelH, 0xFFFFFFFF); // white bg

int flags = FPDF_ANNOT | FPDF_PRINTING | FPDF_LCD_TEXT;
FPDF_RenderPageBitmap(bmp, page, 0, 0, pixelW, pixelH, /*rotation=*/0, flags);

void* pixels = FPDFBitmap_GetBuffer(bmp);  // BGRA byte array
int   stride = FPDFBitmap_GetStride(bmp);  // bytes per row (may include padding)

// Copy/convert pixels as needed …

FPDFBitmap_Destroy(bmp);
FPDF_ClosePage(page);
```

**Bitmap format**: `FPDFBitmap_Create(..., 1)` produces BGRA (Blue-Green-Red-Alpha), 4 bytes per pixel. To get RGBA for HTML Canvas `ImageData`, swap the B and R channels.

### Page Object Listing

```c
#include "fpdf_edit.h"

FPDF_PAGE page = FPDF_LoadPage(doc, pageIndex);
int count = FPDFPage_CountObjects(page);

for (int i = 0; i < count; i++) {
    FPDF_PAGEOBJECT obj = FPDFPage_GetObject(page, i);
    int type = FPDFPageObj_GetType(obj);
    // FPDF_PAGEOBJ_TEXT    = 1
    // FPDF_PAGEOBJ_PATH    = 2
    // FPDF_PAGEOBJ_IMAGE   = 3
    // FPDF_PAGEOBJ_SHADING = 4
    // FPDF_PAGEOBJ_FORM    = 5

    float left, bottom, right, top;
    FPDFPageObj_GetBounds(obj, &left, &bottom, &right, &top);
    // Coordinates in PDF points, origin = bottom-left
}

FPDF_ClosePage(page);
```

### Text Editing

```c
#include "fpdf_edit.h"

FPDF_PAGEOBJECT textObj = FPDFPage_GetObject(page, objectIndex);
// Verify: FPDFPageObj_GetType(textObj) == FPDF_PAGEOBJ_TEXT

// Set new text (UTF-16LE encoded, null-terminated)
FPDF_WIDESTRING wideText = ...; // unsigned short* on Linux/macOS, wchar_t* on Windows
FPDFText_SetText(textObj, wideText);

// MUST regenerate the page content stream after editing
FPDFPage_GenerateContent(page);
```

**Note**: `FPDFText_SetText` preserves the existing font. Changing font requires creating a new text object.

### Image Replacement

```c
#include "fpdf_edit.h"

// For JPEG data — embed directly:
FPDF_FILEACCESS fileAccess;
fileAccess.m_FileLen = jpegDataSize;
fileAccess.m_GetBlock = MyReadCallback;  // reads from buffer
fileAccess.m_pParam = myBufferPtr;

FPDFImageObj_LoadJpegFileInline(&page, 1, imageObj, &fileAccess);
FPDFPage_GenerateContent(page);
```

**Limitation**: PDFium has no built-in PNG→bitmap decoder. For PNG replacement, the image must be decoded to raw BGRA pixels externally, then set via `FPDFImageObj_SetBitmap`. The MVP supports JPEG replacement only.

### Document Saving

```c
#include "fpdf_save.h"

// Callback-based writer
typedef struct {
    FPDF_FILEWRITE fileWrite;
    std::vector<uint8_t> buffer;
} BufferWriter;

int WriteBlock(FPDF_FILEWRITE* pThis, const void* data, unsigned long size) {
    auto* w = (BufferWriter*)pThis;
    auto* bytes = (const uint8_t*)data;
    w->buffer.insert(w->buffer.end(), bytes, bytes + size);
    return 1;
}

BufferWriter writer;
writer.fileWrite.version = 1;
writer.fileWrite.WriteBlock = WriteBlock;
FPDF_SaveAsCopy(doc, &writer.fileWrite, 0);
// writer.buffer now contains the serialised PDF bytes
```

## node-addon-api (N-API) — Key Patterns

### Module Structure

```cpp
#include <napi.h>

Napi::Value MyFunction(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    // info[0], info[1] etc. are JS arguments
    // Return Napi::Number, Napi::String, Napi::Buffer, Napi::Object, etc.
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set("myFunction", Napi::Function::New(env, MyFunction));
    return exports;
}

NODE_API_MODULE(addon_name, Init)
```

### Common Types

```cpp
// Buffer (binary data)
auto buf = info[0].As<Napi::Buffer<uint8_t>>();
uint8_t* data = buf.Data();
size_t len = buf.Length();

// Create Buffer copy for return
auto result = Napi::Buffer<uint8_t>::Copy(env, srcData, srcLen);

// String
std::string s = info[0].As<Napi::String>().Utf8Value();
std::u16string ws = info[0].As<Napi::String>().Utf16Value(); // UTF-16LE

// Number
int n = info[0].As<Napi::Number>().Int32Value();
double d = info[0].As<Napi::Number>().DoubleValue();

// Object
Napi::Object obj = Napi::Object::New(env);
obj.Set("key", Napi::Number::New(env, 42));

// Array
Napi::Array arr = Napi::Array::New(env, count);
arr[i] = obj;
```

### Error Handling

```cpp
// Throw JS exception (returns to JS immediately)
Napi::Error::New(env, "message").ThrowAsJavaScriptException();
return env.Undefined();

// Type-specific errors
Napi::TypeError::New(env, "Expected Buffer").ThrowAsJavaScriptException();
Napi::RangeError::New(env, "Index out of bounds").ThrowAsJavaScriptException();
```

### binding.gyp

```json
{
  "targets": [{
    "target_name": "myaddon",
    "sources": ["src/addon.cc"],
    "include_dirs": [
      "<!@(node -p \"require('node-addon-api').include\")"
    ],
    "dependencies": [
      "<!(node -p \"require('node-addon-api').gyp\")"
    ],
    "cflags!": ["-fno-exceptions"],
    "cflags_cc!": ["-fno-exceptions"]
  }]
}
```

## Prebuilt PDFium Binaries (bblanchon/pdfium-binaries)

- **Release URL pattern**: `https://github.com/bblanchon/pdfium-binaries/releases/latest/download/pdfium-{os}-{arch}.tgz`
- **Desktop targets**:
  - `pdfium-win-x64.tgz`
  - `pdfium-mac-x64.tgz`
  - `pdfium-mac-arm64.tgz`
  - `pdfium-linux-x64.tgz`
- **Archive contents** (after extraction, flat structure — no subdirectories):
  - `fpdfview.h`, `fpdf_edit.h`, `fpdf_text.h`, `fpdf_save.h`, etc. — C headers
  - `pdfium.dll.lib` (Windows import lib), `libpdfium.dylib` (macOS), `libpdfium.so` (Linux)
  - `pdfium.dll` (Windows runtime DLL)
  - `cpp/` — C++ helper headers (`fpdf_scopers.h`, `fpdf_deleters.h`)
  - `*.txt` — Third-party license files
- **Linking**: Shared library approach; the `.node` addon links dynamically against the PDFium shared library. Both must be distributed together.
- **License**: Apache License 2.0 (same as Chromium/PDFium)
