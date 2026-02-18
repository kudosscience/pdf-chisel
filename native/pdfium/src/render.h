/**
 * render.h — Page rendering function declarations.
 */
#ifndef PDFIUM_ADDON_RENDER_H
#define PDFIUM_ADDON_RENDER_H

#include <napi.h>

/**
 * renderPage(handle, pageIndex, scale)
 * → { data: Buffer (RGBA), width: number, height: number }
 */
Napi::Value RenderPage(const Napi::CallbackInfo& info);

#endif // PDFIUM_ADDON_RENDER_H
