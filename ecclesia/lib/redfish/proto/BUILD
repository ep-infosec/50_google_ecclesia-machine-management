load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")

licenses(["notice"])

proto_library(
    name = "redfish_v1_proto",
    srcs = ["redfish_v1.proto"],
    visibility = [
        "//visibility:private",  # Only private by automation, not intent. Owner may accept CLs adding visibility. See go/scheuklappen#explicit-private.
    ],
    deps = ["@com_google_protobuf//:struct_proto"],
)

cc_proto_library(
    name = "redfish_v1_cc_proto",
    visibility = ["//ecclesia:redfish_grpc_users"],
    deps = [":redfish_v1_proto"],
)

cc_grpc_library(
    name = "redfish_v1_cc_grpc_proto",
    srcs = [":redfish_v1_proto"],
    generate_mocks = True,
    grpc_only = ["True"],
    visibility = ["//ecclesia:redfish_grpc_users"],
    deps = [":redfish_v1_cc_proto"],
)
