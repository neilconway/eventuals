"""Defines a local python toolchain for use in rules_python."""

load("@com_github_reboot_dev_pyprotoc_plugin//bazel:py_toolchains.bzl", pyprotoc_plugin_py_toolchains = "py_toolchains")
load("@rules_proto//proto:toolchains.bzl", "rules_proto_toolchains")

def py_toolchains(repo_mapping = {}):
    pyprotoc_plugin_py_toolchains(repo_mapping)
    rules_proto_toolchains()
