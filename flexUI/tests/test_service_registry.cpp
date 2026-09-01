#include <flexUI/service_registry.h>

#include <tinytest.hpp>

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

class FakeEndpoint final : public flexUI::IApplicationServiceEndpoint {
public:
  flexUI::ApplicationServiceSubmitResult
  try_submit(const flexUI::ApplicationServiceRequest &request,
             flexUI::IApplicationServiceCompletionSink &completion_sink) override {
    static_cast<void>(request);
    static_cast<void>(completion_sink);
    ++submissions;
    return {};
  }

  std::size_t submissions = 0;
};

flexUI::ApplicationServiceDescriptor descriptor(std::string capability = "document.storage/1",
                                                std::string operation = "read",
                                                std::size_t max_payload_bytes = 1024) {
  return {std::move(capability), {{std::move(operation), max_payload_bytes}}};
}

flexUI::ApplicationCapabilityManifest manifest(std::initializer_list<std::string> allowed,
                                               std::initializer_list<std::string> required = {}) {
  return {std::vector<std::string>(allowed), std::vector<std::string>(required)};
}

} // namespace

spec("FlexUI application service registry") {
  it("rejects invalid limits and enforces exact service capacity") {
    flexUI::ApplicationServiceRegistryLimits invalid_limits;
    invalid_limits.max_services = 0;
    flexUI::ApplicationServiceRegistryBuilder invalid(invalid_limits);
    auto invalid_build = invalid.build();
    check_false(static_cast<bool>(invalid_build));
    check(invalid_build.error.code == flexUI::ApplicationServiceRegistryErrorCode::InvalidLimits);

    flexUI::ApplicationServiceRegistryLimits limits;
    limits.max_services = 1;
    flexUI::ApplicationServiceRegistryBuilder builder(limits);
    check(builder.register_service(descriptor(), std::make_shared<FakeEndpoint>()));
    auto full =
        builder.register_service(descriptor("document.print/1"), std::make_shared<FakeEndpoint>());
    check_false(static_cast<bool>(full));
    check(full.error.code == flexUI::ApplicationServiceRegistryErrorCode::ServiceLimitExceeded);

    auto built = builder.build();
    check(built);
    check_equal(built.registry->size(), std::size_t{1});

    flexUI::ApplicationServiceRegistryLimits operation_limits;
    operation_limits.max_services = 2;
    operation_limits.max_operations_per_service = 1;
    operation_limits.max_identifier_bytes = 64;
    operation_limits.max_payload_bytes = 8;
    flexUI::ApplicationServiceRegistryBuilder operation_builder(operation_limits);
    check(operation_builder.register_service(descriptor("document.storage/1", "read", 8),
                                             std::make_shared<FakeEndpoint>()));
    auto too_many_operations = descriptor("document.print/1", "read", 8);
    too_many_operations.operations.push_back({"write", 8});
    auto operation_full = operation_builder.register_service(std::move(too_many_operations),
                                                             std::make_shared<FakeEndpoint>());
    check_false(static_cast<bool>(operation_full));
    check(operation_full.error.code ==
          flexUI::ApplicationServiceRegistryErrorCode::OperationLimitExceeded);

    auto payload_too_large = operation_builder.register_service(
        descriptor("document.print/1", "read", 9), std::make_shared<FakeEndpoint>());
    check_false(static_cast<bool>(payload_too_large));
    check(payload_too_large.error.code ==
          flexUI::ApplicationServiceRegistryErrorCode::InvalidPayloadLimit);
  }

  it("rejects malformed descriptors without changing prior registrations") {
    flexUI::ApplicationServiceRegistryBuilder builder;
    auto endpoint = std::make_shared<FakeEndpoint>();
    check(builder.register_service(descriptor(), endpoint));

    auto invalid_capability = builder.register_service(descriptor("Document.Storage/1"),
                                                       std::make_shared<FakeEndpoint>());
    check_false(static_cast<bool>(invalid_capability));
    check(invalid_capability.error.code ==
          flexUI::ApplicationServiceRegistryErrorCode::InvalidCapability);

    auto invalid_version = builder.register_service(descriptor("document.storage/01"),
                                                    std::make_shared<FakeEndpoint>());
    check_false(static_cast<bool>(invalid_version));
    check(invalid_version.error.code ==
          flexUI::ApplicationServiceRegistryErrorCode::InvalidCapability);

    auto invalid_namespace = builder.register_service(descriptor("document..storage/1"),
                                                      std::make_shared<FakeEndpoint>());
    check_false(static_cast<bool>(invalid_namespace));
    check(invalid_namespace.error.code ==
          flexUI::ApplicationServiceRegistryErrorCode::InvalidCapability);

    auto invalid_operation = builder.register_service(descriptor("document.print/1", "Print"),
                                                      std::make_shared<FakeEndpoint>());
    check_false(static_cast<bool>(invalid_operation));
    check(invalid_operation.error.code ==
          flexUI::ApplicationServiceRegistryErrorCode::InvalidOperation);

    auto invalid_payload = builder.register_service(descriptor("document.print/1", "print", 0),
                                                    std::make_shared<FakeEndpoint>());
    check_false(static_cast<bool>(invalid_payload));
    check(invalid_payload.error.code ==
          flexUI::ApplicationServiceRegistryErrorCode::InvalidPayloadLimit);

    auto duplicate_operations = descriptor("document.print/1");
    duplicate_operations.operations.push_back({"read", 16});
    auto duplicate_operation =
        builder.register_service(std::move(duplicate_operations), std::make_shared<FakeEndpoint>());
    check_false(static_cast<bool>(duplicate_operation));
    check(duplicate_operation.error.code ==
          flexUI::ApplicationServiceRegistryErrorCode::DuplicateOperation);

    auto missing_endpoint = builder.register_service(descriptor("document.print/1"), {});
    check_false(static_cast<bool>(missing_endpoint));
    check(missing_endpoint.error.code ==
          flexUI::ApplicationServiceRegistryErrorCode::MissingEndpoint);

    auto built = builder.build();
    check(built);
    check_equal(built.registry->size(), std::size_t{1});
  }

  it("rejects duplicate capabilities without replacing the first endpoint") {
    flexUI::ApplicationServiceRegistryBuilder builder;
    auto first = std::make_shared<FakeEndpoint>();
    auto second = std::make_shared<FakeEndpoint>();
    check(builder.register_service(descriptor(), first));
    auto duplicate = builder.register_service(descriptor(), second);
    check_false(static_cast<bool>(duplicate));
    check(duplicate.error.code == flexUI::ApplicationServiceRegistryErrorCode::DuplicateCapability);

    auto built = builder.build();
    check(built);
    auto resolved =
        built.registry->resolve({{1, 1}, 7, "document.storage/1", "read", "payload"},
                                manifest({"document.storage/1"}, {"document.storage/1"}));
    check(resolved);
    check(resolved.endpoint == first);
    check_false(resolved.endpoint == second);
  }

  it("validates allowed and required capability policy") {
    flexUI::ApplicationServiceRegistryBuilder builder;
    check(builder.register_service(descriptor(), std::make_shared<FakeEndpoint>()));
    auto built = builder.build();
    check(built);

    auto duplicate_allowed =
        built.registry->validate_manifest(manifest({"document.storage/1", "document.storage/1"}));
    check_false(static_cast<bool>(duplicate_allowed));
    check(duplicate_allowed.error.code ==
          flexUI::ApplicationServiceRegistryErrorCode::InvalidManifest);

    auto not_allowed =
        built.registry->validate_manifest(manifest({"document.storage/1"}, {"document.print/1"}));
    check_false(static_cast<bool>(not_allowed));
    check(not_allowed.error.code == flexUI::ApplicationServiceRegistryErrorCode::InvalidManifest);

    auto missing =
        built.registry->validate_manifest(manifest({"document.print/1"}, {"document.print/1"}));
    check_false(static_cast<bool>(missing));
    check(missing.error.code ==
          flexUI::ApplicationServiceRegistryErrorCode::MissingRequiredCapability);

    check(built.registry->validate_manifest(
        manifest({"document.storage/1"}, {"document.storage/1"})));
  }

  it("maps policy and descriptor failures to command validation errors") {
    flexUI::ApplicationServiceRegistryBuilder builder;
    auto storage = descriptor();
    storage.operations.push_back({"write", 4});
    check(builder.register_service(std::move(storage), std::make_shared<FakeEndpoint>()));
    auto built = builder.build();
    check(built);
    const auto policy = manifest({"document.storage/1"});

    check(built.registry->validate_command({1, "document.storage/1", "read", "payload"}, policy)
              .code == flexUI::ApplicationCommandErrorCode::None);
    check(
        built.registry->validate_command({1, "document.print/1", "read", "payload"}, policy).code ==
        flexUI::ApplicationCommandErrorCode::UnknownCapability);
    check(built.registry->validate_command({1, "document.storage/1", "remove", "payload"}, policy)
              .code == flexUI::ApplicationCommandErrorCode::UnsupportedOperation);
    check(built.registry->validate_command({1, "document.storage/1", "write", "large"}, policy)
              .code == flexUI::ApplicationCommandErrorCode::InvalidPayload);
  }

  it("resolves only authorized supported requests") {
    flexUI::ApplicationServiceRegistryBuilder builder;
    auto endpoint = std::make_shared<FakeEndpoint>();
    check(builder.register_service(descriptor(), endpoint));
    auto built = builder.build();
    check(built);

    const flexUI::ApplicationServiceRequest request{
        {1, 1}, 9, "document.storage/1", "read", "payload"};
    auto unauthorized = built.registry->resolve(request, manifest({}));
    check_false(static_cast<bool>(unauthorized));
    check(unauthorized.error.code ==
          flexUI::ApplicationServiceRegistryErrorCode::UnauthorizedCapability);

    auto resolved = built.registry->resolve(request, manifest({"document.storage/1"}));
    check(resolved);
    check(resolved.endpoint == endpoint);
    check_true(built.registry->contains("document.storage/1"));
    check_false(built.registry->contains("document.print/1"));
  }

  it("restricts mutable builder operations to its owner thread") {
    flexUI::ApplicationServiceRegistryBuilder builder;
    flexUI::ApplicationServiceRegistryResult registered;
    flexUI::ApplicationServiceRegistryBuildResult built;
    std::thread worker([&] {
      registered = builder.register_service(descriptor(), std::make_shared<FakeEndpoint>());
      built = builder.build();
    });
    worker.join();

    check(registered.error.code == flexUI::ApplicationServiceRegistryErrorCode::WrongThread);
    check(built.error.code == flexUI::ApplicationServiceRegistryErrorCode::WrongThread);
  }
}
