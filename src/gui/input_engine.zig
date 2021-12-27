const std = @import("std");
const core = @import("core");
const sdl = @import("./sdl.zig");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;
const Rect = core.geometry.Rect;

pub const Button = enum {
    up,
    down,
    left,
    right,
    start_attack,
    start_kick,
    charge,
    backspace,
    spacebar,
    enter,
    escape,
    restart,
    quit,
    beat_level,
    beat_level_5,
    unbeat_level,
    unbeat_level_5,
    page_up,
    page_down,
    home,
    end,
};

pub const InputEngine = struct {
    pub fn init() InputEngine {
        return InputEngine{};
    }

    pub fn handleEvent(self: *InputEngine, event: sdl.c.SDL_Event) ?Button {
        switch (event.@"type") {
            sdl.c.SDL_KEYUP => {
                return null;
            },
            sdl.c.SDL_KEYDOWN => {
                if (event.key.repeat != 0) return null;
                return self.handleKeydown(self.getModifiers(), event.key.keysym.scancode);
            },
            else => unreachable,
        }
    }

    fn handleKeydown(_: InputEngine, modifiers: Modifiers, scancode: c_uint) ?Button {
        switch (scancode) {
            sdl.c.SDL_SCANCODE_LEFT => return if (modifiers == 0) .left else null,
            sdl.c.SDL_SCANCODE_RIGHT => return if (modifiers == 0) .right else null,
            sdl.c.SDL_SCANCODE_UP => if (modifiers == 0)
                return .up
            else if (modifiers == ctrl)
                return .page_up
            else
                return null,
            sdl.c.SDL_SCANCODE_DOWN => if (modifiers == 0)
                return .down
            else if (modifiers == ctrl)
                return .page_down
            else
                return null,
            sdl.c.SDL_SCANCODE_F => return if (modifiers == 0) .start_attack else null,
            sdl.c.SDL_SCANCODE_K => return if (modifiers == 0) .start_kick else null,
            sdl.c.SDL_SCANCODE_C => return if (modifiers == 0) .charge else null,
            sdl.c.SDL_SCANCODE_R => if (modifiers == 0)
                return .restart
            else if (modifiers == ctrl)
                return .quit
            else
                return null,
            sdl.c.SDL_SCANCODE_SPACE => return if (modifiers == 0) .spacebar else null,
            sdl.c.SDL_SCANCODE_BACKSPACE => return if (modifiers == 0) .backspace else null,
            sdl.c.SDL_SCANCODE_RETURN => return if (modifiers == 0) .enter else null,
            sdl.c.SDL_SCANCODE_ESCAPE => return if (modifiers == 0) .escape else null,
            sdl.c.SDL_SCANCODE_LEFTBRACKET => if (modifiers == shift)
                return .unbeat_level
            else if (modifiers == shift | ctrl)
                return .unbeat_level_5
            else
                return null,
            sdl.c.SDL_SCANCODE_RIGHTBRACKET => if (modifiers == shift)
                return .beat_level
            else if (modifiers == shift | ctrl)
                return .beat_level_5
            else
                return null,
            sdl.c.SDL_SCANCODE_PAGEUP => return if (modifiers == 0) .page_up else null,
            sdl.c.SDL_SCANCODE_PAGEDOWN => return if (modifiers == 0) .page_down else null,
            sdl.c.SDL_SCANCODE_HOME => return if (modifiers == 0) .home else null,
            sdl.c.SDL_SCANCODE_END => return if (modifiers == 0) .end else null,
            else => return null,
        }
    }

    const Modifiers = u4;
    const shift = 1;
    const ctrl = 2;
    const alt = 3;
    const meta = 4;

    fn getModifiers(_: InputEngine) Modifiers {
        const sdl_modifiers = sdl.c.SDL_GetModState();
        var result: Modifiers = 0;
        if (sdl_modifiers & (sdl.c.KMOD_LSHIFT | sdl.c.KMOD_RSHIFT) != 0) result |= shift;
        if (sdl_modifiers & (sdl.c.KMOD_LCTRL | sdl.c.KMOD_RCTRL) != 0) result |= ctrl;
        if (sdl_modifiers & (sdl.c.KMOD_LALT | sdl.c.KMOD_RALT) != 0) result |= alt;
        if (sdl_modifiers & (sdl.c.KMOD_LGUI | sdl.c.KMOD_RGUI) != 0) result |= meta;

        return result;
    }
};
