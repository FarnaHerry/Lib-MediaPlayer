#include <huxerui/android/platform_registry.h>
#include "detail/media_library.h"

namespace huxerui::media::detail {
void InstallMediaLibrary(ApplicationContext& context) {
  android::JavaWindowPlatformModuleFactory<PlatformChannel> factory;
  factory.class_name = "org.huxerui.lib.media.NativeMediaLibrary$Factory";
  factory.create = [](PlatformChannel channel) { return channel; };
  context.RegisterPlatformModule<PlatformChannel>(media_library_type, std::move(factory));
}
} // namespace huxerui::media::detail
