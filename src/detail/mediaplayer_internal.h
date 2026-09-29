#pragma once

#include <cstdint>
#include <deque>
#include <thread>

#include <huxerui/mediaplayer.h>
#include <huxerui/external_texture.h>
#include <huxerui/platform_registry.h>
#include <huxerui/state.h>

namespace huxerui::media::detail {

inline constexpr char player_type[] = "huxerui/media/player";
inline constexpr char video_type[] = "huxerui/media/video";

struct NativeSnapshot {
  MediaProgress progress;
  MediaSeekability seekability = MediaSeekability::Unknown;
  std::optional<bool> has_audio;
  std::optional<bool> has_video;
  std::optional<Size> video_size;
  bool buffering = false;
};

enum class NativeEventKind { Update, Ready, Playing, Paused, Seeked, Ended, Interrupted, Error };

struct NativeEvent {
  std::uint64_t generation = 0;
  NativeEventKind kind = NativeEventKind::Update;
  NativeSnapshot snapshot;
  std::uint64_t seek_id = 0;
  MediaError error;
};

/// Implementation-only boundary. Every adapter stops using a source before releasing its retained capability.
class NativePlayer {
public:
  virtual ~NativePlayer() = default;
  std::function<void(NativeEvent)> emit;
  virtual void Load(const MediaSource& source, std::uint64_t generation) = 0;
  virtual void Clear() noexcept = 0;
  virtual void Play() = 0;
  virtual void Pause() = 0;
  virtual void Seek(MediaTime position, std::uint64_t seek_id) = 0;
  virtual void SetVolume(double volume, bool muted) = 0;
  /// 「最多预取多少秒」的会话级偏好（2026-09-29）。默认空实现：只有能配置预取预算的后端才照办
  /// （Android / ExoPlayer 那套）。`MediaTime{0.0}` = 用后端默认。
  virtual void SetBufferBudget(MediaTime) {}
  virtual void Poll() = 0;
  virtual void BindVideo(PlatformChannel) {}
  virtual void UnbindVideo() {}
  virtual std::shared_ptr<ExternalTexture> Texture() {
    return {};
  }
};

using VideoBinding = std::function<void(const std::shared_ptr<NativePlayer>&)>;

class PlayerState : public std::enable_shared_from_this<PlayerState> {
public:
  using Post = std::function<void(std::function<void()>)>;
  explicit PlayerState(Post post);
  void CheckThread() const;
  void Attach(std::shared_ptr<NativePlayer> value);
  void Detach() noexcept;
  void Receive(NativeEvent event);
  void Publish(bool force_progress = false);
  void Report(MediaError error);
  void Poll();
  void Bind(VideoBinding binding);
  void Unbind();
  void StartSeek(MediaTime position);
  void CheckLoop();
  void ResetSource();
  void FinishEnded();

  MediaPlayerState value;
  State<MediaPlayerState> observed;
  State<std::shared_ptr<ExternalTexture>> observed_texture;
  std::shared_ptr<NativePlayer> native;
  std::optional<MediaSource> source;
  /// 会话级偏好：最多预取多少秒（`MediaTime{0.0}` = 后端默认）。跟音量/循环一样跨 Load/Clear 保留，
  /// 并在 `Attach` 时重新应用（后端每次 Load 可能换一个 player 实例）。
  MediaTime buffer_budget{0.0};
  std::uint64_t generation = 0;
  std::uint64_t attachment = 0;
  std::uint64_t seek_serial = 0;
  std::uint64_t active_seek = 0;
  std::optional<MediaTime> pending_seek;
  bool loop_reported = false;
  bool loop_restart = false;
  std::uint64_t next_subscription = 0;
  std::map<std::uint64_t, std::function<void(const MediaProgress&)>> progress_handlers;
  std::map<std::uint64_t, std::function<void()>> ended_handlers;
  std::map<std::uint64_t, std::function<void(const MediaError&)>> error_handlers;
  Post post;

private:
  void NotifyProgress();
  std::uint64_t progress_notification_ = 0;
  std::thread::id owner_;
  MediaProgress published_progress_;
  VideoBinding video_binding_;
};

struct PlayerAccess {
  static const auto& Source(const MediaSource& source) {
    return source.value_;
  }
  static const auto& State(const MediaPlayer& player) {
    return player.state_;
  }
  static MediaPlayer Create(std::shared_ptr<PlayerState> state) {
    return MediaPlayer(std::move(state));
  }
  static void Bind(const MediaPlayer& player, VideoBinding binding) {
    player.state_->Bind(std::move(binding));
  }
  static void Unbind(const MediaPlayer& player) {
    player.state_->Unbind();
  }
};

void InstallPlatform(ApplicationContext& root);
void ValidateVideoProperties(const VideoSurfaceProperties& properties);

} // namespace huxerui::media::detail
