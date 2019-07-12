const std = @import("std");
const ArrayList = std.ArrayList;
// TODO: change to "core" when we dependencies untangled
const core = @import("../index.zig");
const Coord = core.geometry.Coord;
const sign = core.geometry.sign;
const GameEngine = @import("./game_engine.zig").GameEngine;
const GameState = @import("./game_engine.zig").GameState;
const IdMap = @import("./game_engine.zig").IdMap;
const Individual = @import("./game_engine.zig").Individual;
const GameEngineClient = @import("../client/game_engine_client.zig").GameEngineClient;
const Socket = core.protocol.Socket;
const Request = core.protocol.Request;
const Response = core.protocol.Response;
const Action = core.protocol.Action;
const Event = core.protocol.Event;
const PerceivedHappening = core.protocol.PerceivedHappening;
const PerceivedFrame = core.protocol.PerceivedFrame;
const StaticPerception = core.protocol.StaticPerception;

const StateDiff = @import("./game_engine.zig").StateDiff;
const HistoryList = std.TailQueue([]StateDiff);
const HistoryNode = HistoryList.Node;

const allocator = std.heap.c_allocator;

pub fn server_main(main_player_socket: *Socket) !void {
    core.debug.nameThisThread("core");
    defer core.debug.unnameThisThread();
    core.debug.thread_lifecycle.print("init");
    defer core.debug.thread_lifecycle.print("shutdown");

    var game_engine: GameEngine = undefined;
    game_engine.init(allocator);
    var game_state = GameState.init(allocator);

    // create ai clients
    var ai_clients = IdMap(*GameEngineClient).init(allocator);
    const main_player_id: u32 = 1;
    var you_are_alive = true;
    {
        const happenings = try game_engine.getStartGameHappenings();
        try game_state.applyStateChanges(happenings.state_changes);
        for (happenings.state_changes) |diff| {
            switch (diff) {
                .spawn => |individual| {
                    if (individual.id == main_player_id) continue;
                    try handleSpawn(&ai_clients, individual);
                },
                else => {},
            }
        }
    }
    // Welcome to swarkland!
    try main_player_socket.out().write(Response{ .load_state = try game_engine.getStaticPerception(game_state, main_player_id) });

    var response_for_ais = IdMap(Response).init(allocator);
    var history = HistoryList.init();

    // start main loop
    mainLoop: while (true) {
        var actions = IdMap(Action).init(allocator);

        // do ai
        {
            var iterator = ai_clients.iterator();
            while (iterator.next()) |kv| {
                const id = kv.key;
                const response = response_for_ais.getValue(id) orelse Response{ .load_state = try game_engine.getStaticPerception(game_state, id) };
                try actions.putNoClobber(id, doAi(response));
            }
        }
        response_for_ais.clear();

        // read all the inputs, which will block for the human client.
        var is_rewind = false;
        {
            retryRead: while (true) {
                switch (try main_player_socket.in(allocator).read(Request)) {
                    .quit => {
                        core.debug.thread_lifecycle.print("clean shutdown. close");
                        // shutdown the main player
                        main_player_socket.close(Response.game_over);
                        break :mainLoop;
                    },
                    .act => |action| {
                        if (!you_are_alive) {
                            // no. you're are dead.
                            try main_player_socket.out().write(Response.reject_request);
                            continue :retryRead;
                        }
                        std.debug.assert(game_engine.validateAction(action));
                        try actions.putNoClobber(main_player_id, action);
                    },
                    .rewind => {
                        // delay actually rewinding so that we receive all requests.
                        is_rewind = true;
                    },
                }
                break;
            }
        }

        if (is_rewind) {

            // Time goes backward.
            if (rewind(&history)) |state_changes| {
                try game_state.undoStateChanges(state_changes);
                for (state_changes) |_, i| {
                    const diff = state_changes[state_changes.len - 1 - i];
                    switch (diff) {
                        .despawn => |individual| {
                            if (individual.id == main_player_id) {
                                you_are_alive = true;
                            } else {
                                try handleSpawn(&ai_clients, individual);
                            }
                        },
                        .spawn => |individual| {
                            if (individual.id == main_player_id) {
                                @panic("you can't unspawn midgame");
                            } else {
                                handleDespawn(&ai_clients, individual.id);
                            }
                        },
                        else => {},
                    }
                }
            }
            // Even if we didn't actually do a rewind, send a response to keep the communication in sync.
            try main_player_socket.out().write(Response{ .load_state = try game_engine.getStaticPerception(game_state, main_player_id) });
        } else {

            // Time goes forward.
            const happenings = try game_engine.computeHappenings(game_state, actions);
            core.debug.happening.deepPrint("happenings: ", happenings);
            try pushHistoryRecord(&history, happenings.state_changes);
            try game_state.applyStateChanges(happenings.state_changes);
            for (happenings.state_changes) |diff| {
                switch (diff) {
                    .spawn => |individual| {
                        if (individual.id == main_player_id) {
                            @panic("you can't spawn mid game");
                        } else {
                            try handleSpawn(&ai_clients, individual);
                        }
                    },
                    .despawn => |individual| {
                        if (individual.id == main_player_id) {
                            you_are_alive = false;
                        } else {
                            handleDespawn(&ai_clients, individual.id);
                        }
                    },
                    else => {},
                }
            }

            var iterator = happenings.individual_to_perception.iterator();
            while (iterator.next()) |kv| {
                const id = kv.key;
                const response = Response{
                    .stuff_happens = PerceivedHappening{
                        .frames = kv.value,
                        .static_perception = try game_engine.getStaticPerception(game_state, id),
                    },
                };
                if (id == main_player_id) {
                    try main_player_socket.out().write(response);
                } else {
                    try response_for_ais.putNoClobber(id, response);
                }
            }
        }
    }
}

