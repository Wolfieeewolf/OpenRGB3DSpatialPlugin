// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <vector>

/**
 * Durable atlas texture for the room-grid overlay.
 *
 * Pool bake FBOs are recreated on resize / shutdown — their texture names die.
 * The viewport must never keep those ids. Instead engines publish through this
 * handoff and the viewport binds the published name directly (share-group).
 *
 * Publish modes:
 *  - publishShared: zero-copy; retireBeforeDestroy() before deleting the GL tex
 *  - publishCopy: blit into owned RGBA8 (optional Y flip)
 *  - publishFromRgba32f: CPU upload into owned durable tex (Patterns W×2)
 *  - beginCompositeRgba8 + compositePlaneRgba8: stack planes (Screen Mirror)
 *
 * Call releaseDeferred() at the start of each bake, before destroying any FBO.
 */
class SpatialOverlayHandoff
{
public:
    unsigned int textureId() const { return tex_; }
    int width() const { return w_; }
    int height() const { return h_; }
    bool empty() const { return tex_ == 0 || w_ < 1 || h_ < 1; }

    /** Drop textures retired on a prior bake. Pool context must be current. */
    void releaseDeferred();

    bool publishShared(unsigned int src_tex, int w, int h);
    bool publishCopy(unsigned int src_tex, int w, int h, bool flip_y);
    bool publishFromRgba32f(const float* rgba, int w, int h);

    /** Ensure owned RGBA8 atlas and clear to transparent black (padding between planes). */
    bool beginCompositeRgba8(int w, int h);
    bool compositePlaneRgba8(const unsigned char* rgba, int sw, int sh,
                             int dx, int dy, int dw, int dh);

    void retireBeforeDestroy(unsigned int tex);
    void destroy();

private:
    bool ensureBlitFbos();
    bool ensureOwnedTex(int w, int h, unsigned internal_format);
    bool ensureScratchTex(int w, int h);
    void defer(unsigned int tex);

    unsigned int tex_ = 0;
    int w_ = 0;
    int h_ = 0;
    unsigned internal_format_ = 0;
    bool owned_ = false;
    std::vector<unsigned int> deferred_;
    unsigned int blit_read_fbo_ = 0;
    unsigned int blit_draw_fbo_ = 0;
    unsigned int scratch_tex_ = 0;
    int scratch_w_ = 0;
    int scratch_h_ = 0;
};
