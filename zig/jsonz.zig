const std = @import("std");
const jsonz = @import("jsonz");
const bench = @import("bench.zig");

const Adapter = struct {
    pub const name = "jsonz";
    pub const supports_arbitrary = true;
    pub const Arbitrary = jsonz.dom.Document;

    pub fn decode(comptime T: type, allocator: std.mem.Allocator, input: []const u8) !T {
        if (T == Arbitrary) return jsonz.dom.parse(allocator, input, .{});
        return jsonz.typed.parseBorrowed(T, allocator, input, .{ .ignore_unknown_fields = true });
    }

    pub fn decodeBorrowed(allocator: std.mem.Allocator, buffer: []u8, input_len: usize) !Arbitrary {
        return jsonz.dom.parseBorrowed(allocator, buffer, input_len, .{});
    }

    pub fn encode(allocator: std.mem.Allocator, value: anytype) ![]u8 {
        if (@TypeOf(value) == Arbitrary) return value.toSlice(allocator, .{});
        return jsonz.typed.toSlice(allocator, value, .{});
    }

    pub fn scan(value: *const jsonz.dom.Document) void {
        var checksum: u64 = 0;
        walk(value.root(), &checksum);
        std.mem.doNotOptimizeAway(checksum);
    }

    fn walk(node: jsonz.dom.Node, checksum: *u64) void {
        switch (node.kind()) {
            .object => {
                var members = node.objectIterator() catch unreachable;
                while (members.next()) |member| walk(member.value, checksum);
            },
            .array => {
                var elements = node.arrayIterator() catch unreachable;
                while (elements.next()) |element| walk(element, checksum);
            },
            .number => checksum.* +%= @bitCast(node.toNumber(.f64) catch unreachable),
            .string => checksum.* +%= node.asString().?.len,
            .bool, .null => checksum.* +%= 1,
        }
    }

    pub fn deinit(value: anytype) void {
        if (comptime @TypeOf(value.*) == Arbitrary) value.deinit();
    }
};

pub fn main(init: std.process.Init.Minimal) !void {
    try bench.run(Adapter, init);
}
