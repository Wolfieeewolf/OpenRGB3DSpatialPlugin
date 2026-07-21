package me.wolfi.openrgb;

import net.minecraft.client.Minecraft;
import net.minecraft.client.player.LocalPlayer;
import net.minecraft.core.BlockPos;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.item.ItemEntity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LightLayer;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Samples mobs, dropped items, and other entities along a room-sample ray.
 * Entities are gathered once per room frame to avoid per-cell world queries.
 */
final class EntityDisplayColorSampler
{
    private static final Logger LOGGER = LoggerFactory.getLogger("openrgb-sender");
    private static final int MAX_ENTITY_HITS = 4;
    private static final int MAX_FRAME_ENTITIES = 96;
    private static final double RAY_PAD = 0.35;
    private static final ConcurrentHashMap<Identifier, BlockDisplayColorSampler.TextureSample> TEXTURE_AVG =
            new ConcurrentHashMap<>();

    private static List<Entity> frameEntities = List.of();
    private static final ThreadLocal<double[]> T_HIT = ThreadLocal.withInitial(() -> new double[1]);
    private static final ThreadLocal<int[]> RGBA = ThreadLocal.withInitial(() -> new int[4]);
    private static final ThreadLocal<ArrayList<EntityHit>> HITS =
            ThreadLocal.withInitial(() -> new ArrayList<>(4));

    record EntityHit(double distanceSq, int r, int g, int b, int a)
    {
    }

    private EntityDisplayColorSampler()
    {
    }

    /** Call once before sampling cells for a room frame. */
    static void beginFrame(Level world, LocalPlayer player, double maxRangeBlocks)
    {
        frameEntities = List.of();
        if(world == null || player == null)
        {
            return;
        }
        try
        {
            final double range = Math.max(8.0, Math.min(96.0, maxRangeBlocks));
            final Vec3 eye = player.getEyePosition();
            final AABB volume = new AABB(eye, eye).inflate(range);
            final List<Entity> found = world.getEntities(player, volume,
                    e -> e != null && e.isAlive() && !e.isSpectator() && e != player);
            if(found.isEmpty())
            {
                frameEntities = List.of();
                return;
            }
            if(found.size() <= MAX_FRAME_ENTITIES)
            {
                frameEntities = found;
                return;
            }
            // Prefer nearer entities when the area is crowded.
            found.sort(Comparator.comparingDouble(e -> e.distanceToSqr(eye)));
            frameEntities = new ArrayList<>(found.subList(0, MAX_FRAME_ENTITIES));
        }
        catch(Throwable t)
        {
            QuietCatch.debug(LOGGER, "entity frame gather failed", t);
            frameEntities = List.of();
        }
    }

    static void endFrame()
    {
        frameEntities = List.of();
    }

    static List<EntityHit> collectHits(Level world, LocalPlayer player, Vec3 from, Vec3 to)
    {
        final ArrayList<EntityHit> hits = HITS.get();
        hits.clear();
        if(world == null || from == null || to == null || frameEntities.isEmpty())
        {
            return hits;
        }
        try
        {
            final Vec3 dir = to.subtract(from);
            if(dir.lengthSqr() < 1.0e-8)
            {
                return hits;
            }
            final AABB sweep = new AABB(from, to).inflate(RAY_PAD);
            final double[] tHit = T_HIT.get();
            final int[] rgba = RGBA.get();
            for(Entity entity : frameEntities)
            {
                if(hits.size() >= MAX_ENTITY_HITS)
                {
                    break;
                }
                final AABB box = entity.getBoundingBox();
                if(!box.intersects(sweep))
                {
                    continue;
                }
                if(!rayAabb(from, dir, box, tHit))
                {
                    continue;
                }
                final double t = tHit[0];
                if(t < 0.0 || t > 1.0)
                {
                    continue;
                }
                final Vec3 hitPos = from.add(dir.scale(t));
                if(!sampleEntityRgba(world, entity, hitPos, rgba))
                {
                    continue;
                }
                hits.add(new EntityHit(from.distanceToSqr(hitPos), rgba[0], rgba[1], rgba[2], rgba[3]));
            }
            hits.sort(Comparator.comparingDouble(EntityHit::distanceSq));
            return hits;
        }
        catch(Throwable t)
        {
            QuietCatch.debug(LOGGER, "entity display sample failed", t);
            hits.clear();
            return hits;
        }
    }

