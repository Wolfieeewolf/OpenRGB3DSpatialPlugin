// SPDX-License-Identifier: GPL-2.0-only

#ifndef OPENRGBPLUGINSFONT_H
#define OPENRGBPLUGINSFONT_H

#include <QFont>
#include <QString>

class OpenRGBPluginsFont
{
public:
    enum Glyph
    {
        math_minus = 0xE072,
        math_plus  = 0xE074,
    };

    static QString icon(int glyph);
    static QFont   GetFont();

private:
    OpenRGBPluginsFont();

    static OpenRGBPluginsFont* instance;
    int                        fontId;
    QFont                      font;
};

#endif
