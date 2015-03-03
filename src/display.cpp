#include "display.hpp"

#include "util.hpp"
#include "swarkland.hpp"
#include "load_image.hpp"
#include "byte_buffer.hpp"
#include "item.hpp"
#include "input.hpp"
#include "string.hpp"
#include "text.hpp"
#include "resources.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

// screen layout
static const SDL_Rect message_area = { 0, 0, map_size.x * tile_size, 2 * tile_size };
const SDL_Rect main_map_area = { 0, message_area.y + message_area.h, map_size.x * tile_size, map_size.y * tile_size };
static const SDL_Rect status_box_area = { 0, main_map_area.y + main_map_area.h, main_map_area.w, 32 };
static const SDL_Rect hp_area = { 0, status_box_area.y, 200, status_box_area.h };
static const SDL_Rect kills_area = { hp_area.x + hp_area.w, status_box_area.y, 200, status_box_area.h };
static const SDL_Rect status_area = { kills_area.x + kills_area.w, status_box_area.y, status_box_area.w - (kills_area.x + kills_area.w), status_box_area.h };
static const SDL_Rect inventory_area = { main_map_area.x + main_map_area.w, 2 * tile_size, 5 * tile_size, status_box_area.y + status_box_area.h };
static const SDL_Rect entire_window_area = { 0, 0, inventory_area.x + inventory_area.w, status_box_area.y + status_box_area.h };


static SDL_Window * window;
static SDL_Texture * sprite_sheet_texture;
static SDL_Renderer * renderer;

static RuckSackBundle * bundle;
static RuckSackTexture * rs_texture;
static RuckSackImage ** spritesheet_images;

static RuckSackImage * species_images[SpeciesId_COUNT];
static RuckSackImage * floor_images[8];
static RuckSackImage * wall_images[8];
static RuckSackImage * wand_images[WandDescriptionId_COUNT];
static RuckSackImage * equipment_image;

TTF_Font * status_box_font;
static unsigned char *font_buffer;
static SDL_RWops *font_rw_ops;
static Coord status_box_font_size;

static RuckSackImage * find_image(RuckSackImage ** spritesheet_images, long image_count, const char * name) {
    for (int i = 0; i < image_count; i++)
        if (strcmp(spritesheet_images[i]->key, name) == 0)
            return spritesheet_images[i];
    panic("sprite not found");
}
static void load_images(RuckSackImage ** spritesheet_images, long image_count) {
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

    wand_images[WandDescriptionId_BONE_WAND] = find_image(spritesheet_images, image_count, "img/bone_wand.png");
    wand_images[WandDescriptionId_GOLD_WAND] = find_image(spritesheet_images, image_count, "img/gold_wand.png");
    wand_images[WandDescriptionId_PLASTIC_WAND] = find_image(spritesheet_images, image_count, "img/plastic_wand.png");

    equipment_image = find_image(spritesheet_images, image_count, "img/equipment.png");
}

void display_init() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        panic("unable to init SDL");

    window = SDL_CreateWindow("Legend of Swarkland", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, entire_window_area.w, entire_window_area.h, 0);
    if (window == NULL)
        panic("window create failed");
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL)
        panic("renderer create failed");

    if (rucksack_bundle_open_read_mem(get_binary_resources_start(), get_binary_resources_size(), &bundle) != RuckSackErrorNone)
        panic("error opening resource bundle");
    RuckSackFileEntry * entry = rucksack_bundle_find_file(bundle, "spritesheet", -1);
    if (entry == NULL)
        panic("spritesheet not found in bundle");

    if (rucksack_file_open_texture(entry, &rs_texture) != RuckSackErrorNone)
        panic("open texture failed");

    sprite_sheet_texture = load_texture(renderer, rs_texture);

    long image_count = rucksack_texture_image_count(rs_texture);
    spritesheet_images = allocate<RuckSackImage*>(image_count);
    rucksack_texture_get_images(rs_texture, spritesheet_images);
    load_images(spritesheet_images, image_count);

    TTF_Init();

    RuckSackFileEntry * font_entry = rucksack_bundle_find_file(bundle, "font/DejaVuSansMono.ttf", -1);
    if (font_entry == NULL)
        panic("font not found in bundle");
    long font_file_size = rucksack_file_size(font_entry);
    font_buffer = allocate<unsigned char>(font_file_size);
    rucksack_file_read(font_entry, font_buffer);
    font_rw_ops = SDL_RWFromMem(font_buffer, font_file_size);
    if (font_rw_ops == NULL)
        panic("sdl rwops fail");
    status_box_font = TTF_OpenFontRW(font_rw_ops, 0, 13);
    TTF_SetFontHinting(status_box_font, TTF_HINTING_LIGHT);
    TTF_SizeUTF8(status_box_font, "j", &status_box_font_size.x, &status_box_font_size.y);
    // never mind the actual height. crop it off at the line skip height.
    status_box_font_size.y = TTF_FontLineSkip(status_box_font);
}

