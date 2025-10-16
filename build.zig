const std = @import("std");
const c_src_files = [_][]const u8{
    "lib/olog.c",
};

const c_flags = [_][]const u8{
    "-Wall",
    "-Wextra",
    "-Werror",
    "-fPIC",
    "-std=c11",
};

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{ .preferred_optimize_mode = .Debug });

    const libolog = b.addLibrary(.{
        .name = "olog",
        .linkage = .dynamic,
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });

    libolog.addIncludePath(b.path("include/"));
    libolog.root_module.linkSystemLibrary("pthread", .{});
    libolog.installHeader(b.path("include/olog.h"), "olog.h");
    libolog.root_module.addCSourceFiles(.{
        .files = &c_src_files,
        .flags = &c_flags,
    });
    b.installArtifact(libolog);
}
