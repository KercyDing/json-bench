const std = @import("std");

pub const input_allocator = std.heap.c_allocator;
pub const data_limit = 128 * 1024 * 1024;
pub const datasets = [_][]const u8{
    "canada.json", "citm_catalog.json", "fgo.json",  "github_events.json", "gsoc-2018.json",
    "lottie.json", "otfcc.json",        "poet.json", "twitter.json",       "twitterescaped.json",
    "small.json",
};

pub const TwitterUser = struct {
    id: u64,
    name: []const u8,
    screen_name: []const u8,
    location: []const u8,
    description: []const u8,
    verified: bool,
    followers_count: u64,
    friends_count: u64,
    statuses_count: ?u64,
};

pub const TwitterStatus = struct {
    created_at: []const u8,
    id: u64,
    text: []const u8,
    user: TwitterUser,
    retweet_count: u64,
    favorite_count: u64,
};

pub const TwitterDocument = struct { statuses: []const TwitterStatus };
pub const SmallDocument = struct {
    id: u64,
    ok: bool,
    name: []const u8,
    score: f64,
    tags: []const []const u8,
};
pub const CanadaCoordinate = [2]f64;
pub const CanadaGeometry = struct { type: []const u8, coordinates: []const []const CanadaCoordinate };
pub const CanadaFeature = struct { type: []const u8, properties: struct { name: []const u8 }, geometry: CanadaGeometry };
pub const CanadaDocument = struct { type: []const u8, features: []const CanadaFeature };
pub const Poem = struct { desc: []const u8, name: []const u8, id: []const u8 };
pub const GithubActor = struct { gravatar_id: []const u8, login: []const u8, avatar_url: []const u8, url: []const u8, id: u64 };
pub const GithubRepository = struct { url: []const u8, id: u64, name: []const u8 };
pub const GithubEvent = struct { type: []const u8, created_at: []const u8, actor: GithubActor, repo: GithubRepository, public: bool, id: []const u8 };

pub fn isKnownDataset(name: []const u8) bool {
    return std.mem.eql(u8, name, "small.json") or std.mem.eql(u8, name, "canada.json") or std.mem.eql(u8, name, "github_events.json") or
        std.mem.eql(u8, name, "poet.json") or std.mem.eql(u8, name, "twitter.json") or
        std.mem.eql(u8, name, "twitterescaped.json");
}

pub fn repeatCount(size: usize) usize {
    if (size <= 256) return 100_000;

    const target_bytes = 64 * 1024 * 1024;
    return if (size == 0 or size >= target_bytes) 1 else (target_bytes + size - 1) / size;
}

pub inline fn nowNanoseconds() u64 {
    return @intCast(std.Io.Clock.awake.now(std.Options.debug_io).nanoseconds);
}

/// The RFC 6901 pointer the `get` task reads for each dataset.
pub const get_paths = [_]struct { name: []const u8, path: []const u8 }{
    .{ .name = "canada.json", .path = "/features/0/geometry/coordinates/0/0" },
    .{ .name = "citm_catalog.json", .path = "/areaNames/205705993" },
    .{ .name = "fgo.json", .path = "/mstSvt/0/relateQuestIds/0" },
    .{ .name = "github_events.json", .path = "/0/actor/login" },
    .{ .name = "gsoc-2018.json", .path = "/0/name" },
    .{ .name = "lottie.json", .path = "/assets/0/layers/0/nm" },
    .{ .name = "otfcc.json", .path = "/head/version" },
    .{ .name = "poet.json", .path = "/0/name" },
    .{ .name = "twitter.json", .path = "/statuses/0/user/id" },
    .{ .name = "twitterescaped.json", .path = "/statuses/0/user/id" },
};

/// Splits a pointer into reference tokens for libraries without a pointer API.
/// The benchmark paths contain no `~0`/`~1` escapes.
pub fn pointerTokens(comptime pointer: []const u8) [pointerTokenCount(pointer)][]const u8 {
    var tokens: [pointerTokenCount(pointer)][]const u8 = undefined;
    var rest: []const u8 = pointer[1..];
    for (&tokens) |*token| {
        const end = std.mem.indexOfScalar(u8, rest, '/') orelse rest.len;
        token.* = rest[0..end];
        rest = if (end < rest.len) rest[end + 1 ..] else "";
    }
    return tokens;
}

fn pointerTokenCount(comptime pointer: []const u8) usize {
    if (pointer.len == 0) return 0;
    return std.mem.count(u8, pointer, "/");
}
