#include <huxerui/huxerui.h>
#include <huxerui/mediaplayer.h>
#include <app_resources.h>

#include <algorithm>
#include <functional>
#include <iomanip>
#include <sstream>

using namespace huxerui;
using namespace huxerui::media;

namespace {

std::string TimeLabel(MediaTime time) {
  const auto seconds = static_cast<long long>(std::max(0.0, time.count()));
  std::ostringstream text;
  if (seconds >= 3600) text << seconds / 3600 << ':' << std::setw(2) << std::setfill('0');
  text << (seconds >= 3600 ? seconds / 60 % 60 : seconds / 60)
       << ':' << std::setw(2) << std::setfill('0') << seconds % 60;
  return text.str();
}

const char* StatusLabel(MediaPlayerStatus status) {
  switch (status) {
    case MediaPlayerStatus::Empty: return "Nothing loaded";
    case MediaPlayerStatus::Loading: return "Loading";
    case MediaPlayerStatus::Ready: return "Ready to play";
    case MediaPlayerStatus::Playing: return "Playing";
    case MediaPlayerStatus::Paused: return "Paused";
    case MediaPlayerStatus::Interrupted: return "Interrupted";
    case MediaPlayerStatus::Ended: return "Finished";
    case MediaPlayerStatus::Failed: return "Playback failed";
  }
  return "Unknown";
}

[[huxerui::composable]]
View SecondaryAction(View content) {
  const auto& colors = UseTheme().colors;
  auto style = UseEnvironment<ButtonStyle>();
  style.background = colors.surface_container_highest;
  style.label_style.foreground = colors.on_surface;
  style.disabled_background = colors.surface_container;
  style.disabled_label = colors.on_surface_variant;
  ThemeDefinition overrides;
  overrides.Set(style);
  return Theme(overrides, content);
}

[[huxerui::composable]]
View TransportButton(ImageVariant icon, std::string label, bool enabled, bool primary,
    std::function<void()> action) {
  const auto& colors = UseTheme().colors;
  auto style = UseEnvironment<IconButtonStyle>();
  if (primary) {
    style.icon_size = 30.0F;
    style.minimum_interactive_size = 64.0F;
    style.state_layer_size = 64.0F;
    style.corner_radius = 32.0F;
  }
  style.foreground = primary ? colors.on_primary : colors.on_surface;
  style.disabled_foreground = colors.on_surface_variant;
  ThemeDefinition overrides;
  overrides.Set(style);
  return Theme(overrides, IconButton(icon, label).OnClick(action).With(
      Enabled{enabled},
      Background(primary ? (enabled ? colors.primary : colors.surface_container_highest) : Color::Transparent()),
      CornerRadius(style.corner_radius)
  ));
}

[[huxerui::composable]]
View PlaybackControls(MediaPlayer player) {
  const auto state = player.Snapshot();
  const auto& colors = UseTheme().colors;
  auto seek_draft = UseState(std::optional<float>{});
  const bool can_play = state.status != MediaPlayerStatus::Empty && state.status != MediaPlayerStatus::Failed;
  const bool can_seek = state.seekability == MediaSeekability::Seekable
      && state.progress.duration && state.progress.duration->count() > 0;
  const float duration = can_seek ? static_cast<float>(state.progress.duration->count()) : 1.0F;
  const float position = std::clamp(
      seek_draft.Get().value_or(static_cast<float>(state.progress.position.count())), 0.0F, duration);

  View seek_confirmation = Column {};
  if (can_seek && seek_draft.Get()) {
    seek_confirmation = Row {
      Text("Seek to " + TimeLabel(MediaTime(position)), TextRole::Label)
          .With(Grow(), Foreground(colors.on_surface_variant)),
      SecondaryAction(Button("Apply").OnClick([player, seek_draft] {
        if (seek_draft.Get()) static_cast<void>(player.SeekTo(MediaTime(*seek_draft.Get())));
        seek_draft = std::optional<float>{};
      })),
      IconButton(app::images::close, "Cancel seek")
          .OnClick([seek_draft] { seek_draft = std::optional<float>{}; }),
    }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center));
  }

  return Column {
    Row {
      Text(state.is_seeking ? "Seeking..." : state.is_buffering ? "Buffering..." : StatusLabel(state.status), TextRole::Label)
          .With(Foreground(colors.primary)),
      Spacer(),
      Text(TimeLabel(state.progress.position) + " / "
          + (state.progress.duration ? TimeLabel(*state.progress.duration) : "--:--"), TextRole::Label)
          .With(Foreground(colors.on_surface_variant)),
    }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
    Slider(position)
        .Range(0.0F, duration)
        .OnChanged([seek_draft](float value) { seek_draft = value; })
        .With(Enabled{can_seek}, Semantics{.label = "Playback position"}),
    Row {
      TransportButton(app::images::rewind, "Back 10 seconds", can_seek, false, [player, seek_draft] {
        static_cast<void>(player.SeekTo(MediaTime(std::max(0.0, player.Snapshot().progress.position.count() - 10.0))));
        seek_draft = std::optional<float>{};
      }),
      TransportButton(state.play_when_ready ? app::images::pause : app::images::play,
          state.play_when_ready ? "Pause" : "Play", can_play, true, [player] {
            if (player.Snapshot().play_when_ready) static_cast<void>(player.Pause());
            else static_cast<void>(player.Play());
          }),
      TransportButton(app::images::forward, "Forward 10 seconds", can_seek, false, [player, seek_draft] {
        const auto current = player.Snapshot().progress;
        if (current.duration) {
          static_cast<void>(player.SeekTo(MediaTime(std::min(current.duration->count(), current.position.count() + 10.0))));
        }
        seek_draft = std::optional<float>{};
      }),
    }.With(Spacing(24.0F), CrossAlign(CrossAxisAlignment::Center), MainAlign(MainAxisAlignment::Center)),
    seek_confirmation,
    Divider(),
    Flow {
      Chip(app::images::repeat, "Repeat", state.looping)
          .OnChanged([player](bool value) { static_cast<void>(player.SetLooping(value)); }),
      Chip(app::images::volume, "Mute", state.muted)
          .OnChanged([player](bool value) { static_cast<void>(player.SetMuted(value)); }),
    }.With(Spacing(12.0F), MainAlign(MainAxisAlignment::Center)),
    Row {
      Image(app::images::volume).Tint(colors.on_surface_variant)
          .With(Frame{.width = 20.0F, .height = 20.0F}),
      Slider(static_cast<float>(state.volume))
          .OnChanged([player](float value) { static_cast<void>(player.SetVolume(value)); })
          .With(Grow(), Semantics{.label = "Volume"}),
      Text(std::to_string(static_cast<int>(state.volume * 100)) + "%", TextRole::Label)
          .With(Frame{.width = 40.0F}, Foreground(colors.on_surface_variant)),
    }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Center)),
  }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch));
}

