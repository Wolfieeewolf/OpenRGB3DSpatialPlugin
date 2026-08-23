// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPATIALCONTROLLERENTRYKEY_H
#define SPATIALCONTROLLERENTRYKEY_H

#include <QMetaType>
#include <QPair>

// type_code: RGB controller index (>=0), -1 custom virtual, -2 ref point, -3 display plane
using SpatialControllerEntryKey = QPair<int, int>;

Q_DECLARE_METATYPE(SpatialControllerEntryKey)

#endif
