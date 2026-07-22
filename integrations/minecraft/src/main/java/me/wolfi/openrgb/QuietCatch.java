package me.wolfi.openrgb;

import org.slf4j.Logger;

/**
 * Hot-path catch helper: logs at DEBUG so failures are visible when debugging
 * without spamming INFO/WARN every sample tick.
 */
final class QuietCatch
{
    private QuietCatch()
    {
    }

    static void debug(Logger logger, String message, Throwable t)
    {
        if(logger != null && logger.isDebugEnabled())
        {
            logger.debug(message, t);
        }
    }
}
