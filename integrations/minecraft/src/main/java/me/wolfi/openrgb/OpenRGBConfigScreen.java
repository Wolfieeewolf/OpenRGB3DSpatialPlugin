// SPDX-License-Identifier: GPL-2.0-or-later

package me.wolfi.openrgb;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiGraphicsExtractor;
import net.minecraft.client.gui.components.AbstractSliderButton;
import net.minecraft.client.gui.components.AbstractWidget;
import net.minecraft.client.gui.components.Button;
import net.minecraft.client.gui.components.CycleButton;
import net.minecraft.client.gui.components.Tooltip;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.network.chat.Component;

/**
 * Mod Menu screen. Status: live lights, what OpenRGB asked for, how Room Ambilight is doing,
 * the captured game picture with each LED's spot on it, and the colours going to the LEDs.
 * Settings: the switches that belong to the mod (the look is set in OpenRGB, shown read-only).
 */
public class OpenRGBConfigScreen extends Screen
{
    private static final int GREEN = 0xFF3FBF5F;
    private static final int GREY = 0xFF7A7A7A;
    private static final int TEXT = 0xFFFFFFFF;
    private static final int MUTED = 0xFFA8A8A8;
    private static final int PANEL = 0xA0101010;

    private final Screen parent;
    private final OpenRGBSenderConfig config;
    private boolean statusTab = true;
    private final List<AbstractWidget> settingsWidgets = new ArrayList<>();
    private Button statusButton;
    private Button settingsButton;

    /* Frames per second from two readings a second apart. */
    private long rateFrames;
    private long rateAtMs;
    private int framesPerSecond;

    public OpenRGBConfigScreen(Screen parent)
    {
        super(Component.literal("OpenRGB 3D Spatial — Minecraft"));
        this.parent = parent;
        this.config = OpenRGBSenderConfig.get();
    }

    private int contentWidth()
    {
        return Math.min(420, width - 20);
    }

    private int left()
    {
        return width / 2 - contentWidth() / 2;
    }

    @Override
    protected void init()
    {
        settingsWidgets.clear();
        final int w = contentWidth();
        final int x = left();
        final int tabW = (w - 4) / 2;
        statusButton = addRenderableWidget(Button.builder(Component.literal("Status"), b -> showTab(true))
                .bounds(x, 22, tabW, 20).build());
        settingsButton = addRenderableWidget(Button.builder(Component.literal("Settings"), b -> showTab(false))
                .bounds(x + tabW + 4, 22, tabW, 20).build());

        int y = 56;
        final CycleButton<Boolean> telemetry = CycleButton.onOffBuilder(config.enabled)
                .create(x, y, w, 20, Component.literal("Send to OpenRGB"), (btn, val) -> config.enabled = val);
        telemetry.setTooltip(Tooltip.create(Component.literal(
                "Everything: vitals, damage and Room Ambilight. Off = the mod does nothing.")));
        settingsWidgets.add(addRenderableWidget(telemetry));
        y += 24;
        final CycleButton<Boolean> room = CycleButton.onOffBuilder(config.sendRoomSampleFrames)
                .create(x, y, w, 20, Component.literal("Room Ambilight"), (btn, val) -> config.sendRoomSampleFrames = val);
        room.setTooltip(Tooltip.create(Component.literal(
                "Room colours from the world around you. Off = vitals and damage only.")));
        settingsWidgets.add(addRenderableWidget(room));
        y += 24;
        settingsWidgets.add(addRenderableWidget(new IntSlider(x, y, w, "Vitals every", 1, 20, config.telemetryTickDivisor,
                " tick(s)", v -> config.telemetryTickDivisor = v,
                "How often health, hunger, air and pose are sent (20 ticks = 1 second). Higher = less traffic.")));
        y += 24;
        settingsWidgets.add(addRenderableWidget(new IntSlider(x, y, w, "Room rays every", 1, 8,
                config.roomSampleSendInterval, " tick(s)", v -> config.roomSampleSendInterval = v,
                "How often LEDs beside and behind you are traced. LEDs on the game picture update every frame "
                        + "regardless. Higher = lighter on slow PCs.")));

        final int by = height - 28;
        addRenderableWidget(Button.builder(Component.literal("Done"), b -> closeAndSave())
                .bounds(x, by, (w - 4) / 2, 20).build());
        addRenderableWidget(Button.builder(Component.literal("Cancel"), b -> onClose())
                .bounds(x + (w - 4) / 2 + 4, by, (w - 4) / 2, 20).build());
        showTab(statusTab);
    }

