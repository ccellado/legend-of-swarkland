const std = @import("std");
const ArrayList = std.ArrayList;
const HashMap = std.HashMap;
const core = @import("../index.zig");
const Coord = core.geometry.Coord;
const isCardinalDirection = core.geometry.isCardinalDirection;
const makeCoord = core.geometry.makeCoord;
const zero_vector = makeCoord(0, 0);

const Action = core.protocol.Action;
const Species = core.protocol.Species;
const Floor = core.protocol.Floor;
const Wall = core.protocol.Wall;
const PerceivedFrame = core.protocol.PerceivedFrame;
const PerceivedThing = core.protocol.PerceivedThing;
const PerceivedActivity = core.protocol.PerceivedActivity;
const TerrainSpace = core.protocol.TerrainSpace;

const view_distance = core.game_logic.view_distance;

/// an "id" is a strictly server-side concept.
pub fn IdMap(comptime V: type) type {
    return HashMap(u32, V, core.geometry.hashU32, std.hash_map.getTrivialEqlFn(u32));
}

const empty_id_to_coord_map = IdMap(Coord).init(std.debug.failing_allocator);

pub fn CoordMap(comptime V: type) type {
    return HashMap(Coord, V, Coord.hash, Coord.equals);
}

const Terrain = core.matrix.Matrix(TerrainSpace);
const oob_terrain = TerrainSpace{
    .floor = .unknown,
    .wall = .stone,
};

/// Allocates and then calls `init(allocator)` on the new object.
pub fn createInit(allocator: *std.mem.Allocator, comptime T: type) !*T {
    var x = try allocator.create(T);
    x.* = T.init(allocator);
    return x;
}

/// Shallow copies the argument to a newly allocated pointer.
fn allocClone(allocator: *std.mem.Allocator, obj: var) !*@typeOf(obj) {
    var x = try allocator.create(@typeOf(obj));
    x.* = obj;
    return x;
}

