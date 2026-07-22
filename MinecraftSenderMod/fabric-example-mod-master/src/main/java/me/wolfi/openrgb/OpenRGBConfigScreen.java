package me.wolfi.openrgb;

import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.client.gui.components.Button;
import net.minecraft.client.gui.components.CycleButton;
import net.minecraft.client.gui.components.EditBox;
import net.minecraft.network.chat.Component;

public class OpenRGBConfigScreen extends Screen
{
    private final Screen parent;
    private final OpenRGBSenderConfig config;
    private CycleButton<Boolean> enabledButton;
    private CycleButton<Boolean> roomSampleButton;
    private EditBox hostField;
    private EditBox portField;
    private EditBox blocksField;
    private EditBox tickDivisorField;

    public OpenRGBConfigScreen(Screen parent)
    {
        super(Component.literal("OpenRGB Minecraft Sender"));
        this.parent = parent;
        this.config = OpenRGBSenderConfig.get();
    }

    @Override
    protected void init()
    {
        int bw = 220;
        int cx = width / 2 - bw / 2;
        int y = height / 6;

        enabledButton = CycleButton.onOffBuilder(config.enabled)
                .create(cx, y, bw, 20, Component.literal("Send telemetry"), (btn, val) -> config.enabled = val);
        addRenderableWidget(enabledButton);
        y += 28;

        hostField = new EditBox(font, cx, y, bw, 20, Component.literal("Host"));
        hostField.setMaxLength(128);
        hostField.setValue(config.host);
        hostField.setHint(Component.literal("127.0.0.1"));
        addRenderableWidget(hostField);
        y += 28;

        portField = new EditBox(font, cx, y, bw, 20, Component.literal("Port"));
        portField.setMaxLength(5);
        portField.setValue(Integer.toString(config.port));
        portField.setHint(Component.literal("9876"));
        addRenderableWidget(portField);
        y += 28;

        blocksField = new EditBox(font, cx, y, bw, 20, Component.literal("Blocks per meter (0.25-16)"));
        blocksField.setMaxLength(8);
        blocksField.setValue(String.format(java.util.Locale.US, "%.2f", config.blocksPerMeter));
        blocksField.setHint(Component.literal("4.00"));
        addRenderableWidget(blocksField);
        y += 28;

        roomSampleButton = CycleButton.onOffBuilder(config.sendRoomSampleFrames)
                .create(cx, y, bw, 20, Component.literal("Room Ambilight cubemap (mapped LEDs)"), (btn, val) -> config.sendRoomSampleFrames = val);
        addRenderableWidget(roomSampleButton);
        y += 28;

        tickDivisorField = new EditBox(font, cx, y, bw, 20, Component.literal("Telemetry tick divisor"));
        tickDivisorField.setMaxLength(2);
        tickDivisorField.setValue(Integer.toString(config.telemetryTickDivisor));
        tickDivisorField.setHint(Component.literal("1"));
        addRenderableWidget(tickDivisorField);
        y += 36;

        addRenderableWidget(Button.builder(Component.literal("Done"), btn -> closeAndSave())
                .bounds(cx, y, bw, 20)
                .build());
        addRenderableWidget(Button.builder(Component.literal("Cancel"), btn -> onClose())
                .bounds(cx, y + 24, bw, 20)
                .build());
    }

    private void closeAndSave()
    {
        config.host = hostField.getValue().trim();
        config.port = parseInt(portField.getValue(), config.port);
        config.blocksPerMeter = parseFloat(blocksField.getValue(), config.blocksPerMeter);
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

    private static float parseFloat(String text, float fallback)
    {
        try
        {
            return Float.parseFloat(text.trim());
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
