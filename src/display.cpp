#include "display.hpp"

#include "util.hpp"
#include "swarkland.hpp"
#include "load_image.hpp"
#include "byte_buffer.hpp"

#include <rucksack.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

bool expand_message_box;

// screen layout
static const SDL_Rect message_area = { 0, 0, map_size.x * tile_size, 2 * tile_size };
static const SDL_Rect main_map_area = { 0, message_area.y + message_area.h, map_size.x * tile_size, map_size.y * tile_size };
static const SDL_Rect status_box_area = { 0, main_map_area.y + main_map_area.h, main_map_area.w, 32 };
static const SDL_Rect hp_area = { 0, status_box_area.y, 200, status_box_area.h };
static const SDL_Rect kills_area = { hp_area.x + hp_area.w, status_box_area.y, 200, status_box_area.h };
static const SDL_Rect entire_window_area = { 0, 0, status_box_area.w, status_box_area.y + status_box_area.h };


static SDL_Window * window;
static SDL_Texture * sprite_sheet_texture;
static SDL_Renderer * renderer;

static struct RuckSackBundle * bundle;
static struct RuckSackTexture * rs_texture;
static struct RuckSackImage ** spritesheet_images;

static struct RuckSackImage * species_images[SpeciesId_COUNT];
static struct RuckSackImage * floor_images[8];
static struct RuckSackImage * wall_images[8];

static TTF_Font * status_box_font;
static unsigned char *font_buffer;
static SDL_RWops *font_rw_ops;

static struct RuckSackImage * find_image(struct RuckSackImage ** spritesheet_images, long image_count, const char * name) {
    for (int i = 0; i < image_count; i++)
        if (strcmp(spritesheet_images[i]->key, name) == 0)
            return spritesheet_images[i];
    panic("sprite not found");
}
static void load_images(struct RuckSackImage ** spritesheet_images, long image_count) {
    species_images[SpeciesId_HUMAN] = find_image(spritesheet_images, image_count, "img/human.png");
    species_images[SpeciesId_OGRE] = find_image(spritesheet_images, image_count, "img/ogre.png");
    species_images[SpeciesId_DOG] = find_image(spritesheet_images, image_count, "img/dog.png");
    species_images[SpeciesId_PINK_BLOB] = find_image(spritesheet_images, image_count, "img/pink_blob.png");
    species_images[SpeciesId_AIR_ELEMENTAL] = find_image(spritesheet_images, image_count, "img/air_elemental.png");

    floor_images[0] = find_image(spritesheet_images, image_count, "img/grey_dirt0.png");
    floor_images[1] = find_image(spritesheet_images, image_count, "img/grey_dirt1.png");
    floor_images[2] = find_image(spritesheet_images, image_count, "img/grey_dirt2.png");
    floor_images[3] = find_image(spritesheet_images, image_count, "img/grey_dirt3.png");
    floor_images[4] = find_image(spritesheet_images, image_count, "img/grey_dirt4.png");
    floor_images[5] = find_image(spritesheet_images, image_count, "img/grey_dirt5.png");
    floor_images[6] = find_image(spritesheet_images, image_count, "img/grey_dirt6.png");
    floor_images[7] = find_image(spritesheet_images, image_count, "img/grey_dirt7.png");

    wall_images[0] = find_image(spritesheet_images, image_count, "img/brick_brown0.png");
    wall_images[1] = find_image(spritesheet_images, image_count, "img/brick_brown1.png");
    wall_images[2] = find_image(spritesheet_images, image_count, "img/brick_brown2.png");
    wall_images[3] = find_image(spritesheet_images, image_count, "img/brick_brown3.png");
    wall_images[4] = find_image(spritesheet_images, image_count, "img/brick_brown4.png");
    wall_images[5] = find_image(spritesheet_images, image_count, "img/brick_brown5.png");
    wall_images[6] = find_image(spritesheet_images, image_count, "img/brick_brown6.png");
    wall_images[7] = find_image(spritesheet_images, image_count, "img/brick_brown7.png");
}