    private void showTab(boolean status)
    {
        statusTab = status;
        for(AbstractWidget wdg : settingsWidgets)
        {
            wdg.visible = !status;
            wdg.active = !status;
        }
        if(statusButton != null)
        {
            statusButton.active = !status;
            settingsButton.active = status;
        }
    }

    @Override
    public void extractRenderState(final GuiGraphicsExtractor g, final int mouseX, final int mouseY, final float a)
    {
        super.extractRenderState(g, mouseX, mouseY, a);
        g.centeredText(font, title, width / 2, 8, TEXT);
        if(statusTab)
        {
            drawStatus(g);
        }
        else
        {
            final int x = left();
            final int y = 56 + 24 * 4 + 6;
            g.text(font, "Facing, picture, resolution, face size and texture quality", x, y, MUTED);
            g.text(font, "are set in OpenRGB: Minecraft effect → Room Ambilight.", x, y + 11, MUTED);
        }
    }

    private void drawStatus(GuiGraphicsExtractor g)
    {
        final int x = left();
        final int w = contentWidth();
        int y = 52;
        final OpenRGBSenderMod.LinkStatus link = OpenRGBSenderMod.getLinkStatus();
        final OpenRGBSenderMod.LiveStats s = OpenRGBSenderMod.liveStats();

        final long now = System.currentTimeMillis();
        if(rateAtMs == 0 || now - rateAtMs >= 1000)
        {
            if(rateAtMs != 0)
            {
                framesPerSecond = (int)Math.round((s.framesSent - rateFrames) * 1000.0 / (now - rateAtMs));
            }
            rateAtMs = now;
            rateFrames = s.framesSent;
        }
        final boolean sending = config.enabled && config.sendRoomSampleFrames && framesPerSecond > 0;
        final boolean pictureLive = s.pictureOn && s.picture != null;

        /* Lights */
        int lx = x;
        lx = light(g, lx, y, link.linked, "OpenRGB link");
        lx = light(g, lx + 14, y, sending, "Room Ambilight");
        light(g, lx + 14, y, pictureLive, s.pictureOn && !s.firstPerson ? "Game picture (first person only)" : "Game picture");
        y += 16;

        /* What OpenRGB asked for */
        if(link.linked)
        {
            String asked = String.format(Locale.US, "From OpenRGB: %s · face %d · texture %d",
                    s.pictureOn ? "picture + rays" : "rays only", link.faceSize, link.uvDim);
            if(s.pictureOn)
            {
                asked += String.format(Locale.US, " · picture %d wide · blend %d%%", s.pictureWidthSetting,
                        Math.round(s.edge * 100));
            }
            if(Math.abs(s.headingDeg) > 0.05f)
            {
                asked += String.format(Locale.US, " · facing %.0f°", s.headingDeg);
            }
            g.text(font, asked, x, y, MUTED);
        }
        else
        {
            g.text(font, "Waiting for OpenRGB: add Room Ambilight to the effect stack and start it.", x, y, MUTED);
        }
        y += 12;

        /* How it is doing */
        g.text(font, String.format(Locale.US, "Sending %d frames/s · rays %.1f ms per tick", framesPerSecond,
                s.raysMsPerTick), x, y, TEXT);
        y += 11;
        if(s.leds > 0)
        {
            g.text(font, String.format(Locale.US, "%d LEDs: %d from the picture, %d blended, %d from rays", s.leds,
                    s.fromPicture, s.blended, s.fromRays), x, y, TEXT);
        }
        y += 16;

        /* Picture with each LED's spot on it */
        final int thumbW = Math.min(w, 240);
        final int thumbH = thumbW * 9 / 16;
        g.fill(x - 2, y - 2, x + thumbW + 2, y + thumbH + 2, PANEL);
        if(pictureLive)
        {
            drawPicture(g, s.picture, x, y, thumbW, thumbH);
            for(int i = 0; i < s.leds; i++)
            {
                if(s.ledU[i] < 0.0f)
                {
                    continue;
                }
                final int px = x + Math.round(s.ledU[i] * thumbW);
                final int py = y + Math.round((1.0f - s.ledV[i]) * thumbH);
                final int col = ledColour(s.ledRgba, i);
                g.fill(px - 2, py - 2, px + 3, py + 3, 0xFF000000);
                g.fill(px - 1, py - 1, px + 2, py + 2, col);
            }
            g.outline(x, y, thumbW, thumbH, 0x60FFFFFF);
        }
        else
        {
            g.text(font, s.pictureOn ? "No picture right now (paused, menu, third person)"
                                     : "Game picture is off (rays only)", x + 6, y + thumbH / 2 - 4, MUTED);
        }

        /* LED colours, as sent */
        final int sx = x + thumbW + 10;
        final int sw = x + w - sx;
        if(sw >= 40 && s.leds > 0)
        {
            g.text(font, "LEDs", sx, y, MUTED);
            final int cell = 6;
            final int perRow = Math.max(1, (sw + 1) / (cell + 1));
            final int maxRows = Math.max(1, (thumbH - 12) / (cell + 1));
            for(int i = 0; i < s.leds && i / perRow < maxRows; i++)
            {
                final int cx = sx + (i % perRow) * (cell + 1);
                final int cy = y + 12 + (i / perRow) * (cell + 1);
                g.fill(cx, cy, cx + cell, cy + cell, ledColour(s.ledRgba, i));
            }
        }
    }

