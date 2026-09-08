# Lib-MediaPlayer

Native audio and video playback for HuxerUI. The public API is in
[`huxerui/mediaplayer.h`](include/huxerui/mediaplayer.h), under `huxerui::media`.

The player owns playback independently of its optional video surface. The library supplies media state and commands;
applications compose their own controls.

## Integration

Use the current HuxerUI SDK with FileReference-capable platform payloads and the current CLI library project format.
The library uses public SDK APIs only; no HuxerUI source changes are required.

For a local checkout, add this after your app target is created:

```cmake
huxerui_use_library(your_app
    TARGET HuxerUI::MediaPlayer
    PATH "${CMAKE_CURRENT_SOURCE_DIR}/../Lib-MediaPlayer"
)
```

Register `huxerui::media::Install` in your application's `root_hooks`. See the complete
[`preview`](examples/preview/src/app.cpp) for application registration, file picking, URL input, progress,
seeking, volume, looping, and mounting/unmounting video.

```cpp
#include <huxerui/huxerui.h>
#include <huxerui/mediaplayer.h>

using namespace huxerui;
using namespace huxerui::media;

[[huxerui::composable]]
View PlayerPane() {
  auto player = UseMediaPlayer();
  auto notice = UseState(std::string{});

  player.OnError([notice](const MediaError& error) { notice = error.message; });
  player.OnEnded([notice] { notice = "Playback finished."; });

  return Column {
    VideoSurface(player).With(Frame{.height = 240.0F}),
    Row {
      Button("Load").OnClick([player] {
        static_cast<void>(player.Load(MediaSource(File("/absolute/path/to/video.mp4"))));
      }),
      Button("Play").OnClick([player] { static_cast<void>(player.Play()); }),
      Button("Pause").OnClick([player] { static_cast<void>(player.Pause()); }),
    }.With(Spacing(8.0F)),
    Text(notice),
  }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch));
}
```

Replace the example path with an accessible local file. Use a picker-provided `FileReference` on Web and for external
documents. A network source is `MediaSource(MediaHttpSource{"https://your-server/video.mp4", {}})`.
Loading never starts playback automatically.

## Playback contract

- `UseMediaPlayer()` creates a composition-owned session. Copies of `MediaPlayer` identify the same session, but do not
  keep it active after its owning composition unmounts. Registering a callback does not create another player.
- All public operations and callbacks run on the owning UI thread. `Snapshot()` observes state only where it is read
  during composition; put frequently updating controls in a small composable.
- Command `bool` results report acceptance, not successful native completion. Disconnected commands return `false` and
  are not queued. Invalid URLs, malformed headers, nonfinite/negative seek positions, or volume outside `[0, 1]` throw
  `std::invalid_argument`.
- `Load()` replaces the source, invalidates old callbacks, cancels pending seeks, and enters `Loading`.
  `Clear()` releases the source and enters `Empty`. Both preserve volume, mute, and looping preferences.
- `Play()` records intent during loading. Native playback notifications determine `Playing`; buffering is a separate
  flag. `Pause()` cancels play intent without clearing the position. Repeated Play/Pause/Clear requests are idempotent.
- `SeekTo()` requires known seekability and clamps to a known duration. One seek runs at a time; subsequent requests
  collapse to the latest target. Reported position comes from the backend, not from the requested target or a UI clock.
- `Ended` is natural completion. Replay and looping require a finite, seekable source. Successful loops do not emit
  `OnEnded`; unsupported looping reports a nonfatal error and allows normal completion. Failed loop seeks report
  `SeekFailed` and finish naturally.
- `Interrupted` preserves play intent while a native interruption prevents playback. Resumption remains subject to
  platform policy; applications may request Play or Pause explicitly.
- Fatal errors enter `Failed` and remain in `Snapshot().error`. Retry by calling `Load()` again. Recoverable errors are
  delivered through `OnError` without populating the fatal-error field.

`MediaPlayerState` includes status, progress, seekability, play intent, buffering/seeking flags, optional track/video
metadata, preferences, and the current fatal error. Time values are `std::chrono::duration<double>` in seconds.
Missing duration means unknown or nonfinite; missing buffered ranges means the backend cannot provide them, while an
empty range list means none are currently reported. Optional audio/video flags are unknown until the backend can tell.

## Callbacks

`OnProgress`, `OnEnded`, and `OnError` are composition-bound subscriptions. Multiple independent subscribers are allowed;
unmounting a subscription removes it. Extra arguments are ordinary HuxerUI Lifecycle dependencies: changing them replaces
that callback, not the player or source. With no extra dependencies, capture stable State handles rather than changing
plain values.

```cpp
auto progress = UseState(MediaProgress{});
player.OnProgress([progress](const MediaProgress& value) { progress = value; });
```