    private static boolean sampleEntityRgba(Level world, Entity entity, Vec3 hitPos, int[] out)
    {
        final Minecraft client = Minecraft.getInstance();
        int rgb = 0xC0C0C0;
        int alpha = 220;

        if(entity instanceof ItemEntity itemEntity)
        {
            alpha = 235;
            final ItemStack stack = itemEntity.getItem();
            final Identifier itemId = BuiltInRegistries.ITEM.getKey(stack.getItem());
            if(itemId != null)
            {
                final Identifier tex = Identifier.fromNamespaceAndPath(itemId.getNamespace(),
                        "item/" + itemId.getPath());
                final int sampled = sampleTextureDetail(client, tex, entity, hitPos);
                if(sampled != 0)
                {
                    rgb = sampled & 0xFFFFFF;
                    alpha = Math.max(170, (sampled >>> 24) & 0xFF);
                }
            }
        }
        else
        {
            final Identifier typeId = BuiltInRegistries.ENTITY_TYPE.getKey(entity.getType());
            if(typeId != null)
            {
                final Identifier[] candidates = {
                        Identifier.fromNamespaceAndPath(typeId.getNamespace(), "entity/" + typeId.getPath()),
                        Identifier.fromNamespaceAndPath(typeId.getNamespace(),
                                "entity/" + typeId.getPath() + "/" + typeId.getPath()),
                };
                int sampled = 0;
                for(Identifier cand : candidates)
                {
                    sampled = sampleTextureDetail(client, cand, entity, hitPos);
                    if(sampled != 0)
                    {
                        break;
                    }
                }
                if(sampled != 0)
                {
                    rgb = sampled & 0xFFFFFF;
                    alpha = Math.max(190, Math.min(240, (sampled >>> 24) & 0xFF));
                }
                else
                {
                    rgb = hashTypeColor(typeId);
                    alpha = 210;
                }
            }
        }

        // Mild saturation lift so mobs/drops read on LEDs.
        rgb = boostRgb(rgb, 1.25f);

        final BlockPos pos = entity.blockPosition();
        final int sky = world.getBrightness(LightLayer.SKY, pos);
        final int block = world.getBrightness(LightLayer.BLOCK, pos);
        final float skyBright = AtmosphereSampler.skyBrightness(world);
        final float local = Math.max(sky, block) / 15.0f;
        float k = 0.72f + 0.28f * local;
        if(sky >= block)
        {
            k *= (0.45f + 0.55f * skyBright);
        }
        out[0] = ColorMath.clamp255((int)(((rgb >> 16) & 0xFF) * k));
        out[1] = ColorMath.clamp255((int)(((rgb >> 8) & 0xFF) * k));
        out[2] = ColorMath.clamp255((int)((rgb & 0xFF) * k));
        out[3] = ColorMath.clamp255(alpha);
        return out[3] > 0;
    }

    private static int sampleTextureDetail(Minecraft client, Identifier spriteId, Entity entity, Vec3 hitPos)
    {
        if(client == null || spriteId == null)
        {
            return 0;
        }
        // Only use already-cached LRU pixels — never decode PNGs on the hot path.
        final SpritePixels pixels = BlockTexturePrecache.getPixels(spriteId);
        if(pixels != null)
        {
            final float[] uv = mapHitToEntityUv(entity, hitPos);
            return pixels.sampleOpaqueNear(uv[0], uv[1], 40);
        }
        final BlockDisplayColorSampler.TextureSample avg = TEXTURE_AVG.get(spriteId);
        if(avg != null)
        {
            return (Math.max(180, avg.averageAlpha()) << 24) | (avg.rgb() & 0xFFFFFF);
        }
        final BlockDisplayColorSampler.TextureSample cached = BlockTexturePrecache.get(spriteId);
        if(cached != null)
        {
            TEXTURE_AVG.putIfAbsent(spriteId, cached);
            return (Math.max(180, cached.averageAlpha()) << 24) | (cached.rgb() & 0xFFFFFF);
        }
        return 0;
    }

    private static float[] mapHitToEntityUv(Entity entity, Vec3 hitPos)
    {
        final AABB box = entity.getBoundingBox();
        final double w = Math.max(1.0e-4, box.getXsize());
        final double h = Math.max(1.0e-4, box.getYsize());
        final double d = Math.max(1.0e-4, box.getZsize());
        final float u = (float)((hitPos.x - box.minX) / w);
        final float v = (float)(1.0 - (hitPos.y - box.minY) / h);
        final float side = (float)((hitPos.z - box.minZ) / d);
        // Mix XZ so side hits still vary across the sprite.
        return new float[] {
                clamp01(u * 0.65f + side * 0.35f),
                clamp01(v)
        };
    }

    private static int boostRgb(int rgb, float sat)
    {
        int r = (rgb >> 16) & 0xFF;
        int g = (rgb >> 8) & 0xFF;
        int b = rgb & 0xFF;
        float gray = (r + g + b) / 3.0f;
        r = ColorMath.clamp255(Math.round(gray + (r - gray) * sat));
        g = ColorMath.clamp255(Math.round(gray + (g - gray) * sat));
        b = ColorMath.clamp255(Math.round(gray + (b - gray) * sat));
        return (r << 16) | (g << 8) | b;
    }

    private static float clamp01(float v)
    {
        if(v < 0.0f)
        {
            return 0.0f;
        }
        if(v > 1.0f)
        {
            return 1.0f;
        }
        return v;
    }

    private static int hashTypeColor(Identifier typeId)
    {
        final int h = typeId.toString().hashCode();
        int r = 80 + ((h >>> 16) & 0x7F);
        int g = 80 + ((h >>> 8) & 0x7F);
        int b = 80 + (h & 0x7F);
        return (r << 16) | (g << 8) | b;
    }

    private static boolean rayAabb(Vec3 origin, Vec3 dir, AABB box, double[] tOut)
    {
        final double[] range = new double[] {0.0, 1.0};
        if(!clipAxis(origin.x, dir.x, box.minX, box.maxX, range))
        {
            return false;
        }
        if(!clipAxis(origin.y, dir.y, box.minY, box.maxY, range))
        {
            return false;
        }
        if(!clipAxis(origin.z, dir.z, box.minZ, box.maxZ, range))
        {
            return false;
        }
        tOut[0] = range[0];
        return true;
    }

    private static boolean clipAxis(double ox, double dx, double min, double max, double[] range)
    {
        double tMin = range[0];
        double tMax = range[1];
        if(Math.abs(dx) < 1.0e-12)
        {
            return ox >= min && ox <= max;
        }
        double inv = 1.0 / dx;
        double t1 = (min - ox) * inv;
        double t2 = (max - ox) * inv;
        if(t1 > t2)
        {
            final double tmp = t1;
            t1 = t2;
            t2 = tmp;
        }
        tMin = Math.max(tMin, t1);
        tMax = Math.min(tMax, t2);
        if(tMin > tMax)
        {
            return false;
        }
        range[0] = tMin;
        range[1] = tMax;
        return true;
    }
}