void display_init() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        panic("unable to init SDL");
    }
    rucksack_init();

    window = SDL_CreateWindow("Legend of Swarkland", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, entire_window_area.w, entire_window_area.h, 0);
    if (!window) {
        panic("window create failed");
    }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        panic("renderer create failed");
    }

    if (rucksack_bundle_open("build/resources.bundle", &bundle) != RuckSackErrorNone) {
        panic("error opening resource bundle");
    }
    struct RuckSackFileEntry * entry = rucksack_bundle_find_file(bundle, "spritesheet", -1);
    if (!entry) {
        panic("spritesheet not found in bundle");
    }

    if (rucksack_file_open_texture(entry, &rs_texture) != RuckSackErrorNone) {
        panic("open texture failed");
    }

    sprite_sheet_texture = load_texture(renderer, rs_texture);

    long image_count = rucksack_texture_image_count(rs_texture);
    spritesheet_images = new struct RuckSackImage*[image_count];
    rucksack_texture_get_images(rs_texture, spritesheet_images);
    load_images(spritesheet_images, image_count);

    TTF_Init();

    struct RuckSackFileEntry * font_entry = rucksack_bundle_find_file(bundle, "font/OpenSans-Regular.ttf", -1);
    if (!font_entry) {
        panic("font not found in bundle");
    }
    long font_file_size = rucksack_file_size(font_entry);
    font_buffer = new unsigned char[font_file_size];
    rucksack_file_read(font_entry, font_buffer);
    font_rw_ops = SDL_RWFromMem(font_buffer, font_file_size);
    if (!font_rw_ops) {
        panic("sdl rwops fail");
    }
    status_box_font = TTF_OpenFontRW(font_rw_ops, 0, 16);
}

