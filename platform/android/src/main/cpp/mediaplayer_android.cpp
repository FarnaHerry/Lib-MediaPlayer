#include <huxerui/android/platform_registry.h>

#include "detail/channel_player.h"

namespace huxerui::media::detail {

void InstallPlatform(RootContext& root) {
  android::JavaPlatformModuleFactory<std::shared_ptr<NativePlayer>> module;
  module.class_name = "org.huxerui.lib.mediaplayer.NativeMediaPlayer$Factory";
  module.create = [](PlatformChannel channel) -> std::shared_ptr<NativePlayer> {
    auto player = std::make_shared<ChannelPlayer>(std::move(channel));
    player->Connect();
    return player;
  };
  root.RegisterPlatformModule<std::shared_ptr<NativePlayer>>(player_type, std::move(module));
  android::JavaPlatformViewFactory<EncodedVideoProperties, MediaPlayer> view{
      .class_name = "org.huxerui.lib.mediaplayer.NativeVideoSurface$Factory",
      .connect =
          [](const MediaPlayer& player, PlatformChannel channel) {
            auto current = std::make_shared<std::weak_ptr<NativePlayer>>();
            PlayerAccess::Bind(player, [channel, current](const std::shared_ptr<NativePlayer>& native) {
              if (auto old = current->lock())
                old->UnbindVideo();
              *current = native;
              if (native)
                native->BindVideo(channel);
            });
          },
      .disconnect = [](const MediaPlayer& player) { PlayerAccess::Unbind(player); },
  };
  root.RegisterPlatformView<EncodedVideoProperties, MediaPlayer>(video_type, std::move(view));
}

} // namespace huxerui::media::detail
