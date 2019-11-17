const core = @import("../index.zig");

pub const view_distance = 8;

pub fn getAttackRange(species: core.protocol.Species) i32 {
    switch (species) {
        .centaur => return 16,
        else => return 1,
    }
}

pub fn isAffectedByAttacks(species: core.protocol.Species) bool {
    return switch (species) {
        .turtle => false,
        else => true,
    };
}

pub fn isOpenSpace(wall: core.protocol.Wall) bool {
    return wall == .air;
}
