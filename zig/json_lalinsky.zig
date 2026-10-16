const std = @import("std");
const json = @import("json_lalinsky");
const bench = @import("bench.zig");

const Adapter = struct {
    pub const name = "json.zig";
    pub const supports_arbitrary = false;

    pub fn decode(comptime T: type, allocator: std.mem.Allocator, input: []const u8) !T {
        return json.decodeFromSliceLeaky(T, allocator, input, .{ .skip_unknown_fields = true });
    }

    pub fn encode(allocator: std.mem.Allocator, value: anytype) ![]u8 {
        var output: std.Io.Writer.Allocating = .init(allocator);
        errdefer output.deinit();
        try json.encode(value, &output.writer);
        return output.toOwnedSlice();
    }
};

pub fn main(init: std.process.Init.Minimal) !void {
    try bench.run(Adapter, init);
}