const Level = struct {
    width: u16,
    height: u16,
    hatch_positions: []const Coord,
    lava_positions: []const Coord,
    individuals: []const Individual,
};
const the_levels = [_]Level{
    Level{
        .width = 10,
        .height = 10,
        .hatch_positions = [_]Coord{},
        .lava_positions = [_]Coord{},
        .individuals = [_]Individual{Individual{ .id = 0, .abs_position = makeCoord(2, 2), .species = .orc }},
    },
    Level{
        .width = 10,
        .height = 10,
        .hatch_positions = [_]Coord{makeCoord(4, 4)},
        .lava_positions = [_]Coord{},
        .individuals = [_]Individual{
            Individual{ .id = 0, .abs_position = makeCoord(1, 1), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(7, 1), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(4, 7), .species = .orc },
        },
    },
    Level{
        .width = 10,
        .height = 10,
        .hatch_positions = [_]Coord{makeCoord(7, 5)},
        .lava_positions = [_]Coord{
            makeCoord(4, 4), makeCoord(4, 5), makeCoord(4, 6),
            makeCoord(5, 4), makeCoord(5, 5), makeCoord(5, 6),
        },
        .individuals = [_]Individual{
            Individual{ .id = 0, .abs_position = makeCoord(7, 3), .species = .turtle },
            Individual{ .id = 0, .abs_position = makeCoord(2, 5), .species = .turtle },
        },
    },
    Level{
        .width = 14,
        .height = 10,
        .hatch_positions = [_]Coord{makeCoord(6, 5)},
        .lava_positions = [_]Coord{},
        .individuals = [_]Individual{
            Individual{ .id = 0, .abs_position = makeCoord(1, 5), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(2, 5), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(3, 5), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(4, 5), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(4, 4), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(4, 6), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(6, 6), .species = .orc },
        },
    },
    Level{
        .width = 8,
        .height = 5,
        .hatch_positions = [_]Coord{makeCoord(1, 2)},
        .lava_positions = [_]Coord{},
        .individuals = [_]Individual{
            Individual{ .id = 0, .abs_position = makeCoord(6, 1), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(6, 2), .species = .centaur },
            Individual{ .id = 0, .abs_position = makeCoord(6, 3), .species = .orc },
        },
    },
    Level{
        .width = 11,
        .height = 8,
        .hatch_positions = [_]Coord{makeCoord(4, 2)},
        .lava_positions = [_]Coord{},
        .individuals = [_]Individual{
            Individual{ .id = 0, .abs_position = makeCoord(1, 1), .species = .centaur },
            Individual{ .id = 0, .abs_position = makeCoord(9, 6), .species = .centaur },
        },
    },
    Level{
        .width = 15,
        .height = 10,
        .hatch_positions = [_]Coord{makeCoord(7, 2)},
        .lava_positions = [_]Coord{},
        .individuals = [_]Individual{
            Individual{ .id = 0, .abs_position = makeCoord(4, 7), .species = .centaur },
            Individual{ .id = 0, .abs_position = makeCoord(5, 7), .species = .centaur },
            Individual{ .id = 0, .abs_position = makeCoord(6, 7), .species = .centaur },
            Individual{ .id = 0, .abs_position = makeCoord(7, 7), .species = .centaur },
            Individual{ .id = 0, .abs_position = makeCoord(8, 7), .species = .centaur },
            Individual{ .id = 0, .abs_position = makeCoord(9, 7), .species = .centaur },
            Individual{ .id = 0, .abs_position = makeCoord(10, 7), .species = .centaur },
        },
    },
    Level{
        .width = 15,
        .height = 13,
        .hatch_positions = [_]Coord{makeCoord(7, 7)},
        .lava_positions = [_]Coord{},
        .individuals = [_]Individual{
            Individual{ .id = 0, .abs_position = makeCoord(5, 5), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(5, 6), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(5, 7), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(5, 8), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(5, 9), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(9, 5), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(9, 6), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(9, 7), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(9, 8), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(9, 9), .species = .orc },
            Individual{ .id = 0, .abs_position = makeCoord(4, 2), .species = .centaur },
            Individual{ .id = 0, .abs_position = makeCoord(10, 2), .species = .centaur },
            Individual{ .id = 0, .abs_position = makeCoord(9, 1), .species = .centaur },
            Individual{ .id = 0, .abs_position = makeCoord(5, 1), .species = .centaur },
        },
    },
    // the last level must have no enemies so that you can't win it.
    Level{
        .width = 15,
        .height = 10,
        .hatch_positions = [_]Coord{
            makeCoord(2, 2), makeCoord(3, 3), makeCoord(4, 2), makeCoord(3, 4),
        } ++ [_]Coord{
            makeCoord(7, 2), makeCoord(8, 3), makeCoord(7, 4), makeCoord(6, 3),
        } ++ [_]Coord{
            makeCoord(10, 2), makeCoord(10, 3), makeCoord(11, 4), makeCoord(12, 4), makeCoord(12, 3), makeCoord(12, 2),
        } ++ [_]Coord{
            makeCoord(2, 6), makeCoord(2, 7), makeCoord(3, 8), makeCoord(4, 7), makeCoord(5, 8), makeCoord(6, 7), makeCoord(6, 6),
        } ++ [_]Coord{
            makeCoord(8, 6), makeCoord(8, 7), makeCoord(8, 8),
        } ++ [_]Coord{
            makeCoord(10, 6), makeCoord(10, 7), makeCoord(10, 8), makeCoord(11, 6), makeCoord(12, 7), makeCoord(12, 8),
        },
        .lava_positions = [_]Coord{},
        .individuals = [_]Individual{},
    },
};

fn buildTheTerrain(allocator: *std.mem.Allocator) !Terrain {
    var width = u16(0);
    var height = u16(1);
    for (the_levels) |level| {
        width += level.width;
        height = std.math.max(height, level.height);
    }

    var terrain = try Terrain.initFill(allocator, width, height, TerrainSpace{
        .floor = .dirt,
        .wall = .air,
    });
    const border_wall = TerrainSpace{
        .floor = .unknown,
        .wall = .stone,
    };

    var level_x = u16(0);
    for (the_levels) |level| {
        defer level_x += level.width;
        {
            var x: u16 = 0;
            while (x < level.width) : (x += 1) {
                terrain.atUnchecked(level_x + x, 0).* = border_wall;
                var y: u16 = level.height - 1;
                while (y < height) : (y += 1) {
                    terrain.atUnchecked(level_x + x, y).* = border_wall;
                }
            }
        }
        {
            var y: u16 = 1;
            while (y < level.height - 1) : (y += 1) {
                terrain.atUnchecked(level_x + 0, y).* = border_wall;
                terrain.atUnchecked(level_x + level.width - 1, y).* = border_wall;
            }
        }
        for (level.hatch_positions) |coord| {
            terrain.at(level_x + @intCast(u16, coord.x), coord.y).?.floor = .hatch;
        }
        for (level.lava_positions) |coord| {
            terrain.at(level_x + @intCast(u16, coord.x), coord.y).?.floor = .lava;
        }
    }
    return terrain;
}

