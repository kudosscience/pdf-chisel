/**
 * objects.cc — Page object listing, text editing, image replacement.
 */

#include "common.h"
#include "objects.h"

#include <fpdfview.h>
#include <fpdf_edit.h>

#include <cstring>
#include <string>

// ── Object type name mapping ────────────────────────────────────────

static const char* GetObjectTypeName(int type) {
  switch (type) {
    case FPDF_PAGEOBJ_TEXT:    return "text";
    case FPDF_PAGEOBJ_PATH:    return "path";
    case FPDF_PAGEOBJ_IMAGE:   return "image";
    case FPDF_PAGEOBJ_SHADING: return "shading";
    case FPDF_PAGEOBJ_FORM:    return "form";
    default:                   return "unknown";
  }
}

// ── listPageObjects ─────────────────────────────────────────────────

Napi::Value ListPageObjects(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 2 ||
      !info[0].IsNumber() ||
      !info[1].IsNumber()) {
    Napi::TypeError::New(env,
      "listPageObjects: requires (handle: number, pageIndex: number)"
    ).ThrowAsJavaScriptException();
    return env.Undefined();
  }

  int handle    = info[0].As<Napi::Number>().Int32Value();
  int pageIndex = info[1].As<Napi::Number>().Int32Value();

  FPDF_DOCUMENT doc = RequireDocument(env, handle);
  if (!doc) return env.Undefined();

  FPDF_PAGE page = FPDF_LoadPage(doc, pageIndex);
  if (!page) {
    Napi::Error::New(env,
      "listPageObjects: failed to load page " + std::to_string(pageIndex)
    ).ThrowAsJavaScriptException();
    return env.Undefined();
  }

  int objCount = FPDFPage_CountObjects(page);
  Napi::Array result = Napi::Array::New(env, static_cast<size_t>(objCount));

  for (int i = 0; i < objCount; i++) {
    FPDF_PAGEOBJECT obj = FPDFPage_GetObject(page, i);
    int type = FPDFPageObj_GetType(obj);

    float left = 0.0f, bottom = 0.0f, right = 0.0f, top = 0.0f;
    FPDFPageObj_GetBounds(obj, &left, &bottom, &right, &top);

    Napi::Object entry = Napi::Object::New(env);
    entry.Set("id",     Napi::Number::New(env, i));
    entry.Set("type",   Napi::String::New(env, GetObjectTypeName(type)));
    entry.Set("left",   Napi::Number::New(env, static_cast<double>(left)));
    entry.Set("top",    Napi::Number::New(env, static_cast<double>(top)));
    entry.Set("right",  Napi::Number::New(env, static_cast<double>(right)));
    entry.Set("bottom", Napi::Number::New(env, static_cast<double>(bottom)));

    result[static_cast<uint32_t>(i)] = entry;
  }

  FPDF_ClosePage(page);
  return result;
}

// ── editTextObject ──────────────────────────────────────────────────

void EditTextObject(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // editTextObject(handle, pageIndex, objectId, newText [, fontName, fontSize])
  if (info.Length() < 4 ||
      !info[0].IsNumber() ||
      !info[1].IsNumber() ||
      !info[2].IsNumber() ||
      !info[3].IsString()) {
    Napi::TypeError::New(env,
      "editTextObject: requires (handle, pageIndex, objectId, newText)"
    ).ThrowAsJavaScriptException();
    return;
  }

  int handle      = info[0].As<Napi::Number>().Int32Value();
  int pageIndex   = info[1].As<Napi::Number>().Int32Value();
  int objectId    = info[2].As<Napi::Number>().Int32Value();

  // Get text as UTF-16LE for PDFium's FPDF_WIDESTRING
  std::u16string newText = info[3].As<Napi::String>().Utf16Value();

  FPDF_DOCUMENT doc = RequireDocument(env, handle);
  if (!doc) return;

  // Load the page
  FPDF_PAGE page = FPDF_LoadPage(doc, pageIndex);
  if (!page) {
    Napi::Error::New(env,
      "editTextObject: failed to load page " + std::to_string(pageIndex)
    ).ThrowAsJavaScriptException();
    return;
  }

  // Validate object ID
  int objCount = FPDFPage_CountObjects(page);
  if (objectId < 0 || objectId >= objCount) {
    FPDF_ClosePage(page);
    Napi::RangeError::New(env,
      "editTextObject: objectId " + std::to_string(objectId) +
      " out of range [0, " + std::to_string(objCount - 1) + "]"
    ).ThrowAsJavaScriptException();
    return;
  }

  FPDF_PAGEOBJECT obj = FPDFPage_GetObject(page, objectId);

  // Ensure it is a text object
  if (FPDFPageObj_GetType(obj) != FPDF_PAGEOBJ_TEXT) {
    FPDF_ClosePage(page);
    Napi::TypeError::New(env,
      "editTextObject: object " + std::to_string(objectId) + " is not a text object"
    ).ThrowAsJavaScriptException();
    return;
  }

  // Set text content (FPDF_WIDESTRING = const unsigned short* or const wchar_t*)
  FPDF_BOOL ok = FPDFText_SetText(
    obj,
    reinterpret_cast<FPDF_WIDESTRING>(newText.c_str())
  );

  if (!ok) {
    FPDF_ClosePage(page);
    Napi::Error::New(env, "editTextObject: FPDFText_SetText failed")
      .ThrowAsJavaScriptException();
    return;
  }

  // Regenerate the page content stream to persist the edit
  if (!FPDFPage_GenerateContent(page)) {
    FPDF_ClosePage(page);
    Napi::Error::New(env,
      "editTextObject: FPDFPage_GenerateContent failed"
    ).ThrowAsJavaScriptException();
    return;
  }

  FPDF_ClosePage(page);
}

