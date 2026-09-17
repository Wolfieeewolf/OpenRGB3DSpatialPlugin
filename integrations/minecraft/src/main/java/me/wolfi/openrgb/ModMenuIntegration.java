package me.wolfi.openrgb;

import com.terraformersmc.modmenu.api.ConfigScreenFactory;
import com.terraformersmc.modmenu.api.ModMenuApi;

/**
 * Registers the in-game config screen. Shown in Mod Menu and Mod Settings (F6).
 */
public class ModMenuIntegration implements ModMenuApi
{
    @Override
    public ConfigScreenFactory<?> getModConfigScreenFactory()
    {
        return OpenRGBConfigScreen::new;
    }
}