fn assignId(individual: Individual, level_x: i32, id: u32) Individual {
    var ret = individual;
    ret.abs_position.x += level_x;
    ret.id = id;
    return ret;
}
fn findAvailableId(cursor: *u32, usedIds: IdMap(*Individual)) u32 {
    while (usedIds.contains(cursor.*)) {
        cursor.* += 1;
    }
    defer cursor.* += 1;
    return cursor.*;
}

// TODO: sort all arrays to hide iteration order from the server
pub const GameEngine = struct {
    allocator: *std.mem.Allocator,

    pub fn init(self: *GameEngine, allocator: *std.mem.Allocator) void {
        self.* = GameEngine{ .allocator = allocator };
    }

    pub fn validateAction(self: *const GameEngine, action: Action) bool {
        switch (action) {
            .wait => return true,
            .move => |direction| return isCardinalDirection(direction),
            .attack => |direction| return isCardinalDirection(direction),
        }
    }

    pub fn getStartGameHappenings(self: *const GameEngine) !Happenings {
        return Happenings{
            .individual_to_perception = IdMap([]PerceivedFrame).init(self.allocator),
            .state_changes = blk: {
                var ret = ArrayList(StateDiff).init(self.allocator);
                // human is always id 1
                try ret.append(StateDiff{ .spawn = Individual{ .id = 1, .abs_position = makeCoord(7, 7), .species = .human } });
                const level_x = 0;
                for (the_levels[0].individuals) |individual, i| {
                    try ret.append(StateDiff{ .spawn = assignId(individual, level_x, @intCast(u32, i) + 2) });
                }
                try ret.append(StateDiff{
                    .terrain_init = try buildTheTerrain(self.allocator),
                });
                break :blk ret.toOwnedSlice();
            },
        };
    }

    pub const Happenings = struct {
        individual_to_perception: IdMap([]PerceivedFrame),
        state_changes: []StateDiff,
    };

    /// Computes what would happen to the state of the game.
    /// The game_state object passed in should be distroyed and forgotten after this function returns.
    /// This is the entry point for all game rules.
    pub fn computeHappenings(self: *const GameEngine, game_state: *GameState, actions: IdMap(Action)) !Happenings {
        // cache the set of keys so iterator is easier.
        const everybody = try self.allocator.alloc(u32, game_state.individuals.count());
        {
            var iterator = game_state.individuals.iterator();
            for (everybody) |*x| {
                x.* = iterator.next().?.key;
            }
            std.debug.assert(iterator.next() == null);
        }

        var individual_to_perception = IdMap(*MutablePerceivedHappening).init(self.allocator);

        var moves_history = ArrayList(*IdMap(Coord)).init(self.allocator);
        try moves_history.append(try createInit(self.allocator, IdMap(Coord)));
        try moves_history.append(try createInit(self.allocator, IdMap(Coord)));
        var next_moves = moves_history.at(moves_history.len - 1);
        var previous_moves = moves_history.at(moves_history.len - 2);

        var positions_history = ArrayList(*IdMap(Coord)).init(self.allocator);
        try positions_history.append(try createInit(self.allocator, IdMap(Coord)));
        try positions_history.append(try createInit(self.allocator, IdMap(Coord)));
        var next_positions = positions_history.at(positions_history.len - 1);
        var current_positions = positions_history.at(positions_history.len - 2);

        for (everybody) |id| {
            try individual_to_perception.putNoClobber(id, try createInit(self.allocator, MutablePerceivedHappening));
            try current_positions.putNoClobber(id, game_state.individuals.getValue(id).?.abs_position);
        }

        var attacks = IdMap(Coord).init(self.allocator);

        for (everybody) |id| {
            var actor = game_state.individuals.getValue(id).?;
            switch (actions.getValue(id).?) {
                .wait => {},
                .move => |direction| {
                    try next_moves.putNoClobber(id, direction);
                },
                .attack => |direction| {
                    try attacks.putNoClobber(id, direction);
                },
            }
        }

        while (true) {
            for (everybody) |id| {
                try self.observeFrame(
                    game_state,
                    id,
                    individual_to_perception.getValue(id).?,
                    current_positions,
                    Activities{
                        .movement = Activities.Movement{
                            .previous_moves = previous_moves,
                            .next_moves = next_moves,
                        },
                    },
                );
            }

            if (next_moves.count() == 0) break;

            for (everybody) |id| {
                try next_positions.putNoClobber(id, current_positions.getValue(id).?.plus(next_moves.getValue(id) orelse zero_vector));
            }

            try moves_history.append(try createInit(self.allocator, IdMap(Coord)));
            next_moves = moves_history.at(moves_history.len - 1);
            previous_moves = moves_history.at(moves_history.len - 2);

            try positions_history.append(try createInit(self.allocator, IdMap(Coord)));
            next_positions = positions_history.at(positions_history.len - 1);
            current_positions = positions_history.at(positions_history.len - 2);

            // ==================================
            // Collision detection and resolution
            // ==================================
            var collision_counter = CoordMap(usize).init(self.allocator);

            for (everybody) |id| {
                const position = current_positions.getValue(id).?;
                // walls
                if (!core.game_logic.isOpenSpace((game_state.terrain.getCoord(position) orelse oob_terrain).wall)) {
                    // bounce off the wall
                    try next_moves.putNoClobber(id, previous_moves.getValue(id).?.negated());
                }
                // and count entity collision
                _ = try collision_counter.put(position, 1 + (collision_counter.getValue(position) orelse 0));
            }

            {
                var ids = ArrayList(u32).init(self.allocator);
                var iterator = collision_counter.iterator();
                while (iterator.next()) |kv| {
                    if (kv.value <= 1) continue;
                    const position = kv.key;
                    // collect the individuals involved in this collision
                    ids.shrink(0);
                    for (everybody) |id| {
                        if (!current_positions.getValue(id).?.equals(position)) continue;
                        try ids.append(id);
                    }

                    // treat each individual separately
                    for (ids.toSliceConst()) |me| {
                        // consider forces from everyone but yourself
                        var external_force: u4 = 0;
                        for (ids.toSliceConst()) |id| {
                            if (id == me) continue;
                            const prior_velocity = previous_moves.getValue(id) orelse zero_vector;
                            external_force |= core.geometry.directionToCardinalBitmask(prior_velocity);
                        }
                        if (core.geometry.cardinalBitmaskToDirection(external_force)) |push_velocity| {
                            try next_moves.putNoClobber(me, push_velocity);
                        } else {
                            // clusterfuck. reverse course.
                            try next_moves.putNoClobber(me, (previous_moves.getValue(me) orelse zero_vector).negated());
                        }
                    }
                }
            }
        }

        // Attacks
        var deaths = IdMap(void).init(self.allocator);
        for (everybody) |id| {
            var attack_direction = attacks.getValue(id) orelse continue;
            var attacker_position = current_positions.getValue(id).?;
            var attack_distance: i32 = 1;
            const range = core.game_logic.getAttackRange(game_state.individuals.getValue(id).?.species);
            while (attack_distance <= range) : (attack_distance += 1) {
                var damage_position = attacker_position.plus(attack_direction.scaled(attack_distance));
                for (everybody) |other_id| {
                    if (!current_positions.getValue(other_id).?.equals(damage_position)) continue;
                    if (!core.game_logic.isAffectedByAttacks(game_state.individuals.getValue(other_id).?.species)) continue;
                    _ = try deaths.put(other_id, {});
                }
            }
        }
        // Lava
        for (everybody) |id| {
            if ((game_state.terrain.getCoord(current_positions.getValue(id).?) orelse oob_terrain).floor == .lava) {
                _ = try deaths.put(id, {});
            }
        }
        // Perception of Attacks and Death
        for (everybody) |id| {
            if (attacks.count() != 0) {
                try self.observeFrame(
                    game_state,
                    id,
                    individual_to_perception.getValue(id).?,
                    current_positions,
                    Activities{ .attacks = &attacks },
                );
            }
            if (deaths.count() != 0) {
                try self.observeFrame(
                    game_state,
                    id,
                    individual_to_perception.getValue(id).?,
                    current_positions,
                    Activities{ .deaths = &deaths },
                );
            }
        }

        var open_the_way = false;
        var button_getting_pressed: ?Coord = null;
        if (game_state.individuals.count() - deaths.count() <= 1) {
            // Only one person left. You win!
            if (deaths.count() > 0) {
                // Spawn the stairs onward.
                open_the_way = true;
            }
            // check for someone on the button
            for (everybody) |id| {
                if (deaths.contains(id)) continue;
                const coord = current_positions.getValue(id).?;
                if ((game_state.terrain.getCoord(coord) orelse oob_terrain).floor == .hatch) {
                    button_getting_pressed = coord;
                    break;
                }
            }
        }

        var new_id_cursor: u32 = @intCast(u32, game_state.individuals.count());

        // build state changes
        var state_changes = ArrayList(StateDiff).init(self.allocator);
        for (everybody) |id| {
            const from = game_state.individuals.getValue(id).?.abs_position;
            const to = current_positions.getValue(id).?;
            if (to.equals(from)) continue;
            const delta = to.minus(from);
            try state_changes.append(StateDiff{
                .move = StateDiff.IdAndCoord{
                    .id = id,
                    .coord = delta,
                },
            });
        }
        {
            var iterator = deaths.iterator();
            while (iterator.next()) |kv| {
                try state_changes.append(StateDiff{
                    .despawn = blk: {
                        var individual = game_state.individuals.getValue(kv.key).?.*;
                        individual.abs_position = current_positions.getValue(individual.id).?;
                        break :blk individual;
                    },
                });
            }
        }

        if (open_the_way and game_state.level_number + 1 < the_levels.len) {
            var level_x = u16(0);
            for (the_levels[0..game_state.level_number]) |level| {
                level_x += level.width;
            }
            const level = the_levels[game_state.level_number];
            const the_way_y = the_levels[game_state.level_number + 1].hatch_positions[0].y;
            for ([_]Coord{
                makeCoord(level_x + level.width - 1, the_way_y),
                makeCoord(level_x + level.width - 0, the_way_y),
            }) |coord| {
                try state_changes.append(StateDiff{
                    .terrain_update = StateDiff.TerrainDiff{
                        .at = coord,
                        .from = game_state.terrain.getCoord(coord).?,
                        .to = TerrainSpace{
                            .floor = .dirt,
                            .wall = .air,
                        },
                    },
                });
            }
        }

        if (button_getting_pressed) |button_coord| {
            const new_level_number = blk: {
                if (game_state.level_number + 1 < the_levels.len) {
                    try state_changes.append(StateDiff.transition_to_next_level);
                    break :blk game_state.level_number + 1;
                } else {
                    break :blk game_state.level_number;
                }
            };
            var level_x = u16(0);
            for (the_levels[0..new_level_number]) |level| {
                level_x += level.width;
            }
            // close the way
            const the_way_y = the_levels[new_level_number].hatch_positions[0].y;
            for ([_]Coord{
                makeCoord(level_x - 1, the_way_y),
                makeCoord(level_x + 0, the_way_y),
            }) |coord| {
                try state_changes.append(StateDiff{
                    .terrain_update = StateDiff.TerrainDiff{
                        .at = coord,
                        .from = game_state.terrain.getCoord(coord).?,
                        .to = TerrainSpace{
                            .floor = .unknown,
                            .wall = .stone,
                        },
                    },
                });
            }
            // destroy the button
            try state_changes.append(StateDiff{
                .terrain_update = StateDiff.TerrainDiff{
                    .at = button_coord,
                    .from = game_state.terrain.getCoord(button_coord).?,
                    .to = TerrainSpace{
                        .floor = .dirt,
                        .wall = .air,
                    },
                },
            });
            // spawn enemies
            for (the_levels[new_level_number].individuals) |individual| {
                const id = findAvailableId(&new_id_cursor, game_state.individuals);
                try state_changes.append(StateDiff{ .spawn = assignId(individual, level_x, id) });
            }
        }

        // final observations
        try game_state.applyStateChanges(state_changes.toSliceConst());
        current_positions.clear();
        for (everybody) |id| {
            if (!deaths.contains(id)) {
                try self.observeFrame(
                    game_state,
                    id,
                    individual_to_perception.getValue(id).?,
                    current_positions,
                    Activities.static_state,
                );
            }
        }

        return Happenings{
            .individual_to_perception = blk: {
                var ret = IdMap([]PerceivedFrame).init(self.allocator);
                for (everybody) |id| {
                    var frame_list = individual_to_perception.getValue(id).?.frames;
                    // remove empty frames, except the last one
                    var i: usize = 0;
                    frameLoop: while (i + 1 < frame_list.len) : (i +%= 1) {
                        const frame = frame_list.at(i);
                        if (frame.self.activity != PerceivedActivity.none) continue :frameLoop;
                        for (frame.others) |other| {
                            if (other.activity != PerceivedActivity.none) continue :frameLoop;
                        }
                        // delete this frame
                        _ = frame_list.orderedRemove(i);
                        i -%= 1;
                    }
                    try ret.putNoClobber(id, frame_list.toOwnedSlice());
                }
                break :blk ret;
            },
            .state_changes = state_changes.toOwnedSlice(),
        };
    }

    const Activities = union(enum) {
        static_state,
        movement: Movement,
        const Movement = struct {
            previous_moves: *const IdMap(Coord),
            next_moves: *const IdMap(Coord),
        };

        attacks: *const IdMap(Coord),
        deaths: *const IdMap(void),
    };
    fn observeFrame(
        self: *const GameEngine,
        game_state: *const GameState,
        my_id: u32,
        perception: *MutablePerceivedHappening,
        current_positions: *const IdMap(Coord),
        activities: Activities,
    ) !void {
        try perception.frames.append(try getPerceivedFrame(
            self,
            game_state,
            my_id,
            current_positions,
            activities,
        ));
    }

    pub fn getStaticPerception(self: *const GameEngine, game_state: GameState, individual_id: u32) !PerceivedFrame {
        return getPerceivedFrame(
            self,
            &game_state,
            individual_id,
            &empty_id_to_coord_map,
            Activities.static_state,
        );
    }

    fn getPerceivedFrame(
        self: *const GameEngine,
        game_state: *const GameState,
        my_id: u32,
        current_positions: *const IdMap(Coord),
        activities: Activities,
    ) !PerceivedFrame {
        const your_position = current_positions.getValue(my_id) orelse game_state.individuals.getValue(my_id).?.abs_position;
        var yourself: ?PerceivedThing = null;
        var others = ArrayList(PerceivedThing).init(self.allocator);

        var iterator = game_state.individuals.iterator();
        while (iterator.next()) |kv| {
            const id = kv.key;
            const activity = switch (activities) {
                .movement => |data| blk: {
                    const prior_velocity = data.previous_moves.getValue(id) orelse zero_vector;
                    const next_velocity = data.next_moves.getValue(id) orelse zero_vector;
                    if (prior_velocity.equals(zero_vector) and next_velocity.equals(zero_vector)) {
                        break :blk PerceivedActivity{ .none = {} };
                    }
                    const a = PerceivedActivity{
                        .movement = PerceivedActivity.Movement{
                            .prior_velocity = prior_velocity,
                            .next_velocity = next_velocity,
                        },
                    };
                    break :blk a;
                },

                .attacks => |data| if (data.getValue(id)) |direction|
                    PerceivedActivity{
                        .attack = PerceivedActivity.Attack{ .direction = direction },
                    }
                else
                    PerceivedActivity{ .none = {} },

                .deaths => |data| if (data.getValue(id)) |_|
                    PerceivedActivity{ .death = {} }
                else
                    PerceivedActivity{ .none = {} },

                .static_state => PerceivedActivity{ .none = {} },
            };
            const abs_position = current_positions.getValue(id) orelse game_state.individuals.getValue(id).?.abs_position;
            const delta = abs_position.minus(your_position);
            if (delta.magnitudeDiag() > view_distance) continue;
            const thing = PerceivedThing{
                .species = game_state.individuals.getValue(id).?.species,
                .rel_position = delta,
                .activity = activity,
            };
            if (id == my_id) {
                yourself = thing;
            } else {
                try others.append(thing);
            }
        }

        const view_size = view_distance * 2 + 1;
        var terrain_chunk = core.protocol.TerrainChunk{
            .rel_position = makeCoord(-view_distance, -view_distance),
            .matrix = try Terrain.initFill(self.allocator, view_size, view_size, oob_terrain),
        };
        const view_origin = your_position.minus(makeCoord(view_distance, view_distance));
        var cursor = Coord{ .x = undefined, .y = 0 };
        while (cursor.y < view_size) : (cursor.y += 1) {
            cursor.x = 0;
            while (cursor.x < view_size) : (cursor.x += 1) {
                if (game_state.terrain.getCoord(cursor.plus(view_origin))) |cell| {
                    terrain_chunk.matrix.atCoord(cursor).?.* = cell;
                }
            }
        }

        var you_win = blk: for (game_state.terrain.data) |space| {
            if (space.floor == .hatch) {
                break :blk false;
            }
        } else true;

        return PerceivedFrame{
            .self = yourself.?,
            .others = others.toOwnedSlice(),
            .terrain = terrain_chunk,
            .you_win = you_win,
        };
    }
};

