#include "text.hpp"

#include "display.hpp"

#include <SDL2/SDL_ttf.h>

// apparently this is how you use SDL :/
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
static const Uint32 rmask = 0xff000000;
static const Uint32 gmask = 0x00ff0000;
static const Uint32 bmask = 0x0000ff00;
static const Uint32 amask = 0x000000ff;
#else
static const Uint32 rmask = 0x000000ff;
static const Uint32 gmask = 0x0000ff00;
static const Uint32 bmask = 0x00ff0000;
static const Uint32 amask = 0xff000000;
#endif
static SDL_Surface * create_surface(int w, int h) {
    return SDL_CreateRGBSurface(0, w, h, 32, rmask, gmask, bmask, amask);
}
static constexpr Uint32 mask_color(Uint8 value, Uint32 mask) {
    return ((value << 0) |
            (value << 8) |
            (value << 16) |
            (value << 24)) & mask;
}
static constexpr Uint32 pack_color(SDL_Color color) {
    return mask_color(color.r, rmask) |
           mask_color(color.g, gmask) |
           mask_color(color.b, bmask) |
           mask_color(color.a, amask);
}

void SpanImpl::render_surface() {
    if (_surface != NULL)
        return;
    if (is_plain_text()) {
        if (_plain_text->length() == 0)
            return;
        ByteBuffer utf8;
        _plain_text->encode(&utf8);
        // TODO: provide an actual backcolor
        _surface = TTF_RenderUTF8_Blended(status_box_font, utf8.raw(), _foreground);
        return;
    }

    // rich text

    // collect sub surfaces
    List<SDL_Surface*> render_surfaces;
    for (int i = 0; i < _items.length(); i++) {
        Span sub_span = _items[i];
        sub_span->render_surface();
        if (sub_span->_surface == NULL)
            continue;
        render_surfaces.append(sub_span->_surface);
    }
    if (render_surfaces.length() == 0)
        return;

    // measure dimensions
    int width = 0;
    for (int i = 0; i < render_surfaces.length(); i++)
        width += render_surfaces[i]->w;
    SDL_Rect bounds = {0, 0, width, render_surfaces[0]->h};
    _surface = create_surface(bounds.w, bounds.h);
    // copy all the sub surfaces
    SDL_FillRect(_surface, &bounds, pack_color(_background));
    int x_cursor = 0;
    for (int i = 0; i < render_surfaces.length(); i++) {
        SDL_Rect src_rect = {0, 0, render_surfaces[i]->w, render_surfaces[i]->h};
        SDL_Rect dest_rect = src_rect;
        dest_rect.x = x_cursor;
        SDL_BlitSurface(render_surfaces[i], &src_rect, _surface, &dest_rect);
        x_cursor += dest_rect.w;
    }
}
void SpanImpl::render_texture(SDL_Renderer * renderer) {
    if (_texture != NULL)
        return;
    render_surface();
    if (_surface != NULL)
        _texture = SDL_CreateTextureFromSurface(renderer, _surface);
}
