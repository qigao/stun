#include "flex.h"
#include "flex/binary/reader.h"
#include "runtime/scene_clone.h"

namespace flex {

Definition::SharedPtr Definition::load_binary(const char *path) {
  auto def = std::shared_ptr<Definition>(new Definition());
  def->impl_ = std::make_unique<Impl>();

  binary::BinaryReader reader;
  if (!reader.load_file(path)) {
    def->impl_->has_error = true;
    def->impl_->error_message = reader.error_message();
    return def;
  }

  auto *scene = reader.create_scene();
  if (scene) {
    def->impl_->scene = runtime::clone_scene(scene, def->impl_->object_alloc);
  }
  def->impl_->timelines = reader.create_timelines();
  if (!def->impl_->scene) {
    def->impl_->scene =
        Scene::create(reader.canvas_width(), reader.canvas_height(), def->impl_->object_alloc);
  }
  return def;
}

Definition::SharedPtr Definition::load_binary_data(const void *data, size_t size) {
  auto def = std::shared_ptr<Definition>(new Definition());
  def->impl_ = std::make_unique<Impl>();

  binary::BinaryReader reader;
  if (!reader.load_memory(data, size)) {
    def->impl_->has_error = true;
    def->impl_->error_message = reader.error_message();
    return def;
  }

  auto *scene = reader.create_scene();
  if (scene) {
    def->impl_->scene = runtime::clone_scene(scene, def->impl_->object_alloc);
  }
  def->impl_->timelines = reader.create_timelines();
  if (!def->impl_->scene) {
    def->impl_->scene =
        Scene::create(reader.canvas_width(), reader.canvas_height(), def->impl_->object_alloc);
  }
  return def;
}

Definition::SharedPtr Definition::load_binary_encrypted(const char *path,
                                                        const char *password) {
  (void)password;
  auto def = std::shared_ptr<Definition>(new Definition());
  def->impl_ = std::make_unique<Impl>();

  binary::BinaryReader reader;
  if (!reader.load_file_encrypted(path, password)) {
    def->impl_->has_error = true;
    def->impl_->error_message = reader.error_message();
    return def;
  }

  auto *scene = reader.create_scene();
  if (scene) {
    def->impl_->scene = runtime::clone_scene(scene, def->impl_->object_alloc);
  }
  def->impl_->timelines = reader.create_timelines();
  if (!def->impl_->scene) {
    def->impl_->scene =
        Scene::create(reader.canvas_width(), reader.canvas_height(), def->impl_->object_alloc);
  }
  return def;
}

} // namespace flex
