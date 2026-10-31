#include <backends/tango_opengl/published_source.h>

#include <algorithm>
#include <climits>
#include <memory>
#include <stdexcept>
#include <utility>

namespace flex::tango_opengl {
namespace {

struct PinCloser {
    void operator()(salts_fs_root_revision_pin_t* pin) const noexcept {
        if (pin) (void)salts_fs_root_revision_pin_close(pin);
    }
};

void require_valid_publication(const gcanvas::ImagePublicationIdentity& id) {
    if (id.publisher_id.empty() || id.publisher_id.size() > 256u ||
        id.asset_id.empty() || id.asset_id.size() > 256u ||
        id.generation == 0u) {
        throw std::invalid_argument(
            "Salts image source requires a bounded producer-issued generation");
    }
}

} // namespace

gcanvas::PublishedEncodedImage freeze_salts_published_image(
    const salts_fs_root_t* root,
    const std::string& revision_relative_path,
    const gcanvas::ImagePublicationIdentity& publication,
    const std::vector<std::uint8_t>& trusted_encoded_revision,
    std::size_t max_encoded_bytes) {
    require_valid_publication(publication);
    if (!root || revision_relative_path.empty())
        throw std::invalid_argument("Salts image source requires a rooted revision");
    if (max_encoded_bytes == 0u ||
        trusted_encoded_revision.empty() ||
        trusted_encoded_revision.size() > max_encoded_bytes ||
        trusted_encoded_revision.size() > static_cast<std::size_t>(INT_MAX)) {
        throw std::length_error("Salts image source exceeds bounded encoded extent");
    }

    // Exactly one Salts::FS reader/GC implementation. No C++ ifstream,
    // absolute-path concatenation, second file engine, mmap or parser.
    salts_fs_root_revision_pin_t* native = nullptr;
    const int open_rc = salts_fs_root_revision_pin_open(
        root, revision_relative_path.c_str(), &native);
    if (open_rc != 0 || !native) {
        throw std::runtime_error(
            "Salts image source cannot pin original revision: " +
            std::to_string(open_rc));
    }
    std::unique_ptr<salts_fs_root_revision_pin_t, PinCloser> pin(native);

    // Capacity is the exact trusted extent, not an unbounded dynamic read.
    // In particular, a source with an extra appended byte fails closed.
    std::vector<std::uint8_t> frozen(trusted_encoded_revision.size());
    std::size_t observed = 0u;
    const int copy_rc = salts_fs_root_revision_pin_copy_checked(
        pin.get(), reinterpret_cast<char*>(frozen.data()),
        frozen.size(), &observed);
    if (copy_rc != 0 || observed != trusted_encoded_revision.size() ||
        !std::equal(frozen.begin(), frozen.end(),
                    trusted_encoded_revision.begin())) {
        throw std::runtime_error(
            "Salts image source differs from trusted immutable publication");
    }
    // A successful immutable SNAPSHOT is caller-owned after this close.
    // Neither CURRENT switching nor later GC can mutate frozen bytes.
    const int close_rc = salts_fs_root_revision_pin_close(pin.release());
    if (close_rc != 0)
        throw std::runtime_error("Salts image source cannot close revision pin");
    return {publication, std::move(frozen)};
}

} // namespace flex::tango_opengl
