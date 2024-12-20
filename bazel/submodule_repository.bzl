"""Create symlink to specific submodule path."""

_submodule_repository = repository_rule(
    implementation = _submodule_repository_impl,
    local = True,
    attrs = {
        "path": attr.string(mandatory = True),
    },
)

def _submodule_repository_impl(repository_ctx):
    workspace_root = repository_ctx.workspace_root

    for segment in repository_ctx.attr.path.split("/"):
        workspace_root = workspace_root.get_child(segment)

    repository_ctx.symlink(workspace_root, "")

def submodule_repository(name, path, external):
    if not external:
        native.local_repository(
            name = name,
            path = path,
        )
    else:
        _submodule_repository(
            name = name,
            path = path,
        )
