#pragma once

#include <gcanvas/context.hpp>
#include <salts/fs.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace flex::tango_opengl {

// The trusted single-writer publisher owns the identity, manifest and
// immutable revision name. The witness is the producer's independently
// trusted COMPLETE encoded content, not bytes read back from the same path.
// This adapter does not mint generations, parse manifests or run an image
// decoder. The caller must already have qualified the publisher's origin.
gcanvas::PublishedEncodedImage freeze_salts_published_image(
    const salts_fs_root_t* root,
    const std::string& revision_relative_path,
    const gcanvas::ImagePublicationIdentity& publication,
    const std::vector<std::uint8_t>& trusted_encoded_revision,
    std::size_t max_encoded_bytes);

} // namespace flex::tango_opengl
