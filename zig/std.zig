const std = @import("std");
const bench = @import("bench.zig");
const shared = @import("shared.zig");

const Adapter = struct {
    pub const name = "std.json";
    pub const supports_arbitrary = true;
    pub const Arbitrary = std.json.Value;

    pub fn decode(comptime T: type, allocator: std.mem.Allocator, input: []const u8) !T {
        return std.json.parseFromSliceLeaky(T, allocator, input, .{ .ignore_unknown_fields = true });
    }
    pub fn encode(allocator: std.mem.Allocator, value: anytype) ![]u8 {
        var output: std.Io.Writer.Allocating = .init(allocator);
        errdefer output.deinit();
        try std.json.Stringify.value(value, .{}, &output.writer);
        return output.toOwnedSlice();
    }

    pub fn get(value: std.json.Value, comptime pointer: []const u8) void {
        var current = value;
        inline for (shared.pointerTokens(pointer)) |token| {
            switch (current) {
                .object => |object| current = object.get(token) orelse return,
                .array => |array| {
                    const index = std.fmt.parseInt(usize, token, 10) catch return;
                    if (index >= array.items.len) return;
                    current = array.items[index];
                },
                else => return,
            }
        }
        std.mem.doNotOptimizeAway(current);
    }
};

pub fn main(init: std.process.Init.Minimal) !void {
    try bench.run(Adapter, init);
}