void display_finish() {
    TTF_Quit();

    SDL_RWclose(font_rw_ops);
    delete[] font_buffer;

    delete[] spritesheet_images;
    rucksack_texture_destroy(rs_texture);

    SDL_DestroyTexture(sprite_sheet_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    if (rucksack_bundle_close(bundle) != RuckSackErrorNone) {
        panic("error closing resource bundle");
    }
    rucksack_finish();
    SDL_Quit();
}

static Individual get_spectate_individual() {
    return cheatcode_spectator != NULL ? cheatcode_spectator : you;
}

static void render_tile(SDL_Renderer * renderer, SDL_Texture * texture, struct RuckSackImage * guy_image, Coord coord) {
    SDL_Rect source_rect;
    source_rect.x = guy_image->x;
    source_rect.y = guy_image->y;
    source_rect.w = guy_image->width;
    source_rect.h = guy_image->height;

    SDL_Rect dest_rect;
    dest_rect.x = main_map_area.x + coord.x * tile_size;
    dest_rect.y = main_map_area.y + coord.y * tile_size;
    dest_rect.w = tile_size;
    dest_rect.h = tile_size;

    SDL_RenderCopyEx(renderer, texture, &source_rect, &dest_rect, 0.0, NULL, SDL_FLIP_VERTICAL);
}

static const SDL_Color white = {0xff, 0xff, 0xff};
// the text will be aligned to the bottom of the area
static void render_text(const char * str, SDL_Rect area) {
    SDL_Surface * surface = TTF_RenderText_Blended_Wrapped(status_box_font, str, white, area.w);
    SDL_Texture * texture = SDL_CreateTextureFromSurface(renderer, surface);

    // holy shit. the goddamn box is too tall.
    // each new line added during the wrap adds some extra blank space at the bottom.
    // this forumla here was determined through experimentation.
    // piece of shit.
    int real_surface_h = 1 + (int)(surface->h - (float)surface->h / 12.5f);

    SDL_Rect source_rect;
    source_rect.w = min(surface->w, area.w);
    source_rect.h = min(real_surface_h, area.h);
    // align the bottom
    source_rect.x = 0;
    source_rect.y = real_surface_h - source_rect.h;

    SDL_Rect dest_rect;
    dest_rect.x = area.x;
    dest_rect.y = area.y;
    dest_rect.w = source_rect.w;
    dest_rect.h = source_rect.h;
    SDL_RenderFillRect(renderer, &dest_rect);
    SDL_RenderCopyEx(renderer, texture, &source_rect, &dest_rect, 0.0, NULL, SDL_FLIP_NONE);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

static Coord get_mouse_pixels() {
    Coord result;
    SDL_GetMouseState(&result.x, &result.y);
    return result;
}
Coord get_mouse_tile() {
    Coord pixels = get_mouse_pixels();
    pixels.x -= main_map_area.x;
    pixels.y -= main_map_area.y;
    Coord tile_coord = {pixels.x / tile_size, pixels.y / tile_size};
    return tile_coord;
}
static bool rect_contains(SDL_Rect rect, Coord point) {
    return rect.x <= point.x && point.x < rect.x + rect.w &&
           rect.y <= point.y && point.y < rect.y + rect.h;
}
static Coord mouse_hover_tile = Coord::nowhere();
void on_mouse_motion() {
    Coord pixels = get_mouse_pixels();
    if (rect_contains(message_area, pixels)) {
        // the mouse is in the message box
        expand_message_box = true;
    } else {
        expand_message_box = false;
    }
    mouse_hover_tile = get_mouse_tile();
    if (!is_in_bounds(mouse_hover_tile))
        mouse_hover_tile = Coord::nowhere();
}

void get_individual_description(Individual observer, uint256 target_id, ByteBuffer * output) {
    if (observer->id == target_id) {
        output->append("you");
        return;
    }
    PerceivedIndividual target = observer->knowledge.perceived_individuals.get(target_id, NULL);
    if (target == NULL) {
        output->append("it");
        return;
    }
    switch (target->species_id) {
        case SpeciesId_HUMAN:
            output->append("a human");
            return;
        case SpeciesId_OGRE:
            output->append("an ogre");
            return;
        case SpeciesId_DOG:
            output->append("a dog");
            return;
        case SpeciesId_PINK_BLOB:
            output->append("a pink blob");
            return;
        case SpeciesId_AIR_ELEMENTAL:
            output->append("an air elemental");
            return;
        default:
            panic("individual description");
    }
}

void render() {
    Individual spectate_from = get_spectate_individual();

    SDL_RenderClear(renderer);

    // main map
    // render the terrain
    for (Coord cursor = {0, 0}; cursor.y < map_size.y; cursor.y++) {
        for (cursor.x = 0; cursor.x < map_size.x; cursor.x++) {
            Tile tile = spectate_from->knowledge.tiles[cursor];
            if (cheatcode_full_visibility)
                tile = actual_map_tiles[cursor];
            if (tile.tile_type == TileType_UNKNOWN)
                continue;
            Uint8 alpha = 0;
            if (spectate_from->knowledge.tile_is_visible[cursor].any())
                alpha = 255;
            else
                alpha = 128;
            SDL_SetTextureAlphaMod(sprite_sheet_texture, alpha);
            RuckSackImage * image = (tile.tile_type == TileType_FLOOR ? floor_images : wall_images)[tile.aesthetic_index];
            render_tile(renderer, sprite_sheet_texture, image, cursor);
        }
    }

    // render the individuals
    if (!cheatcode_full_visibility) {
        // not cheating
        for (auto iterator = spectate_from->knowledge.perceived_individuals.value_iterator(); iterator.has_next();) {
            PerceivedIndividual individual = iterator.next();
            Uint8 alpha;
            if (individual->invisible || !spectate_from->knowledge.tile_is_visible[individual->location].any())
                alpha = 128;
            else
                alpha = 255;
            SDL_SetTextureAlphaMod(sprite_sheet_texture, alpha);
            render_tile(renderer, sprite_sheet_texture, species_images[individual->species_id], individual->location);
        }
    } else {
        // full visibility
        for (auto iterator = actual_individuals.value_iterator(); iterator.has_next();) {
            Individual individual = iterator.next();
            if (!individual->is_alive)
                continue;
            Uint8 alpha;
            if (individual->invisible || !spectate_from->knowledge.tile_is_visible[individual->location].any())
                alpha = 128;
            else
                alpha = 255;
            SDL_SetTextureAlphaMod(sprite_sheet_texture, alpha);
            render_tile(renderer, sprite_sheet_texture, species_images[individual->species_id], individual->location);
        }
    }

    // status box
    ByteBuffer status_text;
    status_text.format("HP: %d", spectate_from->hitpoints);
    render_text(status_text.raw(), hp_area);

    status_text.resize(0);
    status_text.format("Kills: %d", spectate_from->kill_counter);
    render_text(status_text.raw(), kills_area);

    // message area
    {
        ByteBuffer all_the_text;
        List<RememberedEvent> & events = spectate_from->knowledge.remembered_events;
        for (int i = 0; i < events.length(); i++) {
            RememberedEvent event = events[i];
            if (event != NULL) {
                // append something
                if (i > 0) {
                    // maybe sneak in a delimiter
                    if (events[i - 1] == NULL)
                        all_the_text.append("\n");
                    else
                        all_the_text.append("  ");
                }
                all_the_text.append(event->bytes);
            }
        }
        SDL_Rect current_message_area;
        if (expand_message_box) {
            current_message_area = entire_window_area;
        } else {
            current_message_area = message_area;
        }
        if (all_the_text.length() > 0) {
            render_text(all_the_text.raw(), current_message_area);
        }
    }

    // popup help for hovering over things
    if (mouse_hover_tile != Coord::nowhere()) {
        PerceivedIndividual target = find_perceived_individual_at(spectate_from, mouse_hover_tile);
        if (target != NULL) {
            ByteBuffer description;
            get_individual_description(spectate_from, target->id, &description);
            SDL_Rect rect;
            rect.x = main_map_area.x + (target->location.x + 1) * tile_size;
            rect.y = main_map_area.y + (target->location.y + 1) * tile_size;
            rect.w = entire_window_area.w - rect.x;
            rect.h = entire_window_area.h - rect.y;
            render_text(description.raw(), rect);
        }
    }

    SDL_RenderPresent(renderer);
}