    private int light(GuiGraphicsExtractor g, int x, int y, boolean on, String label)
    {
        g.fill(x, y + 1, x + 7, y + 8, on ? GREEN : GREY);
        g.text(font, label, x + 11, y, on ? TEXT : MUTED);
        return x + 11 + font.width(label);
    }

    /** The small captured picture, in cells (row 0 of the picture is its bottom). */
    private static void drawPicture(GuiGraphicsExtractor g, ScreenFrameCapture.Frame f, int x, int y, int w, int h)
    {
        final int cell = 3;
        for(int cy = 0; cy < h; cy += cell)
        {
            final int row = Math.min(f.height - 1, Math.max(0, f.height - 1 - (cy + cell / 2) * f.height / h));
            for(int cx = 0; cx < w; cx += cell)
            {
                final int col = Math.min(f.width - 1, (cx + cell / 2) * f.width / w);
                final int o = (row * f.width + col) * 4;
                final int argb = 0xFF000000 | ((f.rgba[o] & 0xFF) << 16) | ((f.rgba[o + 1] & 0xFF) << 8) | (f.rgba[o + 2] & 0xFF);
                g.fill(x + cx, y + cy, Math.min(x + w, x + cx + cell), Math.min(y + h, y + cy + cell), argb);
            }
        }
    }

    private static int ledColour(byte[] rgba, int i)
    {
        final int o = i * 4;
        if(o + 3 >= rgba.length)
        {
            return 0xFF000000;
        }
        /* Shown as the LED gets it: colour scaled by its alpha (coverage). */
        final int a = rgba[o + 3] & 0xFF;
        final int r = (rgba[o] & 0xFF) * a / 255;
        final int gg = (rgba[o + 1] & 0xFF) * a / 255;
        final int b = (rgba[o + 2] & 0xFF) * a / 255;
        return 0xFF000000 | (r << 16) | (gg << 8) | b;
    }

    private void closeAndSave()
    {
        config.save();
        onClose();
    }

    @Override
    public void onClose()
    {
        Minecraft.getInstance().gui.setScreen(parent);
    }

    /** Whole-number slider with a label and unit. */
    private static final class IntSlider extends AbstractSliderButton
    {
        private final String label;
        private final int min;
        private final int max;
        private final String unit;
        private final java.util.function.IntConsumer apply;

        IntSlider(int x, int y, int w, String label, int min, int max, int value, String unit,
                  java.util.function.IntConsumer apply, String tooltip)
        {
            super(x, y, w, 20, Component.empty(), (Math.max(min, Math.min(max, value)) - min) / (double)(max - min));
            this.label = label;
            this.min = min;
            this.max = max;
            this.unit = unit;
            this.apply = apply;
            setTooltip(Tooltip.create(Component.literal(tooltip)));
            updateMessage();
        }

        private int current()
        {
            return min + (int)Math.round(value * (max - min));
        }

        @Override
        protected void updateMessage()
        {
            setMessage(Component.literal(label + ": " + current() + unit));
        }

        @Override
        protected void applyValue()
        {
            apply.accept(current());
        }
    }
}
