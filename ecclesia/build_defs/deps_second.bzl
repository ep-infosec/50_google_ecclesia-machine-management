"""Load and configure transitive dependencies.

These are custom dependency loading functions provided by external
projects - due to bazel load formatting these cannot be loaded
in "deps_first.bzl".
"""

load("@com_google_googleapis//:repository_rules.bzl", "switched_rules_by_language")
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
load("@com_github_nelhage_rules_boost//:boost/boost.bzl", "boost_deps")
load("@com_google_tensorflow_serving//tensorflow_serving:workspace.bzl", "tf_serving_workspace")
load("@rules_pkg//:deps.bzl", "rules_pkg_dependencies")
load("@rules_pkg//toolchains/rpm:rpmbuild_configure.bzl", "find_system_rpmbuild")

def ecclesia_deps_second():
    """Loads transitive dependencies of projects imported from dependencies.bzl"""
    switched_rules_by_language(
        name = "com_google_googleapis_imports",
        cc = True,
        grpc = True,
    )
    grpc_deps()
    boost_deps()
    tf_serving_workspace()
    rules_pkg_dependencies()

    # If a system rpmbuild exists, find it and enable pkg_rpm
    find_system_rpmbuild(name = "rules_pkg_rpmbuild")