pub const Individual = struct {
    id: u32,
    species: Species,
    abs_position: Coord,
};
pub const StateDiff = union(enum) {
    spawn: Individual,
    despawn: Individual,
    move: IdAndCoord,
    pub const IdAndCoord = struct {
        id: u32,
        coord: Coord,
    };

    /// can only be in the start game events. can never be undone.
    terrain_init: Terrain,

    terrain_update: TerrainDiff,
    pub const TerrainDiff = struct {
        at: Coord,
        from: TerrainSpace,
        to: TerrainSpace,
    };

    transition_to_next_level,
};

pub const GameState = struct {
    allocator: *std.mem.Allocator,
    terrain: Terrain,
    individuals: IdMap(*Individual),
    level_number: u16,

    pub fn init(allocator: *std.mem.Allocator) GameState {
        return GameState{
            .allocator = allocator,
            .terrain = Terrain.initEmpty(),
            .individuals = IdMap(*Individual).init(allocator),
            .level_number = 0,
        };
    }

    pub fn clone(self: GameState) !GameState {
        return GameState{
            .allocator = self.allocator,
            .terrain = self.terrain,
            .individuals = blk: {
                var ret = IdMap(*Individual).init(self.allocator);
                var iterator = self.individuals.iterator();
                while (iterator.next()) |kv| {
                    try ret.putNoClobber(kv.key, try allocClone(self.allocator, kv.value.*));
                }
                break :blk ret;
            },
            .level_number = self.level_number,
        };
    }

    fn applyStateChanges(self: *GameState, state_changes: []const StateDiff) !void {
        for (state_changes) |diff| {
            switch (diff) {
                .spawn => |individual| {
                    try self.individuals.putNoClobber(individual.id, try allocClone(self.allocator, individual));
                },
                .despawn => |individual| {
                    self.individuals.removeAssertDiscard(individual.id);
                },
                .move => |id_and_coord| {
                    const individual = self.individuals.getValue(id_and_coord.id).?;
                    individual.abs_position = individual.abs_position.plus(id_and_coord.coord);
                },
                .terrain_init => |terrain| {
                    self.terrain = terrain;
                },
                .terrain_update => |data| {
                    self.terrain.atCoord(data.at).?.* = data.to;
                },
                .transition_to_next_level => {
                    self.level_number += 1;
                },
            }
        }
    }
    fn undoStateChanges(self: *GameState, state_changes: []const StateDiff) !void {
        for (state_changes) |_, forwards_i| {
            // undo backwards
            const diff = state_changes[state_changes.len - 1 - forwards_i];
            switch (diff) {
                .spawn => |individual| {
                    self.individuals.removeAssertDiscard(individual.id);
                },
                .despawn => |individual| {
                    try self.individuals.putNoClobber(individual.id, try allocClone(self.allocator, individual));
                },
                .move => |id_and_coord| {
                    const individual = self.individuals.getValue(id_and_coord.id).?;
                    individual.abs_position = individual.abs_position.minus(id_and_coord.coord);
                },
                .terrain_init => {
                    @panic("can't undo terrain init");
                },
                .terrain_update => |data| {
                    self.terrain.atCoord(data.at).?.* = data.from;
                },
                .transition_to_next_level => {
                    self.level_number -= 1;
                },
            }
        }
    }
};

const MutablePerceivedHappening = struct {
    frames: ArrayList(PerceivedFrame),
    pub fn init(allocator: *std.mem.Allocator) MutablePerceivedHappening {
        return MutablePerceivedHappening{
            .frames = ArrayList(PerceivedFrame).init(allocator),
        };
    }
};
