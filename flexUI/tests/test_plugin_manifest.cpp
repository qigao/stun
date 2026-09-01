#include <tinytest.hpp>

#include <flexUI/plugin_host.h>

#include "plugin_dependency_resolver.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#ifndef FLEXUI_TEST_ECHO_PLUGIN
#error FLEXUI_TEST_ECHO_PLUGIN must name the echo test plugin library
#endif
#ifndef FLEXUI_TEST_PROVIDER_PLUGIN
#error FLEXUI_TEST_PROVIDER_PLUGIN must name the provider test plugin library
#endif
#ifndef FLEXUI_TEST_CONSUMER_PLUGIN
#error FLEXUI_TEST_CONSUMER_PLUGIN must name the consumer test plugin library
#endif

namespace {

class TempDirectory final {
public:
  TempDirectory() {
    char *created = tt_make_temp_dir("flexui-plugin");
    if (created != nullptr) {
      path_ = std::filesystem::path(created);
      std::free(created);
    }
  }

  ~TempDirectory() {
    if (!path_.empty()) {
      (void)tt_remove_tree(path_.string().c_str());
    }
  }

  TempDirectory(const TempDirectory &) = delete;
  TempDirectory &operator=(const TempDirectory &) = delete;

  const std::filesystem::path &path() const noexcept { return path_; }

  std::string copy_library(const std::filesystem::path &source) const {
    const auto destination = path_ / source.filename();
    std::filesystem::copy_file(source, destination,
                               std::filesystem::copy_options::overwrite_existing);
    return destination.filename().string();
  }

  std::filesystem::path write(std::string name,
                              const std::string &content) const {
    const auto destination = path_ / std::move(name);
    if (tt_write_file(destination.string().c_str(), content.data(),
                      content.size()) != 0) {
      return {};
    }
    return std::filesystem::absolute(destination);
  }

private:
  std::filesystem::path path_;
};

std::string dependency(std::string name, std::string version, bool optional) {
  return "\n[[dependencies]]\nname = \"" + name +
         "\"\nversion = \"" + version + "\"\noptional = " +
         (optional ? "true\n" : "false\n");
}

std::string manifest(std::string name, std::string version,
                     std::string library, std::string service,
                     std::string capabilities = "[]",
                     std::string permissions = "[]",
                     std::string dependencies = {}) {
  std::string result =
      "manifest_version = 1\nname = \"" + name + "\"\nversion = \"" +
      version + "\"\nlibrary = \"" + library + "\"\nservices = [\"" +
      service + "\"]\ncapabilities = " + capabilities +
      "\npermissions = " + permissions + "\n";
  if (dependencies.empty()) {
    result += "dependencies = []\n";
  }
  result += "\n[abi]\nmajor = 1\nminor = 0\n";
  result += dependencies;
  return result;
}

void stop_if_built(flexUI::PluginHostBuildResult &built) {
  if (built.host != nullptr) {
    check_true(
        static_cast<bool>(built.host->stop(std::chrono::seconds(1))));
  }
}

} // namespace

