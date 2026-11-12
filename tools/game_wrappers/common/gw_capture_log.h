// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <cstdint>
#include <cstddef>

/**
 * Session capture log — default is FULL dump (every grid cell + API line).
 *
 * Env OPENRGB_GW_CAPTURE:
 *   0 / off      — disable
 *   1 / full     — everything incl. grid_raw COLORREF dumps (default when unset)
 *   sampled      — ~10 Hz per device + averages only (if logs get huge)
 */
namespace GwCapture
{

void BeginSession();
void EndSession();

void LogApi(const char* api, const char* detail);

/** Always logged — effect types we could not expand into a grid. */
void LogUnhandled(const char* api, int effect, const char* detail);

/** Hex dump of effect params (unknown / hardware effects). */
void LogParamDump(const char* api, int effect, const void* data, std::size_t bytes);

void LogStatic(const char* device, int effect, std::uint32_t color);
void LogStaticDual(const char* device, int effect, std::uint32_t c1, std::uint32_t c2);
void LogGrid(const char* device,
             int effect,
             const std::uint32_t* colors,
             int rows,
             int cols);

void NoteCreate(const char* device, int effect, bool stored);
void NoteSetEffect(const char* device, bool found);
void NoteDeleteEffect();

const char* SessionPath();

} // namespace GwCapture
