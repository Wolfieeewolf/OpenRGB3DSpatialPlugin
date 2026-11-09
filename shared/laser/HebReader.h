// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "LaserFrame.h"

#include <string>

namespace HebReader
{
/**
 * Load a Laserworld ShowEditor / HE-Laserscan figure (.heb) into LaserShow frames.
 * Reverse-engineered: 9-byte points (x,y int16 LE, colour u16, aux u16, status u8).
 * On failure, show.error is set and frames empty.
 */
LaserShow LoadFile(const std::string& path);
}
