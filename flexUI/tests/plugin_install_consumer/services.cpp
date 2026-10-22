#include <flexUI/service_registry.h>

#include <iostream>

int main() {
  using flexUI::ApplicationServiceRegistryErrorCode;

  flexUI::ApplicationServiceRegistryBuilder builder;
  const auto built = builder.build();
  if (!built || built.registry->size() != 0 ||
      built.registry->contains("example.echo/1")) {
    std::cerr << "Empty installed registry construction failed\n";
    return 1;
  }
  if (!built.registry->validate_manifest({})) {
    std::cerr << "Empty manifest was rejected\n";
    return 1;
  }

  flexUI::ApplicationCapabilityManifest manifest;
  manifest.allowed = {"example.echo/1"};
  manifest.required = {"example.echo/1"};
  const auto missing = built.registry->validate_manifest(manifest);
  if (missing || missing.error.code !=
                     ApplicationServiceRegistryErrorCode::MissingRequiredCapability) {
    std::cerr << "Missing required capability was not reported\n";
    return 1;
  }

  flexUI::ApplicationServiceRegistryLimits limits;
  limits.max_services = 0;
  flexUI::ApplicationServiceRegistryBuilder invalid_builder(limits);
  const auto invalid = invalid_builder.build();
  if (invalid || invalid.error.code != ApplicationServiceRegistryErrorCode::InvalidLimits) {
    std::cerr << "Invalid registry limits were not rejected\n";
    return 1;
  }

  std::cout << "Services installed registry consumer passed\n";
  return 0;
}
