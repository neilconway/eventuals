"""Create symlink to specific submodule path."""

def _submodule_repository_impl(repository_ctx):
    # Remove the need to try and get a path to 'WORKSPACE.bazel'
    # once we start using a bazel version that includes the following commit:
    # https://github.com/bazelbuild/bazel/commit/8edf6abec40c848a5df93647f948e31f32452ae6
    workspace_root = repository_ctx.path(Label("//:WORKSPACE.bazel")).dirname

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
        _submodule_repository(
            name = name,
            path = path,
        )
    else:
        native.local_repository(
            name = name,
            path = path,
        )
