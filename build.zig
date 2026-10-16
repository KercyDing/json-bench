const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const serde = b.dependency("serde", .{ .target = target, .optimize = optimize });
    const jsonz = b.dependency("jsonz", .{ .target = target, .optimize = optimize });
    const json_lalinsky = b.dependency("json_lalinsky", .{ .target = target, .optimize = optimize });

    addBenchmark(b, target, optimize, "jsonz_bench", "zig/jsonz.zig", &.{
        .{ .name = "jsonz", .module = jsonz.module("jsonz") },
    });
    addBenchmark(b, target, optimize, "serde_bench", "zig/serde.zig", &.{
        .{ .name = "serde", .module = serde.module("serde") },
    });
    addBenchmark(b, target, optimize, "std_bench", "zig/std.zig", &.{});
    addBenchmark(b, target, optimize, "json_lalinsky_bench", "zig/json_lalinsky.zig", &.{
        .{ .name = "json_lalinsky", .module = json_lalinsky.module("json") },
    });
}

fn addBenchmark(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    name: []const u8,
    root: []const u8,
    imports: []const std.Build.Module.Import,
) void {
    const module = b.createModule(.{
        .root_source_file = b.path(root),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
        .imports = imports,
    });
    const executable = b.addExecutable(.{ .name = name, .root_module = module });
    executable.use_llvm = true;
    b.installArtifact(executable);
}
