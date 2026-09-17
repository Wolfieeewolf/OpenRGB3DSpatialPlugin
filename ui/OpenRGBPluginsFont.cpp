// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGBPluginsFont.h"
#include <QFontDatabase>
#include <QStringList>

OpenRGBPluginsFont* OpenRGBPluginsFont::instance = nullptr;

OpenRGBPluginsFont::OpenRGBPluginsFont()
    : fontId(-1)
{
}

QString OpenRGBPluginsFont::icon(int glyph)
{
    return QChar(glyph);
}

QFont OpenRGBPluginsFont::GetFont()
{
    if(!instance)
    {
        instance        = new OpenRGBPluginsFont();
        instance->fontId = QFontDatabase::addApplicationFont(":/OpenRGBPlugins.ttf");
        if(instance->fontId != -1)
        {
            const QString family = QFontDatabase::applicationFontFamilies(instance->fontId).at(0);
            instance->font       = QFont(family, 13);
            instance->font.setStyleStrategy(QFont::PreferAntialias);
        }
    }
    return instance->font;
}
