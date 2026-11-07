// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "LaserFrame.h"

#include <string>

namespace IldaReader
{
/** Load an ILDA (.ild) file (formats 0/1/2/4/5). On failure, show.error is set and frames empty. */
LaserShow LoadFile(const std::string& path);
}
