/**
 * document.h — Document lifecycle function declarations.
 */
#ifndef PDFIUM_ADDON_DOCUMENT_H
#define PDFIUM_ADDON_DOCUMENT_H

#include <napi.h>

/** openDocument(data: Buffer, password?: string): number */
Napi::Value OpenDocument(const Napi::CallbackInfo& info);

/** closeDocument(handle: number): void */
void CloseDocument(const Napi::CallbackInfo& info);

/** getPageCount(handle: number): number */
Napi::Value GetPageCount(const Napi::CallbackInfo& info);

/** saveDocument(handle: number): Buffer */
Napi::Value SaveDocument(const Napi::CallbackInfo& info);

#endif // PDFIUM_ADDON_DOCUMENT_H