// ── replaceImageObject ──────────────────────────────────────────────

/**
 * FPDF_FILEACCESS wrapper that reads from an in-memory buffer.
 * Used to hand JPEG data to FPDFImageObj_LoadJpegFileInline.
 */
struct BufferFileAccess {
  FPDF_FILEACCESS access;
  const uint8_t* data;
  unsigned long   size;
};

static int BufferReadBlock(
  void* param,
  unsigned long position,
  unsigned char* pBuf,
  unsigned long size
) {
  auto* bfa = static_cast<BufferFileAccess*>(param);
  if (position + size > bfa->size) return 0;
  std::memcpy(pBuf, bfa->data + position, size);
  return 1;
}

void ReplaceImageObject(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // replaceImageObject(handle, pageIndex, objectId, imageData, format)
  if (info.Length() < 5 ||
      !info[0].IsNumber() ||
      !info[1].IsNumber() ||
      !info[2].IsNumber() ||
      !info[3].IsBuffer() ||
      !info[4].IsString()) {
    Napi::TypeError::New(env,
      "replaceImageObject: requires "
      "(handle, pageIndex, objectId, imageData: Buffer, format: string)"
    ).ThrowAsJavaScriptException();
    return;
  }

  int handle      = info[0].As<Napi::Number>().Int32Value();
  int pageIndex   = info[1].As<Napi::Number>().Int32Value();
  int objectId    = info[2].As<Napi::Number>().Int32Value();
  auto imageData  = info[3].As<Napi::Buffer<uint8_t>>();
  std::string fmt = info[4].As<Napi::String>().Utf8Value();

  FPDF_DOCUMENT doc = RequireDocument(env, handle);
  if (!doc) return;

  FPDF_PAGE page = FPDF_LoadPage(doc, pageIndex);
  if (!page) {
    Napi::Error::New(env,
      "replaceImageObject: failed to load page " + std::to_string(pageIndex)
    ).ThrowAsJavaScriptException();
    return;
  }

  int objCount = FPDFPage_CountObjects(page);
  if (objectId < 0 || objectId >= objCount) {
    FPDF_ClosePage(page);
    Napi::RangeError::New(env,
      "replaceImageObject: objectId " + std::to_string(objectId) +
      " out of range [0, " + std::to_string(objCount - 1) + "]"
    ).ThrowAsJavaScriptException();
    return;
  }

  FPDF_PAGEOBJECT obj = FPDFPage_GetObject(page, objectId);

  if (FPDFPageObj_GetType(obj) != FPDF_PAGEOBJ_IMAGE) {
    FPDF_ClosePage(page);
    Napi::TypeError::New(env,
      "replaceImageObject: object " + std::to_string(objectId) +
      " is not an image object"
    ).ThrowAsJavaScriptException();
    return;
  }

  FPDF_BOOL ok = 0;

  if (fmt == "jpeg") {
    // Embed JPEG data directly via FPDFImageObj_LoadJpegFileInline
    BufferFileAccess bfa;
    bfa.access.m_FileLen  = static_cast<unsigned long>(imageData.Length());
    bfa.access.m_GetBlock = BufferReadBlock;
    bfa.access.m_Param    = &bfa;
    bfa.data              = imageData.Data();
    bfa.size              = static_cast<unsigned long>(imageData.Length());

    ok = FPDFImageObj_LoadJpegFileInline(
      &page, /*count=*/1, obj, &bfa.access
    );
  } else {
    // PNG and other formats require decoding to raw pixels.
    // This is a known limitation of the current implementation.
    FPDF_ClosePage(page);
    Napi::Error::New(env,
      "replaceImageObject: only 'jpeg' format is currently supported. "
      "Convert other formats to JPEG before calling this function."
    ).ThrowAsJavaScriptException();
    return;
  }

  if (!ok) {
    FPDF_ClosePage(page);
    Napi::Error::New(env,
      "replaceImageObject: failed to load replacement image"
    ).ThrowAsJavaScriptException();
    return;
  }

  // Regenerate page content stream
  if (!FPDFPage_GenerateContent(page)) {
    FPDF_ClosePage(page);
    Napi::Error::New(env,
      "replaceImageObject: FPDFPage_GenerateContent failed"
    ).ThrowAsJavaScriptException();
    return;
  }

  FPDF_ClosePage(page);
}
