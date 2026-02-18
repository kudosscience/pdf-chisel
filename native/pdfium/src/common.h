/**
 * common.h — Shared state and utilities for the PDFium N-API addon.
 */
#ifndef PDFIUM_ADDON_COMMON_H
#define PDFIUM_ADDON_COMMON_H

#include <napi.h>
#include <fpdfview.h>
#include <map>

// ── Global document registry ────────────────────────────────────────

/** Maps integer handle → FPDF_DOCUMENT. */
extern std::map<int, FPDF_DOCUMENT> g_documents;

/** Monotonically increasing handle counter. */
extern int g_nextHandle;

/** Whether FPDF_InitLibraryWithConfig has been called. */
extern bool g_initialized;

// ── Utility functions ───────────────────────────────────────────────

/**
 * Ensure the PDFium library is initialised.
 * Safe to call multiple times; only the first call has an effect.
 */
void EnsurePdfiumInit();

/**
 * Look up a document handle in the registry.
 * Throws a JS Error and returns nullptr if not found.
 */
FPDF_DOCUMENT RequireDocument(Napi::Env env, int handle);

#endif // PDFIUM_ADDON_COMMON_H
