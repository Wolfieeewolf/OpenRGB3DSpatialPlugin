package me.wolfi.openrgb;

import net.minecraft.client.gui.DrawContext;
import net.minecraft.client.gui.screen.Screen;
import net.minecraft.client.gui.widget.ButtonWidget;
import net.minecraft.client.gui.widget.CyclingButtonWidget;
import net.minecraft.client.gui.widget.TextFieldWidget;
import net.minecraft.text.Text;

public class OpenRGBConfigScreen extends Screen
{
    private final Screen parent;
    private final OpenRGBSenderConfig config;
    private CyclingButtonWidget enabledButton;
    private CyclingButtonWidget voxelButton;
    private TextFieldWidget hostField;
    private TextFieldWidget portField;
    private TextFieldWidget blocksField;
    private TextFieldWidget tickDivisorField;
    private TextFieldWidget voxelIntervalField;

    public OpenRGBConfigScreen(Screen parent)
    {
        super(Text.literal("OpenRGB Minecraft Sender"));
        this.parent = parent;
        this.config = OpenRGBSenderConfig.get();
    }

    @Override
    protected void init()
    {
        int bw = 220;
        int cx = width / 2 - bw / 2;
        int y = height / 6;

        enabledButton = CyclingButtonWidget.onOffBuilder(config.enabled)
                .build(cx, y, bw, 20, Text.literal("Send telemetry"), (btn, val) -> config.enabled = val);
        addDrawableChild(enabledButton);
        y += 28;

        hostField = new TextFieldWidget(textRenderer, cx, y, bw, 20, Text.literal("Host"));
        hostField.setMaxLength(128);
        hostField.setText(config.host);
        hostField.setPlaceholder(Text.literal("127.0.0.1"));
        addDrawableChild(hostField);
        y += 28;

        portField = new TextFieldWidget(textRenderer, cx, y, bw, 20, Text.literal("Port"));
        portField.setMaxLength(5);
        portField.setText(Integer.toString(config.port));
        portField.setPlaceholder(Text.literal("9876"));
        addDrawableChild(portField);
        y += 28;

        blocksField = new TextFieldWidget(textRenderer, cx, y, bw, 20, Text.literal("Blocks per meter"));
        blocksField.setMaxLength(8);
        blocksField.setText(String.format(java.util.Locale.US, "%.2f", config.blocksPerMeter));
        blocksField.setPlaceholder(Text.literal("1.00"));
        addDrawableChild(blocksField);
        y += 28;

        voxelButton = CyclingButtonWidget.onOffBuilder(config.sendVoxelFrames)
                .build(cx, y, bw, 20, Text.literal("Send voxel frames (room VR tint)"), (btn, val) -> config.sendVoxelFrames = val);
        addDrawableChild(voxelButton);
        y += 28;

        tickDivisorField = new TextFieldWidget(textRenderer, cx, y, bw, 20, Text.literal("Telemetry tick divisor"));
        tickDivisorField.setMaxLength(2);
        tickDivisorField.setText(Integer.toString(config.telemetryTickDivisor));
        tickDivisorField.setPlaceholder(Text.literal("2"));
        addDrawableChild(tickDivisorField);
        y += 28;

        voxelIntervalField = new TextFieldWidget(textRenderer, cx, y, bw, 20, Text.literal("Voxel send interval"));
        voxelIntervalField.setMaxLength(2);
        voxelIntervalField.setText(Integer.toString(config.voxelSendInterval));
        voxelIntervalField.setPlaceholder(Text.literal("1"));
        addDrawableChild(voxelIntervalField);
        y += 36;

        addDrawableChild(ButtonWidget.builder(Text.literal("Done"), btn -> closeAndSave())
                .dimensions(cx, y, bw, 20)
                .build());
        addDrawableChild(ButtonWidget.builder(Text.literal("Cancel"), btn -> client.setScreen(parent))
                .dimensions(cx, y + 24, bw, 20)
                .build());
    }

    private void closeAndSave()
    {
        config.host = hostField.getText().trim();
        config.port = parseInt(portField.getText(), config.port);
        config.blocksPerMeter = parseFloat(blocksField.getText(), config.blocksPerMeter);
        config.telemetryTickDivisor = parseInt(tickDivisorField.getText(), config.telemetryTickDivisor);
        config.voxelSendInterval = parseInt(voxelIntervalField.getText(), config.voxelSendInterval);
        config.save();
        client.setScreen(parent);
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
    public void render(DrawContext context, int mouseX, int mouseY, float delta)
    {
        renderBackground(context, mouseX, mouseY, delta);
        super.render(context, mouseX, mouseY, delta);
        final int titleWidth = textRenderer.getWidth(title);
        context.drawText(textRenderer, title, (width - titleWidth) / 2, 12, 0xFFFFFF, true);
    }

    @Override
    public void close()
    {
        client.setScreen(parent);
    }
}
