const std = @import("std");
const bench = @import("bench.zig");

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

    pub fn scan(value: *const std.json.Value) void {
        var checksum: u64 = 0;
        walk(value.*, &checksum);
        std.mem.doNotOptimizeAway(checksum);
    }

    fn walk(value: std.json.Value, checksum: *u64) void {
        switch (value) {
            .object => |object| {
                var members = object.iterator();
                while (members.next()) |member| walk(member.value_ptr.*, checksum);
            },
            .array => |array| for (array.items) |element| walk(element, checksum),
            .integer => |integer| checksum.* +%= @bitCast(@as(f64, @floatFromInt(integer))),
            .float => |float| checksum.* +%= @bitCast(float),
            .number_string => |text| checksum.* +%= text.len,
            .string => |text| checksum.* +%= text.len,
            .bool, .null => checksum.* +%= 1,
        }
    }
};

pub fn main(init: std.process.Init.Minimal) !void {
    try bench.run(Adapter, init);
}
