"""Create symlink to specific submodule path."""
load("@bazel_tools//tools/build_defs/repo:local.bzl", "local_repository")

def _submodule_repository_impl(repository_ctx):
    # Remove the need to try and get a path to 'WORKSPACE.bzlmod'
    # once we start using a bazel version that includes the following commit:
    # https://github.com/bazelbuild/bazel/commit/8edf6abec40c848a5df93647f948e31f32452ae6
    workspace_root = repository_ctx.path(Label("//:WORKSPACE.bzlmod")).dirname

    for segment in repository_ctx.attr.path.split("/"):
        workspace_root = workspace_root.get_child(segment)

    repository_ctx.symlink(workspace_root, "")

_submodule_repository = repository_rule(
    implementation = _submodule_repository_impl,
    local = True,
    attrs = {
        "path": attr.string(mandatory = True),
    },
)

def submodule_repository(name, path, external):
    if external:
        print("creating external submodule for {}, path = {}".format(name, path))
        _submodule_repository(
            name = name,
            path = path,
        )
    else:
        print("creating local submodule for {}, path = {}".format(name, path))
        local_repository(
            name = name,
            path = path,
        )
