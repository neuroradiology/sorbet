cc_library(
    name = "com_github_ludocode_mpack",
    srcs = glob([
        "src/mpack/*.c",
        "src/mpack/*.h",
    ]),
    hdrs = glob([
        "src/mpack/*.h",
    ]),
    includes = [
        "src",
    ],
    linkstatic = select({
        "@com_stripe_ruby_typer//tools/config:linkshared": 0,
        "//conditions:default": 1,
    }),
    visibility = ["//visibility:public"],
)
