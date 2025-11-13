const std = @import("std");

pub fn build(b: *std.Build) !void {
    const lib_mod = b.createModule(.{
        .link_libcpp = true,
        .target = b.resolveTargetQuery(.{
            .cpu_arch = .wasm32,
            .os_tag = .wasi,
        }),
        .optimize = .ReleaseSmall,
    });
    lib_mod.addCSourceFile(.{
        .language = .cpp,
        .file = b.path("src/main.cpp"),
        .flags = &.{
            "-std=c++23",
        },
    });
    lib_mod.addIncludePath(b.path("src"));

    const cart = b.addExecutable(.{
        .name = "loderunner",
        .root_module = lib_mod,
    });
    cart.wasi_exec_model = .reactor;
    cart.entry = .disabled;
    cart.import_memory = true;

    cart.initial_memory = 65536;
    cart.max_memory = 65536;
    cart.stack_size = 14752;

    b.installArtifact(cart);

    const run_cmd = b.addSystemCommand(&.{
        "w4",
        "run-native",
        "zig-out/bin/loderunner.wasm",
    });
    run_cmd.step.dependOn(b.getInstallStep());

    const run_step = b.step("run", "Run the cart");
    run_step.dependOn(&run_cmd.step);
}