fn rewind(history: *HistoryList) ?[]StateDiff {
    const node = history.pop() orelse return null;
    return node.data;
}

fn pushHistoryRecord(history: *HistoryList, state_changes: []StateDiff) !void {
    const history_node: *HistoryNode = try allocator.create(HistoryNode);
    history_node.data = state_changes;
    history.append(history_node);
}

fn handleSpawn(ai_clients: *IdMap(*GameEngineClient), individual: Individual) !void {
    // initialize ai socket
    var client = try allocator.create(GameEngineClient);
    client.startAsAi(individual.id);
    try ai_clients.putNoClobber(individual.id, client);
}

fn handleDespawn(ai_clients: *IdMap(*GameEngineClient), id: u32) void {
    ai_clients.removeAssertDiscard(id);
}

fn doAi(response: Response) Action {
    // This should be sitting waiting for us already, since we just wrote it earlier.
    var static_perception = switch (response) {
        .load_state => |static_perception| static_perception,
        .stuff_happens => |perceived_happening| perceived_happening.static_perception,
        else => @panic("unexpected response type in AI"),
    };
    return getNaiveAiDecision(static_perception);
}

fn getNaiveAiDecision(static_perception: StaticPerception) Action {
    const self_position = static_perception.self.?.abs_position;
    const target_position = blk: {
        // KILLKILLKILL HUMANS
        for (static_perception.others) |other| {
            if (other.species == .human) break :blk other.abs_position;
        }
        // no human? kill each other then!
        for (static_perception.others) |other| break :blk other.abs_position;
        // i'm the last one? dance!
        return Action{ .move = Coord{ .x = 0, .y = 1 } };
    };

    const delta = target_position.minus(self_position);
    std.debug.assert(!(delta.x == 0 and delta.y == 0));
    const range = core.game_logic.getAttackRange(static_perception.self.?.species);

    if (delta.x * delta.y == 0) {
        // straight shot
        if (delta.x * delta.x + delta.y * delta.y <= range * range) {
            // within range
            return Action{ .attack = Coord{ .x = sign(delta.x), .y = sign(delta.y) } };
        } else {
            // move straight twoard the target, even if someone else is in the way
            return Action{ .move = Coord{ .x = sign(delta.x), .y = sign(delta.y) } };
        }
    }
    // We have a diagonal space to traverse.
    // We need to choose between the long leg of the rectangle and the short leg.
    const options = [_]Coord{
        Coord{ .x = sign(delta.x), .y = 0 },
        Coord{ .x = 0, .y = sign(delta.y) },
    };
    const long_index = blk: {
        if (delta.x * delta.x > delta.y * delta.y) {
            // x is longer
            break :blk usize(0);
        } else if (delta.x * delta.x < delta.y * delta.y) {
            // y is longer
            break :blk usize(1);
        } else {
            // exactly diagonal. let's say that clockwise is longer.
            break :blk @boolToInt(delta.x != delta.y);
        }
    };
    // Archers want to line up for a shot; melee wants to avoid lining up for a shot.
    var option_index = if (range == 1) long_index else 1 - long_index;
    // If someone's in the way, then prefer the other way.
    // If someone's in the way in both directions, then go with our initial preference.
    {
        var flip_flop_counter = usize(0);
        while (flip_flop_counter < 2) : (flip_flop_counter += 1) {
            const move_into_position = self_position.plus(options[option_index]);
            for (static_perception.others) |perceived_other| {
                if (perceived_other.abs_position.equals(move_into_position)) {
                    // somebody's there already.
                    option_index = 1 - option_index;
                    break;
                }
            }
        }
    }
    return Action{ .move = options[option_index] };
}