[[huxerui::composable]]
View Diagnostics(MediaPlayer player) {
  auto expanded = UseState(false);
  auto progress_events = UseState(0);
  player.OnProgress([progress_events](const MediaProgress&) { progress_events += 1; });
  View details = Column {};
  if (expanded.Get()) {
    const auto state = player.Snapshot();
    details = Column {
      Text::Format("Progress callbacks: {}", progress_events),
      Text(state.progress.buffered_ranges
          ? "Buffered ranges: " + std::to_string(state.progress.buffered_ranges->size())
          : "Buffered ranges: unavailable"),
      Text("Backend status: " + std::string(StatusLabel(state.status))),
    }.With(Spacing(8.0F), Padding(12.0F));
  }
  return Column {
    SecondaryAction(Button(expanded.Get() ? "Hide diagnostics" : "Show diagnostics")
        .OnClick([expanded] { expanded = !expanded.Get(); })),
    details,
  }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Start));
}

[[huxerui::composable]]
View MediaPage(bool music) {
  auto player = UseMediaPlayer();
  auto tasks = UseTaskScope();
  auto picker = UseService<FilePicker>();
  auto address = UseState(TextEditingValue::FromText(""));
  auto source_name = UseState(std::string{});
  auto error_message = UseState(std::string{});
  auto show_link = UseState(false);
  auto source_revision = UseState(0);
  const auto& colors = UseTheme().colors;
  const bool compact = UseViewportClass() == ViewportClass::Compact;

  player.OnError([error_message](const MediaError& error) {
    error_message = (error.fatal ? "Error: " : "Notice: ") + error.message
        + (error.platform_code.empty() ? "" : " (" + error.platform_code + ")");
  });

  View link_editor = Column {};
  if (show_link.Get()) {
    link_editor = Column {
      TextField(address)
          .Placeholder(music ? "https://example.com/audio.mp3" : "https://example.com/video.mp4")
          .OnChanged([address](const TextEditingValue& value) { address = value; })
          .With(Semantics{.label = "Media URL"}),
      Button("Load URL").OnClick([player, address, source_name, error_message, source_revision, music] {
        try {
          error_message = std::string{};
          if (player.Load(MediaSource(MediaHttpSource{address->text, {}}))) {
            source_name = music ? "Network audio" : "Network video";
            source_revision += 1;
          }
        } catch (const std::invalid_argument& error) { error_message = error.what(); }
      }).With(Enabled{!address->text.empty()}),
      Text("Load a direct media link, then press Play.", TextRole::Label)
          .With(Foreground(colors.on_surface_variant)),
    }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch));
  }

  View artwork = Column {};
  if (music) {
    artwork = Column {
      Image(app::images::album).Fit(ImageFit::Contain)
          .With(Frame{.width = compact ? 176.0F : 240.0F, .height = compact ? 176.0F : 240.0F}),
      Text("MUSIC", TextRole::Label).With(Foreground(colors.primary)),
      Text(source_name->empty() ? "Your next favorite track" : source_name.Get(), TextRole::Title)
          .Align(TextAlign::Center),
      Text(source_name->empty() ? "Open an audio file and settle in." : "Audio playback", TextRole::Label)
          .Align(TextAlign::Center).With(Foreground(colors.on_surface_variant)),
    }.With(Spacing(12.0F), Padding(16.0F), CrossAlign(CrossAxisAlignment::Center));
  } else if (source_name->empty()) {
    artwork = Column {
      Image(app::images::video).With(Frame{.width = 56.0F, .height = 56.0F}),
      Text("A screen for your stories", TextRole::Title).Align(TextAlign::Center),
      Text("Open a video file or load a direct link.", TextRole::Label)
          .Align(TextAlign::Center).With(Foreground(colors.on_surface_variant)),
    }.With(Frame{.height = compact ? 210.0F : 320.0F}, Padding(20.0F), Spacing(16.0F),
        MainAlign(MainAxisAlignment::Center), CrossAlign(CrossAxisAlignment::Center),
        Background(Color::Rgb(10, 15, 24)), CornerRadius(16.0F));
  } else {
    artwork = Column {
      VideoSurface(player).With(Frame{.height = compact ? 210.0F : 360.0F}, Background(Color::Black())),
      Text(source_name, TextRole::Label).With(Padding(8.0F), Foreground(colors.on_surface_variant)),
    }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Stretch));
  }

  return Column {
    Flow {
      Button(music ? "Open audio" : "Open video").OnClick([player, tasks, picker, source_name, error_message, source_revision, music] {
        tasks.Launch([player, picker, source_name, error_message, source_revision, music]() -> Task<void> {
          try {
            auto file = co_await picker->OpenFileAsync({
                .name = music ? "Audio" : "Video", .content_types = {music ? "audio/*" : "video/*"}});
            if (!file) co_return;
            error_message = std::string{};
            if (player.Load(MediaSource(*file))) {
              source_name = file->Name().empty() ? "Selected file" : file->Name();
              source_revision += 1;
            }
          } catch (const std::exception& error) { error_message = error.what(); }
        });
      }).With(Enabled{picker->CanOpenFiles()}),
      SecondaryAction(Button(show_link.Get() ? "Hide link" : "Open link")
          .OnClick([show_link] { show_link = !show_link.Get(); })),
      SecondaryAction(Button("Unload").OnClick([player, source_name, error_message, source_revision] {
        static_cast<void>(player.Clear());
        source_name = std::string{};
        error_message = std::string{};
        source_revision += 1;
      }).With(Enabled{!source_name->empty()})),
    }.With(Spacing(12.0F)),
    link_editor,
    Column {
      artwork,
      PlaybackControls(player).Key(source_revision.Get()),
    }.With(Padding(compact ? 16.0F : 24.0F), Spacing(24.0F), CrossAlign(CrossAxisAlignment::Stretch),
        Background(colors.surface_container), CornerRadius(24.0F)),
    error_message->empty() ? View(Column {})
        : Text(error_message).With(Padding(16.0F), Foreground(colors.error),
            Border{colors.error, 1.0F}, CornerRadius(12.0F)),
    Diagnostics(player).Key(source_revision.Get()),
  }.With(Spacing(16.0F), CrossAlign(CrossAxisAlignment::Stretch));
}

