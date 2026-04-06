// SPDX-License-Identifier: GPL-2.0-only

#ifndef MINECRAFTEFFECTLIBRARY_H
#define MINECRAFTEFFECTLIBRARY_H

#include <QString>
#include <set>
#include <string>
#include <vector>

namespace MinecraftEffectLibrary
{

inline const char* LibraryHubClassId()
{
    return "_MinecraftLibraryHub_";
}

inline const std::set<std::string>& CollapsedClassNames()
{
    static const std::set<std::string> s = {
        "MinecraftGame",
        "MinecraftHealth",
        "MinecraftHunger",
        "MinecraftAir",
        "MinecraftDurability",
        "MinecraftDamage",
        "MinecraftWorldTint",
        "MinecraftLightning",
    };
    return s;
}

struct Variant
{
    const char* class_name;
    const char* label;
};

inline const std::vector<Variant>& Variants()
{
    static const std::vector<Variant> v = {
        {"MinecraftAir", "Air"},
        {"MinecraftDamage", "Damage flash"},
        {"MinecraftDurability", "Durability"},
        {"MinecraftHealth", "Health"},
        {"MinecraftHunger", "Hunger"},
        {"MinecraftLightning", "Lightning"},
        {"MinecraftWorldTint", "World tint"},
        {"MinecraftGame", "All layers (bundled)"},
    };
    return v;
}

inline bool IsCollapsedClass(const std::string& cn)
{
    return CollapsedClassNames().find(cn) != CollapsedClassNames().end();
}

inline bool SearchMatchesFamily(const QString& search)
{
    if(search.isEmpty())
    {
        return true;
    }
    if(search.contains(QStringLiteral("minecraft"), Qt::CaseInsensitive))
    {
        return true;
    }
    if(search.contains(QStringLiteral("fabric"), Qt::CaseInsensitive))
    {
        return true;
    }
    for(const Variant& var : Variants())
    {
        if(QString::fromUtf8(var.label).contains(search, Qt::CaseInsensitive))
        {
            return true;
        }
    }
    if(QStringLiteral("Minecraft (Fabric)").contains(search, Qt::CaseInsensitive))
    {
        return true;
    }
    return false;
}

} // namespace MinecraftEffectLibrary

#endif
