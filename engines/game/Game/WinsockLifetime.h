// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Reference-counted WSAStartup for GameTelemetryBridge + GameLightingBridge. */
namespace WinsockLifetime
{
bool Acquire();
void Release();
} // namespace WinsockLifetime
