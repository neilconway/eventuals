"""Loads extra repos required for grpc."""

load("@rules_proto_grpc//:repositories.bzl", "rules_proto_grpc_repos", "rules_proto_grpc_toolchains")

def grpc_repos():
    rules_proto_grpc_toolchains()
    rules_proto_grpc_repos()
