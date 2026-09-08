(() => {
  Module["createMediaPlayerBundle"] = () => {
    const P = Module.HuxerUI.PlatformPayload;
    const holder = {instance: null};

    class Player {
      constructor(events) {
        this.events = events;
        this.generation = 0n;
        this.seekId = 0n;
        this.video = null;
        this.host = null;
        this.reference = null;
        this.objectUrl = null;
        this.volume = 1;
        this.muted = false;
        this.buffering = false;
        this.ready = false;
        this.disposed = false;
        this.properties = {fit: "contain", horizontal: "center", vertical: "center"};
      }

      error(code, message, fatal, platformCode = "") {
        if (this.disposed) return;
        this.events.emit("media", P.object({
          kind: P.string("error"), generation: P.int64(this.generation), seekId: P.int64(this.seekId),
          code: P.string(code), message: P.string("HuxerUI media: " + message),
          fatal: P.booleanValue(fatal), platformCode: P.string(platformCode),
        }));
      }

      send(kind) {
        const video = this.video;
        if (!video || this.disposed) return;
        const duration = Number.isFinite(video.duration) && video.duration >= 0 ? video.duration : null;
        const buffered = [];
        for (let i = 0; i < video.buffered.length; ++i) {
          buffered.push(P.list([P.doubleValue(video.buffered.start(i)), P.doubleValue(video.buffered.end(i))]));
        }
        this.events.emit("media", P.object({
          kind: P.string(kind), generation: P.int64(this.generation), seekId: P.int64(this.seekId),
          position: P.doubleValue(Number.isFinite(video.currentTime) ? Math.max(0, video.currentTime) : 0),
          duration: duration === null ? P.nullValue() : P.doubleValue(duration), buffered: P.list(buffered),
          seekable: this.ready && (video.seekable.length > 0 || video.readyState >= 3) ? P.booleanValue(duration !== null && video.seekable.length > 0) : P.nullValue(),
          audio: this.ready && video.audioTracks ? P.booleanValue(video.audioTracks.length > 0) : P.nullValue(),
          video: this.ready ? P.booleanValue(video.videoWidth > 0 && video.videoHeight > 0) : P.nullValue(),
          width: video.videoWidth > 0 ? P.doubleValue(video.videoWidth) : P.nullValue(),
          height: video.videoHeight > 0 ? P.doubleValue(video.videoHeight) : P.nullValue(),
          buffering: P.booleanValue(this.buffering),
        }));
      }

      clear() {
        this.ready = false;
        this.buffering = false;
        this.seekId = 0n;
        if (this.listeners) this.listeners.abort();
        this.listeners = null;
        const video = this.video;
        this.video = null;
        if (video) {
          video.pause();
          video.removeAttribute("src");
          video.load();
          video.remove();
        }
        if (this.objectUrl) URL.revokeObjectURL(this.objectUrl);
        this.objectUrl = null;
        if (this.reference) this.reference.close();
        this.reference = null;
      }

      async load(args) {
        this.clear();
        this.generation = args.requireField("generation").requireInt64();
        const generation = this.generation;
        if (args.requireField("headers").fields().size) {
          this.error("unsupported", "Custom HTTP headers are unavailable for browser media elements", true);
          return;
        }
        const kind = args.requireField("kind").requireString();
        let source;
        if (kind === "file") {
          this.error("unsupported", "Use FileReference for browser files; virtual filesystem paths are not media URLs", true);
          return;
        }
        if (kind === "reference") {
          const reference = args.requireField("value").requireFileReference();
          this.reference = reference;
          try {
            const file = await reference.getFile();
            if (this.disposed || this.generation !== generation || this.reference !== reference) return;
            this.objectUrl = URL.createObjectURL(file);
            source = this.objectUrl;
          } catch (_) {
            if (!this.disposed && this.generation === generation && this.reference === reference) {
              this.error("permissionDenied", "The browser file could not be opened", true);
            }
            return;
          }
        } else source = args.requireField("value").requireString();

        const video = document.createElement("video");
        this.video = video;
        video.controls = false;
        video.playsInline = true;
        video.preload = "auto";
        video.volume = this.volume;
        video.muted = this.muted;
        this.listeners = new AbortController();
        const listen = (name, callback) => video.addEventListener(name, () => {
          if (!this.disposed && this.video === video && this.generation === generation) callback();
        }, {signal: this.listeners.signal});
        listen("loadedmetadata", () => { this.ready = true; this.send("ready"); });
        listen("playing", () => { this.buffering = false; this.send("playing"); });
        listen("pause", () => { if (!video.ended) this.send("paused"); });
        listen("waiting", () => { this.buffering = true; this.send("update"); });
        listen("canplay", () => { this.buffering = false; this.send("update"); });
        listen("seeked", () => { if (this.seekId) { this.send("seeked"); this.seekId = 0n; } });
        listen("ended", () => this.send("ended"));
        listen("durationchange", () => { if (this.ready) this.send("update"); });
        listen("resize", () => { if (this.ready) this.send("update"); });
        listen("error", () => {
          const code = video.error ? video.error.code : 0;
          this.error(code === 2 ? "network" : code === 3 ? "decode" : code === 4 ? "unsupported" : "unknown",
            "The browser could not play the media source", true, String(code));
        });
        this.applyVideo();
        video.src = source;
        video.load();
      }

      playNow() {
        const video = this.video;
        const generation = this.generation;
        if (!video || this.disposed) return;
        video.play().catch(error => {
          if (this.disposed || this.video !== video || this.generation !== generation || error.name === "AbortError") return;
          this.error(error.name === "NotAllowedError" ? "playbackNotAllowed" : "output",
            "The browser rejected playback", false, error.name || "");
        });
      }

      pauseNow() {
        if (this.video) { this.video.pause(); this.send("paused"); }
      }

      attach(host, properties) {
        this.host = host;
        this.properties = properties;
        this.applyVideo();
      }
      detach() {
        if (this.video) this.video.remove();
        this.host = null;
      }
      applyVideo() {
        if (!this.video) return;
        const p = this.properties;
        Object.assign(this.video.style, {
          width: "100%", height: "100%", display: "block", objectFit: p.fit,
          objectPosition: `${p.horizontal === "start" ? "0%" : p.horizontal === "end" ? "100%" : "50%"} ${p.vertical === "start" ? "0%" : p.vertical === "end" ? "100%" : "50%"}`,
        });
        if (this.host && this.video.parentElement !== this.host) this.host.appendChild(this.video);
      }

      invoke(method, args, result) {
        if (this.disposed) { result.fail("media/disposed", "Media session is disposed", P.nullValue()); return; }
        if (!["initialize", "load", "volume"].includes(method) && args.requireField("generation").requireInt64() !== this.generation) {
          result.complete(P.nullValue());
          return;
        }
        switch (method) {
          case "initialize": break;
          case "load": {
            const generation = args.requireField("generation").requireInt64();
            this.load(args).catch(() => {
              if (!this.disposed && this.generation === generation) this.error("invalidSource", "The media source could not be prepared", true);
            });
            break;
          }
          case "clear": this.clear(); break;
          case "play": this.playNow(); break;
          case "pause": this.pauseNow(); break;
          case "volume":
            this.volume = args.requireField("volume").requireDouble();
            this.muted = args.requireField("muted").requireBoolean();
            if (this.video) { this.video.volume = this.volume; this.video.muted = this.muted; }
            break;
          case "poll": if (this.ready) this.send("update"); break;
          case "seek":
            if (this.video) {
              this.seekId = args.requireField("seekId").requireInt64();
              try {
                const position = args.requireField("position").requireDouble();
                if (Math.abs(this.video.currentTime - position) < 1e-6) { this.send("seeked"); this.seekId = 0n; }
                else this.video.currentTime = position;
              } catch (_) { this.error("seekFailed", "The browser rejected the seek", false); this.seekId = 0n; }
            }
            break;
          default: result.fail("media/unknown-command", "Unknown media command", P.nullValue()); return;
        }
        result.complete(P.nullValue());
      }

      dispose() {
        if (this.disposed) return;
        this.disposed = true;
        this.clear();
        this.host = null;
      }
    }

    return {holder, factory: {create(options, events) {
      options.requireNull();
      holder.instance = new Player(events);
      return holder.instance;
    }}};
  };
})();
