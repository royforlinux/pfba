//
// Created by cpasjuste on 21/11/16.
//

#include "ctr_renderer.h"
#include "ctr_font.h"
#include "ctr_texture.h"
#include <sf2d.h>
#include <citro3d.h>

//////////
// INIT //
//////////
CTRRenderer::CTRRenderer() : Renderer() {

    sf2d_init();
    sf2d_set_clear_color((u32) RGBA8(color.r, color.g, color.b, color.a));
    sf2d_set_3D(0);

    sftd_init();
}
//////////
// INIT //
//////////

//////////
// FONT //
//////////
Font *CTRRenderer::LoadFont(const char *path, int size) {

    Font *font = (Font *)
            new CTRFont(path, size);
    return font;
}

void CTRRenderer::DrawFont(Font *font, int x, int y, const char *fmt, ...) {

    if (((CTRFont *) font)->font == NULL) {
        return;
    }

    char msg[MAX_PATH];
    memset(msg, 0, MAX_PATH);
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, MAX_PATH, fmt, args);
    va_end(args);

    StartDrawing();
    sftd_draw_text(((CTRFont *) font)->font, x, y,
                   (unsigned int) RGBA8(font->color.r, font->color.g,
                                        font->color.b, font->color.a),
                   (unsigned int) ((float) font->size * font->scaling), msg);
}
//////////
// FONT //
//////////

/////////////
// TEXTURE //
/////////////
Texture *CTRRenderer::CreateTexture(int w, int h) {
    CTRTexture *texture = new CTRTexture(w, h);
    if (texture->tex == NULL) {
        delete (texture);
        return NULL;
    }
    return (Texture *) texture;
}

Texture *CTRRenderer::LoadTexture(const char *file) {
    return NULL;
}

void CTRRenderer::DrawTexture(Texture *texture, int x, int y, int w, int h, float rotation) {
    if (texture && ((CTRTexture *) texture)->tex) {

        float sx = (float) w / (float) texture->width;
        float sy = (float) h / (float) texture->height;
        StartDrawing();

        const float rad = rotation * 0.0174532925f;
        sf2d_draw_texture_rotate_scale_hotspot(((CTRTexture *) texture)->tex,
                                               x + w / 2, y + h / 2,
                                               rad,
                                               sx, sy,
                                               x + w / 2,
                                               y + h / 2);
    }
}

int CTRRenderer::LockTexture(Texture *texture, const Rect &rect, void **pixels, int *pitch) {
    *pixels = ((CTRTexture *) texture)->tex->tex.data;
    *pitch = texture->width * 2;
    return 0;
}

/////////////
// TEXTURE //
/////////////

const Rect CTRRenderer::GetWindowSize() {
    Rect rect{0, 0, 400, 240};
    return rect;
}

void CTRRenderer::SetShader(int index) {

}

void CTRRenderer::DrawLine(int x1, int y1, int x2, int y2, const Color &c) {

    StartDrawing();

    sf2d_draw_line(x1, y1, x2, y2, 1,
                   (u32) RGBA8(c.r, c.g, c.b, c.a));
}

void CTRRenderer::DrawRect(const Rect &rect, const Color &c, bool fill) {

    StartDrawing();

    if (fill) {
        sf2d_draw_rectangle(rect.x, rect.y, rect.w, rect.h,
                            (u32) RGBA8(c.r, c.g, c.b, c.a));
    } else {
        DrawLine(rect.x, rect.y, rect.x + rect.w, rect.y, c);               // top
        DrawLine(rect.x, rect.y, rect.x, rect.y + rect.h, c);               // left
        DrawLine(rect.x, rect.y + rect.h, rect.x + rect.w, rect.y + rect.h, c);     // bottom
        DrawLine(rect.x + rect.w, rect.y, rect.x + rect.w, rect.y + rect.h, c);     // right
    }
}

void CTRRenderer::Clip(const Rect &rect) {

}

void CTRRenderer::Clear() {
}

void CTRRenderer::Flip() {
    if (drawing_started) {
        sf2d_end_frame();
        sf2d_swapbuffers();
        drawing_started = false;
    }
}

void CTRRenderer::Delay(unsigned int ms) {

}

CTRRenderer::~CTRRenderer() {
    sftd_fini();
    sf2d_fini();
}

void CTRRenderer::StartDrawing() {
    if (!drawing_started) {
        sf2d_start_frame(GFX_TOP, GFX_LEFT);
        drawing_started = true;
    }
}

void ctr_printf(const char *msg, ...) {
    char buffer[256];
    va_list args;
    va_start(args, msg);
    vsnprintf(buffer, 256, msg, args);
    svcOutputDebugString(buffer, strlen(buffer));
    va_end(args);
}