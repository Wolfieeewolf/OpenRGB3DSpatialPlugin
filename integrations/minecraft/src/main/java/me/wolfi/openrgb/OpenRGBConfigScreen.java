package me.wolfi.openrgb;

import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.client.gui.components.Button;
import net.minecraft.client.gui.components.CycleButton;
import net.minecraft.client.gui.components.EditBox;
import net.minecraft.client.gui.components.MultiLineTextWidget;
import net.minecraft.client.gui.components.StringWidget;
import net.minecraft.client.gui.components.Tooltip;
import net.minecraft.network.chat.Component;

/**
 * Minimal sender controls + OpenRGB link status.
 * Room Ambilight quality (UV, face size, sky) is tuned in OpenRGB, not here.
 */
public class OpenRGBConfigScreen extends Screen
{
    private final Screen parent;
    private final OpenRGBSenderConfig config;
    private CycleButton<Boolean> enabledButton;
    private CycleButton<Boolean> roomSampleButton;
    private EditBox tickDivisorField;
    private MultiLineTextWidget statusWidget;

    public OpenRGBConfigScreen(Screen parent)
    {
        super(Component.literal("OpenRGB Sender"));
        this.parent = parent;
        this.config = OpenRGBSenderConfig.get();
    }

    @Override
    protected void init()
    {
        final int bw = 240;
        final int cx = width / 2 - bw / 2;
        int y = 28;

        StringWidget title = new StringWidget(cx, y, bw, 12, this.title, font);
        addRenderableWidget(title);
        y += 20;

        enabledButton = CycleButton.onOffBuilder(config.enabled)
                .create(cx, y, bw, 20, Component.literal("Send telemetry"), (btn, val) -> config.enabled = val);
        addRenderableWidget(enabledButton);
        y += 26;

        roomSampleButton = CycleButton.onOffBuilder(config.sendRoomSampleFrames)
                .create(cx, y, bw, 20, Component.literal("Room Ambilight"), (btn, val) -> config.sendRoomSampleFrames = val);
        addRenderableWidget(roomSampleButton);
        y += 26;

        tickDivisorField = new EditBox(font, cx, y, bw, 20, Component.literal("Telemetry every N ticks"));
        tickDivisorField.setMaxLength(2);
        tickDivisorField.setValue(Integer.toString(config.telemetryTickDivisor));
        tickDivisorField.setHint(Component.literal("1 = every tick"));
        tickDivisorField.setTooltip(Tooltip.create(
                Component.literal("Higher = less UDP traffic for vitals. Room Ambilight still runs every tick when enabled.")));
        addRenderableWidget(tickDivisorField);
        y += 28;

        statusWidget = new MultiLineTextWidget(cx, y, Component.literal(buildStatusText()), font)
                .setMaxWidth(bw)
                .setMaxRows(5);
        addRenderableWidget(statusWidget);
        y += Math.max(48, statusWidget.getHeight()) + 8;

        addRenderableWidget(Button.builder(Component.literal("Refresh status"), btn -> refreshStatus())
                .bounds(cx, y, bw, 20)
                .build());
        y += 28;

        addRenderableWidget(Button.builder(Component.literal("Done"), btn -> closeAndSave())
                .bounds(cx, y, bw, 20)
                .build());
        addRenderableWidget(Button.builder(Component.literal("Cancel"), btn -> onClose())
                .bounds(cx, y + 24, bw, 20)
                .build());
    }

    private void refreshStatus()
    {
        if(statusWidget != null)
        {
            statusWidget.setMessage(Component.literal(buildStatusText()));
        }
    }

    private static String buildStatusText()
    {
        final OpenRGBSenderMod.LinkStatus st = OpenRGBSenderMod.getLinkStatus();
        if(st == null || !st.linked)
        {
            return "OpenRGB: waiting for Room Ambilight config\n"
                    + "Select Room Ambilight in OpenRGB, then join a world.\n"
                    + "Host/port: config/openrgb-minecraft-sender.json";
        }
        return String.format(java.util.Locale.US,
                "OpenRGB: linked (config #%d)\nCubemap %dx%d  ·  UV %d  ·  %d LED texels\nSky / weather: %s",
                st.configId, st.faceSize, st.faceSize, st.uvDim, st.ledTexels,
                st.skyEnabled ? "on" : "off");
    }

    private void closeAndSave()
    {
        config.telemetryTickDivisor = parseInt(tickDivisorField.getValue(), config.telemetryTickDivisor);
        config.save();
        onClose();
    }

    private static int parseInt(String text, int fallback)
    {
        try
        {
            return Integer.parseInt(text.trim());
        }
        catch(NumberFormatException ignored)
        {
            return fallback;
        }
    }

    @Override
    public void onClose()
    {
        Minecraft.getInstance().gui.setScreen(parent);
    }
}