void display_finish() {
    TTF_Quit();

    SDL_RWclose(font_rw_ops);
    destroy(font_buffer, 0);

    destroy(spritesheet_images, 0);
    rucksack_texture_close(rs_texture);

    SDL_DestroyTexture(sprite_sheet_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    if (rucksack_bundle_close(bundle) != RuckSackErrorNone) {
        panic("error closing resource bundle");
    }
    SDL_Quit();
}

static inline bool rect_contains(SDL_Rect rect, Coord point) {
    return rect.x <= point.x && point.x < rect.x + rect.w &&
           rect.y <= point.y && point.y < rect.y + rect.h;
}

static Thing get_spectate_individual() {
    return cheatcode_spectator != NULL ? cheatcode_spectator : you;
}

static void render_tile(SDL_Renderer * renderer, SDL_Texture * texture, RuckSackImage * guy_image, int alpha, Coord coord) {
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

    SDL_SetTextureAlphaMod(sprite_sheet_texture, alpha);
    SDL_RenderCopyEx(renderer, texture, &source_rect, &dest_rect, 0.0, NULL, SDL_FLIP_VERTICAL);
}

// {0, 0, w, h}
static inline SDL_Rect get_texture_bounds(SDL_Texture * texture) {
    SDL_Rect result = {0, 0, 0, 0};
    Uint32 format;
    int access;
    SDL_QueryTexture(texture, &format, &access, &result.w, &result.h);
    return result;
}

static void set_color(SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}
static void render_texture(SDL_Texture * texture, SDL_Rect source_rect, SDL_Rect output_area, int horizontal_align, int vertical_align) {
    SDL_Rect dest_rect;
    if (horizontal_align < 0) {
        dest_rect.x = output_area.x + output_area.w - source_rect.w;
    } else {
        dest_rect.x = output_area.x;
    }
    if (vertical_align < 0) {
        dest_rect.y = output_area.y + output_area.h - source_rect.h;
    } else {
        dest_rect.y = output_area.y;
    }
    dest_rect.w = source_rect.w;
    dest_rect.h = source_rect.h;
    set_color(black);
    SDL_RenderFillRect(renderer, &dest_rect);
    SDL_RenderCopyEx(renderer, texture, &source_rect, &dest_rect, 0.0, NULL, SDL_FLIP_NONE);
}
static void render_div(Div div, SDL_Rect output_area, int horizontal_align, int vertical_align) {
    div->set_max_size(output_area.w, output_area.h);
    SDL_Texture * texture = div->get_texture(renderer);
    if (texture == NULL)
        return;
    SDL_Rect source_rect = get_texture_bounds(texture);
    render_texture(texture, source_rect, output_area, horizontal_align, vertical_align);
}

Coord get_mouse_tile(SDL_Rect area) {
    Coord pixels = get_mouse_pixels();
    if (!rect_contains(area, pixels))
        return Coord::nowhere();
    pixels.x -= area.x;
    pixels.y -= area.y;
    Coord tile_coord = {pixels.x / tile_size, pixels.y / tile_size};
    return tile_coord;
}

static const char * get_species_name_str(SpeciesId species_id) {
    switch (species_id) {
        case SpeciesId_HUMAN:
            return "human";
        case SpeciesId_OGRE:
            return "ogre";
        case SpeciesId_DOG:
            return "dog";
        case SpeciesId_PINK_BLOB:
            return "pink blob";
        case SpeciesId_AIR_ELEMENTAL:
            return "air elemental";
        default:
            panic("individual description");
    }
}
Span get_species_name(SpeciesId species_id) {
    return new_span(get_species_name_str(species_id), light_brown, black);
}
Span get_thing_description(Thing observer, uint256 target_id) {
    PerceivedThing actual_thing = observer->life()->knowledge.perceived_things.get(target_id);
    switch (actual_thing->thing_type) {
        case ThingType_INDIVIDUAL:
            return get_individual_description(observer, target_id);
        case ThingType_WAND:
            return get_item_description(observer, target_id);
    }
    panic("thing type");
}
static Span get_status_description(const StatusEffects & status_effects) {
    Span result = new_span();
    if (status_effects.invisible)
        result->append("invisible ");
    if (status_effects.confused_timeout > 0)
        result->append("confused ");
    result->set_color(pink, black);
    return result;
}
Span get_individual_description(Thing observer, uint256 target_id) {
    if (observer->id == target_id)
        return new_span("you", light_blue, black);
    PerceivedThing target = observer->life()->knowledge.perceived_things.get(target_id, NULL);
    if (target == NULL)
        return new_span("it", light_brown, black);
    Span result = new_span();
    result->format("a %s%s", get_status_description(target->status_effects), get_species_name(target->life().species_id));
    return result;
}
static const char * get_item_description_str(Thing observer, uint256 item_id) {
    PerceivedThing item = observer->life()->knowledge.perceived_things.get(item_id, NULL);
    if (item == NULL) {
        // can't see the wand
        return "wand";
    }
    WandId true_id = observer->life()->knowledge.wand_identities[item->wand_info().description_id];
    if (true_id != WandId_UNKNOWN) {
        switch (true_id) {
            case WandId_WAND_OF_CONFUSION:
                return "wand of confusion";
            case WandId_WAND_OF_DIGGING:
                return "wand of digging";
            case WandId_WAND_OF_STRIKING:
                return "wand of striking";
            default:
                panic("wand id");
        }
    } else {
        switch (item->wand_info().description_id) {
            case WandDescriptionId_BONE_WAND:
                return "bone wand";
            case WandDescriptionId_GOLD_WAND:
                return "gold wand";
            case WandDescriptionId_PLASTIC_WAND:
                return "plastic wand";
            default:
                panic("wand id");
        }
    }
}
Span get_item_description(Thing observer, uint256 item_id) {
    Span result = new_span("a ");
    result->append(new_span(get_item_description_str(observer, item_id), light_green, black));
    return result;
}


static void popup_help(SDL_Rect area, Coord tile_in_area, Div div) {
    Coord upper_left_corner = Coord{area.x, area.y} + Coord{tile_in_area.x * tile_size, tile_in_area.y * tile_size};
    Coord lower_right_corner = upper_left_corner + Coord{tile_size, tile_size};
    int horizontal_align = upper_left_corner.x < entire_window_area.w/2 ? 1 : -1;
    int vertical_align = upper_left_corner.y < entire_window_area.h/2 ? 1 : -1;
    SDL_Rect rect;
    if (horizontal_align < 0) {
        rect.x = 0;
        rect.w = upper_left_corner.x;
    } else {
        rect.x = lower_right_corner.x;
        rect.w = entire_window_area.w - lower_right_corner.x;
    }
    if (vertical_align < 0) {
        rect.y = 0;
        rect.h = upper_left_corner.y;
    } else {
        rect.y = lower_right_corner.y;
        rect.h = entire_window_area.h - lower_right_corner.y;
    }
    render_div(div, rect, horizontal_align, vertical_align);
}

// TODO: this duplication looks silly
static RuckSackImage * get_image_for_perceived_thing(PerceivedThing thing) {
    switch (thing->thing_type) {
        case ThingType_INDIVIDUAL:
            return species_images[thing->life().species_id];
        case ThingType_WAND:
            return wand_images[thing->wand_info().description_id];
    }
    panic("thing type");
}
static RuckSackImage * get_image_for_thing(Thing thing) {
    switch (thing->thing_type) {
        case ThingType_INDIVIDUAL:
            return species_images[thing->life()->species_id];
        case ThingType_WAND:
            return wand_images[thing->wand_info()->description_id];
    }
    panic("thing type");
}

static int previous_events_length = 0;
static int previous_event_forget_counter = 0;
static uint256 previous_spectator_id = uint256::zero();
static Div events_div = new_div();
static Div hp_div = new_div();
static Div kills_div = new_div();
static Div status_div = new_div();
static Div keyboard_hover_div = new_div();
static Div mouse_hover_div = new_div();

void render() {
    Thing spectate_from = get_spectate_individual();

    set_color(black);
    SDL_RenderClear(renderer);

    // main map
    // render the terrain
    for (Coord cursor = {0, 0}; cursor.y < map_size.y; cursor.y++) {
        for (cursor.x = 0; cursor.x < map_size.x; cursor.x++) {
            Tile tile = spectate_from->life()->knowledge.tiles[cursor];
            if (cheatcode_full_visibility)
                tile = actual_map_tiles[cursor];
            if (tile.tile_type == TileType_UNKNOWN)
                continue;
            Uint8 alpha;
            if (spectate_from->life()->knowledge.tile_is_visible[cursor].any()) {
                // it's in our direct line of sight
                if (input_mode == InputMode_ZAP_CHOOSE_DIRECTION || input_mode == InputMode_THROW_CHOOSE_DIRECTION) {
                    // actually, let's only show the cardinal directions
                    Coord vector = spectate_from->location - cursor;
                    if (vector.x * vector.y == 0) {
                        // cardinal direction
                        alpha = 0xff;
                    } else if (abs(vector.x) == abs(vector.y)) {
                        // diagonal
                        alpha = 0xff;
                    } else {
                        alpha = 0x7f;
                    }
                } else {
                    alpha = 0xff;
                }
            } else {
                alpha = 0x7f;
            }
            RuckSackImage * image = (tile.tile_type == TileType_FLOOR ? floor_images : wall_images)[tile.aesthetic_index];
            render_tile(renderer, sprite_sheet_texture, image, alpha, cursor);
        }
    }

    // render the things
    if (!cheatcode_full_visibility) {
        // not cheating
        List<PerceivedThing> things;
        PerceivedThing thing;
        for (auto iterator = spectate_from->life()->knowledge.perceived_things.value_iterator(); iterator.next(&thing);) {
            if (thing->location == Coord::nowhere())
                continue;
            things.append(thing);
        }
        sort<PerceivedThing, compare_perceived_things_by_type_and_z_order>(things.raw(), things.length());
        // only render 1 of each type of thing in each location on the map
        MapMatrix<bool> item_pile_rendered;
        item_pile_rendered.set_all(false);
        for (int i = 0; i < things.length(); i++) {
            PerceivedThing thing = things[i];
            if (thing->thing_type == ThingType_WAND) {
                if (item_pile_rendered[thing->location])
                    continue;
                item_pile_rendered[thing->location] = true;
            }
            Uint8 alpha;
            if (thing->status_effects.invisible || !spectate_from->life()->knowledge.tile_is_visible[thing->location].any())
                alpha = 0x7f;
            else
                alpha = 0xff;
            render_tile(renderer, sprite_sheet_texture, get_image_for_perceived_thing(thing), alpha, thing->location);

            List<PerceivedThing> inventory;
            find_items_in_inventory(spectate_from, thing, &inventory);
            if (inventory.length() > 0)
                render_tile(renderer, sprite_sheet_texture, equipment_image, alpha, thing->location);
        }
    } else {
        // full visibility
        Thing thing;
        for (auto iterator = actual_things.value_iterator(); iterator.next(&thing);) {
            // TODO: this exposes hashtable iteration order
            if (!thing->still_exists)
                continue;
            if (thing->location == Coord::nowhere())
                continue;
            Uint8 alpha;
            if (thing->status_effects.invisible || !spectate_from->life()->knowledge.tile_is_visible[thing->location].any())
                alpha = 0x7f;
            else
                alpha = 0xff;
            render_tile(renderer, sprite_sheet_texture, get_image_for_thing(thing), alpha, thing->location);

            List<Thing> inventory;
            find_items_in_inventory(thing->id, &inventory);
            if (inventory.length() > 0)
                render_tile(renderer, sprite_sheet_texture, equipment_image, alpha, thing->location);
        }
    }

    // status box
    {
        String hp_string = new_string();
        int hp = spectate_from->life()->hitpoints;
        hp_string->format("HP: %d", hp);
        Span hp_span = new_span(hp_string);
        if (hp <= 3)
            hp_span->set_color(white, red);
        else if (hp < 10)
            hp_span->set_color(black, amber);
        else
            hp_span->set_color(white, dark_green);
        hp_div->set_content(hp_span);
        render_div(hp_div, hp_area, 1, 1);
    }
    {
        String kills_string = new_string();
        kills_string->format("Kills: %d", spectate_from->life()->kill_counter);
        kills_div->set_content(new_span(kills_string));
        render_div(kills_div, kills_area, 1, 1);
    }
    {
        status_div->set_content(get_status_description(spectate_from->status_effects));
        render_div(status_div, status_area, 1, 1);
    }

    // message area
    {
        bool expand_message_box = rect_contains(message_area, get_mouse_pixels());
        List<RememberedEvent> & events = spectate_from->life()->knowledge.remembered_events;
        bool refresh_events = previous_event_forget_counter != spectate_from->life()->knowledge.event_forget_counter || previous_spectator_id != spectate_from->id;
        if (refresh_events) {
            previous_events_length = 0;
            previous_event_forget_counter = spectate_from->life()->knowledge.event_forget_counter;
            previous_spectator_id = spectate_from->id;
            events_div->clear();
        }
        for (int i = previous_events_length; i < events.length(); i++) {
            RememberedEvent event = events[i];
            if (event != NULL) {
                // append something
                if (i > 0) {
                    // maybe sneak in a delimiter
                    if (events[i - 1] == NULL)
                        events_div->append_newline();
                    else
                        events_div->append_spaces(2);
                }
                events_div->append(event->span);
            }
        }
        previous_events_length = events.length();
        if (expand_message_box) {
            // truncate from the bottom
            events_div->set_max_size(entire_window_area.w, entire_window_area.h);
            SDL_Texture * texture = events_div->get_texture(renderer);
            if (texture != NULL) {
                SDL_Rect source_rect = get_texture_bounds(texture);
                int overflow = source_rect.h - entire_window_area.h;
                if (overflow > 0) {
                    source_rect.y += overflow;
                    source_rect.h -= overflow;
                }
                render_texture(texture, source_rect, entire_window_area, 1, 1);
            }
        } else {
            render_div(events_div, message_area, 1, -1);
        }
    }

    // inventory pane
    List<Thing> inventory;
    find_items_in_inventory(spectate_from->id, &inventory);
    {
        bool render_cursor = input_mode == InputMode_ZAP_CHOOSE_ITEM || input_mode == InputMode_DROP_CHOOSE_ITEM || input_mode == InputMode_THROW_CHOOSE_ITEM;
        if (render_cursor) {
            // render the cursor
            SDL_Rect cursor_rect;
            cursor_rect.x = inventory_area.x;
            cursor_rect.y = inventory_area.y + tile_size * inventory_cursor;
            cursor_rect.w = tile_size;
            cursor_rect.h = tile_size;
            set_color(amber);
            SDL_RenderFillRect(renderer, &cursor_rect);
        }
        Coord location = {map_size.x, 0};
        for (int i = 0; i < inventory.length(); i++) {
            Thing & item = inventory[i];
            render_tile(renderer, sprite_sheet_texture, wand_images[item->wand_info()->description_id], 0xff, location);
            location.y += 1;
        }
        if (render_cursor) {
            // also show popup help
            keyboard_hover_div->set_content(get_item_description(spectate_from, inventory[inventory_cursor]->id));
            popup_help(inventory_area, Coord{0, inventory_cursor}, keyboard_hover_div);
        }
    }

    // popup help for hovering over things
    Coord mouse_hover_map_tile = get_mouse_tile(main_map_area);
    if (mouse_hover_map_tile != Coord::nowhere()) {
        List<PerceivedThing> things;
        find_perceived_things_at(spectate_from, mouse_hover_map_tile, &things);
        if (things.length() != 0) {
            Div content = new_div();
            for (int i = 0; i < things.length(); i++) {
                PerceivedThing target = things[i];
                if (i > 0 )
                    content->append_newline();
                Span thing_and_carrying = new_span();
                thing_and_carrying->append(get_thing_description(spectate_from, target->id));
                List<PerceivedThing> inventory;
                find_items_in_inventory(spectate_from, target, &inventory);
                if (inventory.length() > 0) {
                    thing_and_carrying->append(" carrying:");
                    content->append(thing_and_carrying);
                    for (int j = 0; j < inventory.length(); j++) {
                        content->append_newline();
                        content->append_spaces(4);
                        content->append(get_thing_description(spectate_from, inventory[j]->id));
                    }
                } else {
                    content->append(thing_and_carrying);
                }
            }
            mouse_hover_div->set_content(content);
            popup_help(main_map_area, mouse_hover_map_tile, mouse_hover_div);
        }
    }
    Coord mouse_hover_inventory_tile = get_mouse_tile(inventory_area);
    if (mouse_hover_inventory_tile.x == 0) {
        int inventory_index = mouse_hover_inventory_tile.y;
        if (0 <= inventory_index && inventory_index < inventory.length()) {
            mouse_hover_div->set_content(get_item_description(spectate_from, inventory[inventory_index]->id));
            popup_help(inventory_area, Coord{0, inventory_index}, mouse_hover_div);
        }
    }

    SDL_RenderPresent(renderer);
}
