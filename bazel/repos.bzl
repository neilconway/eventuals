"""Adds repostories/archives."""

########################################################################
# DO NOT EDIT THIS FILE unless you are inside the
# https://github.com/3rdparty/stout-eventuals repository. If you
# encounter it anywhere else it is because it has been copied there in
# order to simplify adding transitive dependencies. If you want a
# different version of stout-eventuals follow the Bazel build
# instructions at https://github.com/3rdparty/stout-eventuals.
########################################################################

load("//3rdparty/bazel-rules-libuv:repos.bzl", libuv_repos="repos")

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

def repos(external = True, repo_mapping = {}):
    libuv_repos(
        repo_mapping = repo_mapping
    )

    if external and "com_github_3rdparty_stout_eventuals" not in native.existing_rules():
        git_repository(
            name = "com_github_3rdparty_stout_eventuals",
            remote = "https://github.com/3rdparty/stout-eventuals",
            commit = "4ac1ec6870fa7423223f455fc0ed4dba37bef051",
            shallow_since = "1629450574 +0200",
            repo_mapping = repo_mapping,
        )