suite("FlexUI plugin manifest") {
  group("schema and policy") {
    it("loads a bounded manifest and matches its DLL descriptor") {
      TempDirectory package;
      const auto library = package.copy_library(FLEXUI_TEST_ECHO_PLUGIN);
      const auto path = package.write(
          "plugin.toml", manifest("test.echo", "1.0.0", library,
                                  "test.echo/1"));
      flexUI::PluginHostBuilder builder;
      check_true(static_cast<bool>(builder.load_manifest(path)));
      auto built = builder.build();
      check_true(static_cast<bool>(built));
      check_equal(built.host->statistics().plugin_count, std::size_t{1});
      stop_if_built(built);
    }

    it("rejects a non-canonical semantic version") {
      TempDirectory package;
      const auto library = package.copy_library(FLEXUI_TEST_ECHO_PLUGIN);
      const auto path = package.write(
          "plugin.toml",
          manifest("test.echo", "01.0.0", library, "test.echo/1"));
      flexUI::PluginHostBuilder builder;
      const auto loaded = builder.load_manifest(path);
      check_false(static_cast<bool>(loaded));
      check_equal(static_cast<int>(loaded.error.code),
                  static_cast<int>(flexUI::PluginHostErrorCode::InvalidManifest));
      check_not_equal(loaded.error.message.find("SemVer"), std::string::npos);
    }

    it("accepts canonical semantic version prerelease and build syntax") {
      TempDirectory package;
      const auto library = package.copy_library(FLEXUI_TEST_ECHO_PLUGIN);
      const auto path = package.write(
          "plugin.toml",
          manifest("test.echo", "1.0.0-alpha.1+build.7", library,
                   "test.echo/1"));
      flexUI::PluginHostBuilder builder;
      check_true(static_cast<bool>(builder.load_manifest(path)));
    }

    it("rejects unknown root fields instead of ignoring misspellings") {
      TempDirectory package;
      const auto library = package.copy_library(FLEXUI_TEST_ECHO_PLUGIN);
      auto content =
          manifest("test.echo", "1.0.0", library, "test.echo/1");
      content.insert(content.find("\n[abi]"), "\npermissons = []");
      const auto path = package.write("plugin.toml", content);
      flexUI::PluginHostBuilder builder;
      const auto loaded = builder.load_manifest(path);
      check_false(static_cast<bool>(loaded));
      check_equal(static_cast<int>(loaded.error.code),
                  static_cast<int>(flexUI::PluginHostErrorCode::InvalidManifest));
      check_not_equal(loaded.error.message.find("permissons"),
                      std::string::npos);
    }

    it("denies a declared permission unless host policy grants it") {
      TempDirectory package;
      const auto library = package.copy_library(FLEXUI_TEST_ECHO_PLUGIN);
      const auto path = package.write(
          "plugin.toml", manifest("test.echo", "1.0.0", library,
                                  "test.echo/1", "[]", "[\"file\"]"));
      flexUI::PluginHostBuilder denied_builder;
      const auto denied = denied_builder.load_manifest(path);
      check_false(static_cast<bool>(denied));
      check_equal(static_cast<int>(denied.error.code),
                  static_cast<int>(flexUI::PluginHostErrorCode::PermissionDenied));
      check_not_equal(denied.error.message.find("file"), std::string::npos);

      flexUI::PluginHostPolicy policy;
      policy.granted_permissions =
          flexUI::plugin_permission_mask(flexUI::PluginPermission::File);
      flexUI::PluginHostBuilder allowed_builder({}, policy);
      check_true(static_cast<bool>(allowed_builder.load_manifest(path)));
      auto built = allowed_builder.build();
      check_true(static_cast<bool>(built));
      stop_if_built(built);
    }

    it("rejects a library that canonicalizes outside its package directory") {
      TempDirectory package;
      const auto library = package.copy_library(FLEXUI_TEST_ECHO_PLUGIN);
      const auto child = package.path() / "child";
      std::filesystem::create_directory(child);
      const auto content = manifest("test.echo", "1.0.0", "../" + library,
                                    "test.echo/1");
      const auto path = child / "plugin.toml";
      check_equal(tt_write_file(path.string().c_str(), content.data(),
                                content.size()),
                  0);
      flexUI::PluginHostBuilder builder;
      const auto loaded = builder.load_manifest(std::filesystem::absolute(path));
      check_false(static_cast<bool>(loaded));
      check_equal(static_cast<int>(loaded.error.code),
                  static_cast<int>(flexUI::PluginHostErrorCode::InvalidPath));
    }

    it("rejects a descriptor identity that disagrees with the manifest") {
      TempDirectory package;
      const auto library = package.copy_library(FLEXUI_TEST_ECHO_PLUGIN);
      const auto path = package.write(
          "plugin.toml", manifest("test.other", "1.0.0", library,
                                  "test.echo/1"));
      flexUI::PluginHostBuilder builder;
      check_true(static_cast<bool>(builder.load_manifest(path)));
      const auto built = builder.build();
      check_false(static_cast<bool>(built));
      check_equal(static_cast<int>(built.error.code),
                  static_cast<int>(flexUI::PluginHostErrorCode::ManifestMismatch));
    }

    it("rejects a service set that disagrees with the DLL descriptor") {
      TempDirectory package;
      const auto library = package.copy_library(FLEXUI_TEST_ECHO_PLUGIN);
      const auto path = package.write(
          "plugin.toml", manifest("test.echo", "1.0.0", library,
                                  "test.other/1"));
      flexUI::PluginHostBuilder builder;
      check_true(static_cast<bool>(builder.load_manifest(path)));
      const auto built = builder.build();
      check_false(static_cast<bool>(built));
      check_equal(static_cast<int>(built.error.code),
                  static_cast<int>(flexUI::PluginHostErrorCode::ManifestMismatch));
    }

    it("rejects a manifest beyond its configured file bound") {
      TempDirectory package;
      const auto library = package.copy_library(FLEXUI_TEST_ECHO_PLUGIN);
      const auto path = package.write(
          "plugin.toml", manifest("test.echo", "1.0.0", library,
                                  "test.echo/1"));
      flexUI::PluginHostLimits limits;
      limits.max_manifest_bytes = 16;
      flexUI::PluginHostBuilder builder(limits);
      const auto loaded = builder.load_manifest(path);
      check_false(static_cast<bool>(loaded));
      check_equal(
          static_cast<int>(loaded.error.code),
          static_cast<int>(flexUI::PluginHostErrorCode::ResourceLimitExceeded));
    }
  }

  group("dependency graph") {
    it("returns dependency-first order with staging order as tie break") {
      flexUI::plugin_host_detail::PluginManifest consumer;
      consumer.name = "test.consumer";
      consumer.version = "1.0.0";
      consumer.dependencies.push_back({"test.provider", "1.0.0", false});
      flexUI::plugin_host_detail::PluginManifest provider;
      provider.name = "test.provider";
      provider.version = "1.0.0";
      flexUI::plugin_host_detail::PluginManifest independent;
      independent.name = "test.independent";
      independent.version = "1.0.0";
      const std::vector<flexUI::plugin_host_detail::PluginManifest> manifests = {
          consumer, provider, independent};
      const auto resolved =
          flexUI::plugin_host_detail::resolve_plugin_dependencies(manifests, {});
      check_true(static_cast<bool>(resolved));
      check_equal(resolved.manifest_order.size(), std::size_t{3});
      check_equal(resolved.manifest_order[0], std::size_t{1});
      check_equal(resolved.manifest_order[1], std::size_t{0});
      check_equal(resolved.manifest_order[2], std::size_t{2});
    }

    it("fails before DLL loading when a required dependency is missing") {
      TempDirectory package;
      const auto library = package.copy_library(FLEXUI_TEST_ECHO_PLUGIN);
      const auto path = package.write(
          "plugin.toml",
          manifest("test.echo", "1.0.0", library, "test.echo/1", "[]",
                   "[]", dependency("test.missing", "1.0.0", false)));
      flexUI::PluginHostBuilder builder;
      check_true(static_cast<bool>(builder.load_manifest(path)));
      const auto built = builder.build();
      check_false(static_cast<bool>(built));
      check_equal(static_cast<int>(built.error.code),
                  static_cast<int>(flexUI::PluginHostErrorCode::DependencyMissing));
    }

    it("permits only an explicitly optional missing dependency") {
      TempDirectory package;
      const auto library = package.copy_library(FLEXUI_TEST_ECHO_PLUGIN);
      const auto path = package.write(
          "plugin.toml",
          manifest("test.echo", "1.0.0", library, "test.echo/1", "[]",
                   "[]", dependency("test.missing", "1.0.0", true)));
      flexUI::PluginHostBuilder builder;
      check_true(static_cast<bool>(builder.load_manifest(path)));
      auto built = builder.build();
      check_true(static_cast<bool>(built));
      stop_if_built(built);
    }

    it("rejects a present dependency with a different exact version") {
      TempDirectory package;
      const auto provider_library =
          package.copy_library(FLEXUI_TEST_PROVIDER_PLUGIN);
      const auto consumer_library =
          package.copy_library(FLEXUI_TEST_CONSUMER_PLUGIN);
      const auto provider = package.write(
          "provider.toml", manifest("test.provider", "1.0.0",
                                    provider_library, "test.provider/1"));
      const auto consumer = package.write(
          "consumer.toml",
          manifest("test.consumer", "1.0.0", consumer_library,
                   "test.consumer/1", "[]", "[]",
                   dependency("test.provider", "2.0.0", true)));
      flexUI::PluginHostBuilder builder;
      check_true(static_cast<bool>(builder.load_manifest(consumer)));
      check_true(static_cast<bool>(builder.load_manifest(provider)));
      const auto built = builder.build();
      check_false(static_cast<bool>(built));
      check_equal(
          static_cast<int>(built.error.code),
          static_cast<int>(flexUI::PluginHostErrorCode::DependencyVersionMismatch));
    }

    it("rejects dependency version range syntax in manifest v1") {
      TempDirectory package;
      const auto library = package.copy_library(FLEXUI_TEST_ECHO_PLUGIN);
      const auto path = package.write(
          "plugin.toml",
          manifest("test.echo", "1.0.0", library, "test.echo/1", "[]",
                   "[]", dependency("test.provider", ">=1.0.0", false)));
      flexUI::PluginHostBuilder builder;
      const auto loaded = builder.load_manifest(path);
      check_false(static_cast<bool>(loaded));
      check_equal(static_cast<int>(loaded.error.code),
                  static_cast<int>(flexUI::PluginHostErrorCode::InvalidManifest));
    }

    it("rejects a dependency cycle") {
      TempDirectory package;
      const auto provider_library =
          package.copy_library(FLEXUI_TEST_PROVIDER_PLUGIN);
      const auto consumer_library =
          package.copy_library(FLEXUI_TEST_CONSUMER_PLUGIN);
      const auto provider = package.write(
          "provider.toml",
          manifest("test.provider", "1.0.0", provider_library,
                   "test.provider/1", "[]", "[]",
                   dependency("test.consumer", "1.0.0", false)));
      const auto consumer = package.write(
          "consumer.toml",
          manifest("test.consumer", "1.0.0", consumer_library,
                   "test.consumer/1", "[]", "[]",
                   dependency("test.provider", "1.0.0", false)));
      flexUI::PluginHostBuilder builder;
      check_true(static_cast<bool>(builder.load_manifest(provider)));
      check_true(static_cast<bool>(builder.load_manifest(consumer)));
      const auto built = builder.build();
      check_false(static_cast<bool>(built));
      check_equal(static_cast<int>(built.error.code),
                  static_cast<int>(flexUI::PluginHostErrorCode::DependencyCycle));
    }

    it("builds a reverse-staged dependency pair") {
      TempDirectory package;
      const auto provider_library =
          package.copy_library(FLEXUI_TEST_PROVIDER_PLUGIN);
      const auto consumer_library =
          package.copy_library(FLEXUI_TEST_CONSUMER_PLUGIN);
      const auto provider = package.write(
          "provider.toml", manifest("test.provider", "1.0.0",
                                    provider_library, "test.provider/1"));
      const auto consumer = package.write(
          "consumer.toml",
          manifest("test.consumer", "1.0.0", consumer_library,
                   "test.consumer/1", "[\"test.provider/1\"]", "[]",
                   dependency("test.provider", "1.0.0", false)));
      flexUI::PluginHostBuilder builder;
      check_true(static_cast<bool>(builder.load_manifest(consumer)));
      check_true(static_cast<bool>(builder.load_manifest(provider)));
      auto built = builder.build();
      check_true(static_cast<bool>(built));
      check_equal(built.host->statistics().plugin_count, std::size_t{2});
      check_true(built.registry->contains("test.provider/1"));
      check_true(built.registry->contains("test.consumer/1"));
      stop_if_built(built);
    }

    it("allows a manifest dependency to bind an explicit legacy candidate") {
      TempDirectory package;
      const auto consumer_library =
          package.copy_library(FLEXUI_TEST_CONSUMER_PLUGIN);
      const auto consumer = package.write(
          "consumer.toml",
          manifest("test.consumer", "1.0.0", consumer_library,
                   "test.consumer/1", "[\"test.provider/1\"]", "[]",
                   dependency("test.provider", "1.0.0", false)));
      flexUI::PluginHostBuilder builder;
      check_true(static_cast<bool>(builder.load_plugin(
          std::filesystem::path(FLEXUI_TEST_PROVIDER_PLUGIN))));
      check_true(static_cast<bool>(builder.load_manifest(consumer)));
      auto built = builder.build();
      check_true(static_cast<bool>(built));
      check_equal(built.host->statistics().plugin_count, std::size_t{2});
      stop_if_built(built);
    }
  }

  group("capability contract") {
    it("fails when a required capability is absent from the final registry") {
      TempDirectory package;
      const auto library = package.copy_library(FLEXUI_TEST_ECHO_PLUGIN);
      const auto path = package.write(
          "plugin.toml",
          manifest("test.echo", "1.0.0", library, "test.echo/1",
                   "[\"test.missing/1\"]"));
      flexUI::PluginHostBuilder builder;
      check_true(static_cast<bool>(builder.load_manifest(path)));
      const auto built = builder.build();
      check_false(static_cast<bool>(built));
      check_equal(static_cast<int>(built.error.code),
                  static_cast<int>(flexUI::PluginHostErrorCode::CapabilityMissing));
    }

    it("rejects duplicate plugin IDs while staging") {
      TempDirectory package;
      const auto library = package.copy_library(FLEXUI_TEST_ECHO_PLUGIN);
      const auto first = package.write(
          "first.toml", manifest("test.echo", "1.0.0", library,
                                 "test.echo/1"));
      const auto second = package.write(
          "second.toml", manifest("test.echo", "1.0.0", library,
                                  "test.echo/1"));
      flexUI::PluginHostBuilder builder;
      check_true(static_cast<bool>(builder.load_manifest(first)));
      const auto duplicate = builder.load_manifest(second);
      check_false(static_cast<bool>(duplicate));
      check_equal(static_cast<int>(duplicate.error.code),
                  static_cast<int>(flexUI::PluginHostErrorCode::InvalidManifest));
    }
  }
}
