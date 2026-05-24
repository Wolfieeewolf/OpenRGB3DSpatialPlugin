# SPDX-License-Identifier: GPL-2.0-only
"""Split OpenRGB3DSpatialTab_Layout.cpp into profile + custom-controller TUs."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "ui"
LAYOUT = ROOT / "OpenRGB3DSpatialTab_Layout.cpp"
CUSTOM = ROOT / "OpenRGB3DSpatialTab_LayoutCustomControllers.cpp"

CUSTOM_HEADER = """// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialTab.h"
#include "ControllerDisplayUtils.h"
#include "SpatialTabLedHelpers.h"
#include "PluginSettingsPaths.h"
#include "SpatialControllerCardList.h"
#include "GridSpaceUtils.h"
#include "ControllerLayout3D.h"
#include "VirtualController3D.h"
#include "LogManager.h"
#include "CustomControllerDialog.h"
#include "SettingsManager.h"
#include "PluginUiUtils.h"
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace filesystem = std::filesystem;

"""

def main() -> None:
    lines = LAYOUT.read_text(encoding="utf-8").splitlines()
    # 1-based inclusive ranges -> 0-based slices
    # Line numbers from grep on full Layout.cpp (1-based inclusive).
    custom_body = lines[160:725] + lines[1644:2100]
    keep_body = lines[0:160] + lines[725:1644] + lines[2100:]

    CUSTOM.write_text(CUSTOM_HEADER + "\n".join(custom_body) + "\n", encoding="utf-8", newline="\n")
    LAYOUT.write_text("\n".join(keep_body) + "\n", encoding="utf-8", newline="\n")
    print(f"wrote {CUSTOM.name}: {len(custom_body)} lines")
    print(f"wrote {LAYOUT.name}: {len(keep_body)} lines")


if __name__ == "__main__":
    main()