[[huxerui::composable]]
View MediaContent() {
  auto selected = UseState(std::size_t{0});
  const bool compact = UseViewportClass() == ViewportClass::Compact;
  return Column {
    TopAppBar("MediaPlayer"),
    ScrollView(Column {
      Text("HUXERUI / PLAYER PREVIEW", TextRole::Label).With(Foreground(UseTheme().colors.on_surface_variant)),
      Tabs({"Video", "Music"}, selected).OnChanged([selected](std::size_t index) { selected = index; }),
      MediaPage(selected.Get() == 1).Key(selected.Get()),
      Text("Switching pages stops playback and clears the current source.", TextRole::Label)
          .With(Foreground(UseTheme().colors.on_surface_variant)),
    }.With(Frame{.max_width = 1000.0F}, Padding(compact ? 16.0F : 32.0F),
        Spacing(20.0F), CrossAlign(CrossAxisAlignment::Stretch))).With(Grow()),
  }.With(CrossAlign(CrossAxisAlignment::Stretch), Background(UseTheme().colors.background));
}

[[huxerui::composable]]
View PreviewTheme() {
  const auto& colors = UseTheme().colors;
  constexpr float action_height = 44.0F;
  constexpr float action_radius = action_height / 2.0F;
  constexpr float icon_size = 24.0F;
  const auto action_padding = EdgeInsets::Symmetric(16.0F, 10.0F);

  auto button = UseEnvironment<ButtonStyle>();
  button.background = colors.primary;
  button.label_style.foreground = colors.on_primary;
  button.disabled_background = colors.surface_container_highest;
  button.disabled_label = colors.on_surface_variant;
  button.minimum_height = action_height;
  button.corner_radii = CornerRadii(action_radius);
  button.padding = action_padding;

  auto chip = UseEnvironment<ChipStyle>();
  chip.background = colors.surface;
  chip.label_style.foreground = colors.on_surface;
  chip.selected_background = colors.primary;
  chip.selected_label = colors.on_primary;
  chip.disabled_background = colors.surface_container;
  chip.disabled_selected_background = colors.surface_container_highest;
  chip.disabled_label = colors.on_surface_variant;
  chip.disabled_selected_label = colors.on_surface_variant;
  auto outline = colors.on_surface_variant;
  outline.alpha = 0.3F;
  chip.border = Border{outline, 1.0F};
  chip.selected_border = Border{colors.primary, 1.0F};
  chip.disabled_border = chip.border;
  chip.disabled_selected_border = chip.border;
  chip.minimum_height = action_height;
  chip.corner_radii = button.corner_radii;
  chip.padding = action_padding;
  chip.label_style.font = button.label_style.font;
  chip.icon_size = icon_size;
  chip.icon_spacing = 8.0F;

  auto icon_button = UseEnvironment<IconButtonStyle>();
  icon_button.foreground = colors.on_surface;
  icon_button.disabled_foreground = colors.on_surface_variant;
  icon_button.minimum_interactive_size = action_height;
  icon_button.state_layer_size = action_height;
  icon_button.corner_radius = action_radius;
  icon_button.icon_size = icon_size;

  ThemeDefinition overrides;
  overrides.Set(button).Set(chip).Set(icon_button);
  return Theme(overrides, MediaContent());
}

} // namespace

View App() { return FlatDarkTheme(PreviewTheme()); }

const Application application{
    App,
    {
        .window = {
            .title = "MediaPlayer Preview",
            .initial_size = {1040.0F, 900.0F},
        },
        .root_hooks = {
            huxerui::media::Install,
        },
    }
};