Progress delivers an initial snapshot, normally samples native state about every 250 ms, and publishes significant
transitions such as load, clear, seek completion, pause, and end. Queued bursts coalesce; this is not a precise timer or
a video-frame callback. Callback data matches the current snapshot at delivery. Old-source and disconnected callbacks
are discarded. Ended/error subscriptions do not replay historical events.

## Video surface

`VideoSurface(player, properties)` shows video only: it has no controls, autoplay, fullscreen, or transport ownership.
Provide bounded layout. At most one surface may be mounted for a player; a second binding throws `std::logic_error`.
Removing the surface does not issue Pause or release the source.

`VideoSurfaceProperties` reuses `ImageFit` (`Contain`, `Cover`, `Fill`, `None`, `ScaleDown`) and horizontal/vertical
`Start`, `Center`, or `End` alignment. `Stretch` is not a content alignment and is rejected.

## Platform adapters and requirements

| Platform | Native backend | Video output | Additional requirement |
| --- | --- | --- | --- |
| Windows | Media Foundation MediaEngine | D3D11 frame server and HWND swap chain | Windows media components and D3D11-capable hardware |
| macOS | AVFoundation | AVPlayerLayer / NSView | macOS SDK; app-owned network policy |
| iOS | AVFoundation | AVPlayerLayer / UIView | iOS SDK; app-owned audio/session and network policies |
| Android | AndroidX Media3 ExoPlayer | TextureView | API 23+, Media3 `1.9.3` resolved by Gradle |
| Linux | GtkMediaFile / GtkMediaStream | GdkTexture through HuxerUI ExternalTexture | GTK 4.14+ and a working GTK media backend/codecs |
| Web | HTMLMediaElement | Native video element | Browser media codecs, permissions, and playback policy |

Container, codec, and streaming-protocol support follows each backend; this is not a bundled FFmpeg codec suite.
Android currently includes the progressive ExoPlayer backend, not the optional HLS/DASH modules.

`FileReference` is retained until the backend stops using the source. Android uses its native URI; Web resolves its
native File to an object URL without reading the whole file into memory. Desktop/Apple adapters use `AsFile()` while
retaining the original reference. References without a native-readable location report `Unsupported`; raw sequential
streams and custom stream providers are not accepted as public media inputs in this version.

Custom HTTP headers are validated by the API but **currently report `Unsupported` on every adapter**. They are never
silently ignored, and no adapter adds a fetching proxy or forwards caller headers across redirects. HTTP sources with
empty headers use the native backend's networking and redirect behavior.

Applications own Android INTERNET permission, Apple transport-security policy, browser CORS/mixed-content requirements,
and any background playback/audio-session configuration. The preview declares Android INTERNET permission. The library
does not change process-wide Apple audio-session categories or add background services. Browser Play may fail with
`PlaybackNotAllowed`; invoke it directly from a user gesture after the source is ready.

Out of scope: playlists, speed control, track/subtitle selection, DRM, casting, downloads, PiP, system media controls,
background services, and raw decoded frame/PCM access.

## Build and validation

Platform shells are generated with `huxerui platform add`. The library keeps iOS and macOS adapters in
`platform/ios/src` and `platform/macos/src` respectively. Their Objective-C++ implementations are compiled into the
CMake library target; the generated iOS Swift package remains the CLI's native-package integration point.

From `examples/preview`, with the corresponding HuxerUI SDK/toolchain installed:

```sh
huxerui build windows --profile debug
huxerui build android --profile debug
huxerui build web --profile debug
```

Use `huxerui run <platform>` for interactive testing. Android builds require the JDK expected by the generated Gradle
project; use the CLI's `--java-home` option if the system Java is different.

From the library root, in a configured native compiler environment:

```sh
cmake -G Ninja -S . -B build/tests -DHUXERUI_HOME=/path/to/sdk -DHUXERUI_MEDIAPLAYER_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build/tests
ctest --test-dir build/tests --output-on-failure
```

Tests cover shared state transitions, coalesced seeking/progress, looping, reentrant callbacks, stale-source/attachment
events, preferences, and surface ownership. Windows adds a real native audio smoke test using a temporary generated WAV
that is muted and removed afterward. If Node and the SDK Web registry are available, CTest also runs Web adapter tests
with real SDK payloads and a simulated media element. These do not replace browser/device playback tests.

Windows also tests fractional-pixel video cropping across fit modes, alignments, and output sizes. To exercise the
production video backend and window factory against a local video, run
`build/tests/tests/mediaplayer_windows_video.exe /path/to/video.mp4` after building the tests. This muted test uses an
off-screen window, checks native frame transfer/presentation after resizing, and waits for natural completion. It reads
the supplied file without modifying or copying it; use a short clip (under 20 seconds).

Manual acceptance should include local audio/video, a picker reference, HTTPS media, pause/seek/end/loop, a denied or
unsupported source, replacing media while loading/seeking, and hiding/remounting video without stopping audio. Verify
video fitting on narrow/wide windows, output on actual hardware, and platform interruptions separately.
