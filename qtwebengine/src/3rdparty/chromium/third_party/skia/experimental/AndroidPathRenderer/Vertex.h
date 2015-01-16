/*
 * Copyright 2012 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ANDROID_HWUI_VERTEX_H
#define ANDROID_HWUI_VERTEX_H

namespace android {
namespace uirenderer {

/**
 * Simple structure to describe a vertex with a position and a texture.
 */
struct Vertex {
    float position[2];

    static inline void set(Vertex* vertex, float x, float y) {
        vertex[0].position[0] = x;
        vertex[0].position[1] = y;
    }
}; // struct Vertex

/**
 * Simple structure to describe a vertex with a position and a texture.
 */
/*struct TextureVertex {
    float position[2];
    float texture[2];

    static inline void set(TextureVertex* vertex, float x, float y, float u, float v) {
        vertex[0].position[0] = x;
        vertex[0].position[1] = y;
        vertex[0].texture[0] = u;
        vertex[0].texture[1] = v;
    }

    static inline void setUV(TextureVertex* vertex, float u, float v) {
        vertex[0].texture[0] = u;
        vertex[0].texture[1] = v;
    }
};*/ // struct TextureVertex

/**
 * Simple structure to describe a vertex with a position and an alpha value.
 */
struct AlphaVertex : Vertex {
    float alpha;

    static inline void set(AlphaVertex* vertex, float x, float y, float alpha) {
        Vertex::set(vertex, x, y);
        vertex[0].alpha = alpha;
    }

    static inline void setColor(AlphaVertex* vertex, float alpha) {
        vertex[0].alpha = alpha;
    }
}; // struct AlphaVertex

/**
 * Simple structure to describe a vertex with a position and an alpha value.
 */
/*struct AAVertex : Vertex {
    float width;
    float length;

    static inline void set(AAVertex* vertex, float x, float y, float width, float length) {
        Vertex::set(vertex, x, y);
        vertex[0].width = width;
        vertex[0].length = length;
    }

    static inline void setColor(AAVertex* vertex, float width, float length) {
        vertex[0].width = width;
        vertex[0].length = length;
    }
};*/ // struct AlphaVertex

}; // namespace uirenderer
}; // namespace android

#endif // ANDROID_HWUI_VERTEX_H
