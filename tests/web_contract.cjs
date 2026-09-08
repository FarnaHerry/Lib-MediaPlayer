const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

const sdk = process.argv[2] || process.env.HUXERUI_HOME;
if (!sdk) throw new Error("Pass the installed HuxerUI SDK directory");
const released = [];
const revoked = [];
global.Module = {huxeruiWebFileReferenceRelease: handle => released.push(handle)};
vm.runInThisContext(fs.readFileSync(path.join(sdk, "lib/cmake/HuxerUI/HuxerUIWebPlatformRegistry.js"), "utf8"));

class Video extends EventTarget {
  constructor() {
    super();
    this.style = {};
    this.currentTime = 0;
    this.duration = NaN;
    this.videoWidth = 0;
    this.videoHeight = 0;
    this.readyState = 0;
    this.buffered = {length: 0};
    this.seekable = {length: 0};
    this.plays = 0;
    this.pauses = 0;
    this.playError = null;
  }
  load() {}
  pause() { ++this.pauses; }
  play() { ++this.plays; return this.playError ? Promise.reject(this.playError) : Promise.resolve(); }
  remove() { this.parentElement = null; }
  removeAttribute(name) { delete this[name]; }
  fire(name) { this.dispatchEvent(new Event(name)); }
}
global.document = {createElement: name => { assert.equal(name, "video"); return new Video(); }};
const revoke = URL.revokeObjectURL;
URL.revokeObjectURL = url => { revoked.push(url); revoke(url); };
vm.runInThisContext(fs.readFileSync(path.join(__dirname, "../platform/web/mediaplayer.js"), "utf8"));
const P = Module.HuxerUI.PlatformPayload;
const events = [];
const bundle = Module.createMediaPlayerBundle();
const player = bundle.factory.create(P.nullValue(), {emit(name, payload) {
  assert.equal(name, "media");
  events.push(payload);
}});
function invoke(method, fields = {}) {
  let completed = false;
  player.invoke(method, P.object({generation: P.int64(player.generation), ...fields}), {
    complete(result) { result.requireNull(); completed = true; },
    fail(code, message) { throw new Error(`${code}: ${message}`); },
  });
  assert.equal(completed, true);
}
function load(generation, kind = "http", value = P.string("https://example.test/media.mp4"), headers = {}) {
  invoke("load", {generation: P.int64(generation), kind: P.string(kind), value, headers: P.object(headers)});
}
const tick = () => new Promise(resolve => setImmediate(resolve));
const field = (name, payload = events.at(-1)) => payload.requireField(name);

(async () => {
  invoke("initialize");
  invoke("volume", {volume: P.doubleValue(0.4), muted: P.booleanValue(true)});
  load(1n);
  const video = player.video;
  assert.equal(video.volume, 0.4);
  assert.equal(video.muted, true);
  video.duration = 10;
  video.readyState = 1;
  video.fire("loadedmetadata");
  assert.equal(field("kind").requireString(), "ready");
  assert.equal(field("duration").requireDouble(), 10);
  assert.equal(field("seekable").isNull(), true);
  video.readyState = 4;
  video.seekable.length = 1;
  video.currentTime = 2;
  invoke("poll");
  assert.equal(field("position").requireDouble(), 2);
  assert.equal(field("seekable").requireBoolean(), true);
  assert.equal(field("audio").isNull(), true);
  player.playNow();
  assert.equal(video.plays, 1, "Play must reach the element before returning to preserve user activation");
  invoke("seek", {seekId: P.int64(3n), position: P.doubleValue(5)});
  video.fire("seeked");
  assert.equal(field("kind").requireString(), "seeked");
  assert.equal(field("seekId").requireInt64(), 3n);
  const host = {appendChild(child) { child.parentElement = this; }};
  player.attach(host, {fit: "cover", horizontal: "end", vertical: "start"});
  assert.equal(video.style.objectPosition, "100% 0%");
  assert.equal(video.parentElement, host);
  const pauses = video.pauses;
  player.detach();
  assert.equal(video.pauses, pauses, "Video detachment must not issue Pause");
  video.playError = {name: "NotAllowedError"};
  player.playNow();
  await tick();
  assert.equal(field("code").requireString(), "playbackNotAllowed");
  assert.equal(field("fatal").requireBoolean(), false);
  load(2n);
  const eventCount = events.length;
  video.fire("ended");
  video.fire("error");
  assert.equal(events.length, eventCount, "Old elements must not publish events");
  load(3n, "http", P.string("https://example.test/media.mp4"), {Authorization: P.string("test")});
  assert.equal(field("code").requireString(), "unsupported");
  assert.equal(player.video, null);
  load(4n, "file", P.string("file:///media.mp4"));
  assert.equal(field("code").requireString(), "unsupported");
  const bridge = Module.huxerUIWebPlatformBridge;
  const reference = bridge.createFileReference(1, 1, {file: new Blob(["test"])}, true);
  load(5n, "reference", P.fileReference(reference));
  await tick();
  const objectUrl = player.objectUrl;
  assert.ok(objectUrl.startsWith("blob:"));
  assert.equal(released.includes(1), false);
  invoke("clear");
  assert.ok(released.includes(1));
  assert.ok(revoked.includes(objectUrl));
  let resolveFile;
  const pending = bridge.createFileReference(2, 2, {handle: {getFile: () => new Promise(resolve => { resolveFile = resolve; })}}, true);
  load(6n, "reference", P.fileReference(pending));
  load(7n);
  const currentVideo = player.video;
  resolveFile(new Blob(["old"]));
  await tick();
  assert.equal(player.video, currentVideo, "Late FileReference resolution replaced the active source");
  assert.ok(released.includes(2));
  player.dispose();
  player.dispose();
  assert.equal(player.video, null);
  const finalCount = events.length;
  currentVideo.fire("ended");
  assert.equal(events.length, finalCount);
  assert.deepEqual(released, [1, 2]);
  console.log("Web adapter contract tests passed (real SDK payloads, simulated media element)");
})().catch(error => { console.error(error); process.exitCode = 1; });
