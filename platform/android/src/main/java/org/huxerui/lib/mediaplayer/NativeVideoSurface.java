package org.huxerui.lib.mediaplayer;

import android.content.Context;
import android.graphics.Color;
import android.view.TextureView;
import android.view.View;
import android.widget.FrameLayout;

import androidx.media3.common.VideoSize;
import androidx.media3.exoplayer.ExoPlayer;

import org.huxerui.HuxerUIPlatformChannel;
import org.huxerui.HuxerUIPlatformView;
import org.huxerui.PlatformPayload;

public final class NativeVideoSurface implements HuxerUIPlatformView {
    NativeMediaPlayer session;
    private final TextureView texture;
    private final View shutter;
    private final FrameLayout host;
    private ExoPlayer attached;
    private boolean firstFrameRendered;
    private String fit = "contain", horizontal = "center", vertical = "center";

    public static final class Factory implements HuxerUIPlatformView.Factory {
        public Factory() {}
        @Override
        public HuxerUIPlatformView create(
                Context context, PlatformPayload properties, HuxerUIPlatformChannel.Events events) {
            return new NativeVideoSurface(context, properties);
        }
    }

    private NativeVideoSurface(Context context, PlatformPayload properties) {
        texture = new TextureView(context);
        shutter = new View(context);
        shutter.setBackgroundColor(Color.BLACK);
        host = new FrameLayout(context) {
            @Override
            protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
                layoutVideo();
                shutter.layout(0, 0, host.getWidth(), host.getHeight());
            }
        };
        host.setBackgroundColor(Color.BLACK);
        host.setClipChildren(true);
        host.addView(texture, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
        // 纹理必须能创建 SurfaceTexture 才能解码；用遮挡层等首帧，不能靠隐藏纹理等尺寸。
        host.addView(shutter, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
        texture.setVisibility(View.INVISIBLE);
        update(properties);
    }

    @Override
    public View getView() {
        return host;
    }
    @Override
    public void update(PlatformPayload properties) {
        fit = properties.requireField("fit").requireString();
        horizontal = properties.requireField("horizontal").requireString();
        vertical = properties.requireField("vertical").requireString();
        refresh();
    }
    void attachPlayer(ExoPlayer player) {
        if (attached == player) {
            refresh();
            return;
        }
        firstFrameRendered = false;
        shutter.setVisibility(View.VISIBLE);
        if (attached != null)
            attached.clearVideoTextureView(texture);
        attached = player;
        if (player != null)
            player.setVideoTextureView(texture);
        refresh();
    }
    void refresh() {
        // 缓存首帧可能早于下一次布局；先同步更新比例，不能 requestLayout 后就显示整屏纹理。
        texture.setVisibility(attached != null ? View.VISIBLE : View.INVISIBLE);
        layoutVideo();
        host.requestLayout();
    }
    void firstFrameRendered(ExoPlayer player) {
        if (attached != player) return;
        firstFrameRendered = true;
        refresh();
    }
    private void layoutVideo() {
        VideoSize size = attached == null ? VideoSize.UNKNOWN : attached.getVideoSize();
        if (size.width <= 0 || size.height <= 0 || host.getWidth() <= 0 || host.getHeight() <= 0) {
            shutter.setVisibility(View.VISIBLE);
            texture.layout(0, 0, host.getWidth(), host.getHeight());
            return;
        }
        float width = size.width * size.pixelWidthHeightRatio;
        float height = size.height;
        float sx = host.getWidth() / width, sy = host.getHeight() / height;
        float scale = 1;
        if (fit.equals("contain"))
            scale = Math.min(sx, sy);
        else if (fit.equals("cover"))
            scale = Math.max(sx, sy);
        else if (fit.equals("scale-down"))
            scale = Math.min(1, Math.min(sx, sy));
        width = fit.equals("fill") ? host.getWidth() : width * scale;
        height = fit.equals("fill") ? host.getHeight() : height * scale;
        int x = Math.round((host.getWidth() - width)
                * (horizontal.equals("start")              ? 0
                                : horizontal.equals("end") ? 1
                                                           : 0.5f));
        int y = Math.round((host.getHeight() - height)
                * (vertical.equals("start")              ? 0
                                : vertical.equals("end") ? 1
                                                         : 0.5f));
        texture.layout(x, y, x + Math.round(width), y + Math.round(height));
        // 首帧、真实画幅与宿主尺寸都就绪，而且 TextureView 已是最终比例，才撤遮挡。
        // 暂停/SeekTo(0)/同会话晋升不重置首帧标记，保留邻居已经画出的那一帧。
        shutter.setVisibility(firstFrameRendered ? View.INVISIBLE : View.VISIBLE);
    }
    @Override
    public HuxerUIPlatformChannel.Cancellation invoke(
            String method, PlatformPayload args, HuxerUIPlatformChannel.Result result) {
        if (method.equals("bind")) {
            final NativeMediaPlayer wanted = NativeMediaPlayer.find(args.requireInt64());
            // 同会话重复绑定不拆纹理、不清首帧标记；角色晋升不能重新盖住已预热的画面。
            if (session != wanted || session == null || session.surface != this) {
                unbind();
                session = wanted;
            }
            if (session == null) {
                result.fail("media/disconnected", "Media session is disconnected", PlatformPayload.nullValue());
                return null;
            }
            session.surface = this;
            attachPlayer(session.player);
        } else if (method.equals("unbind"))
            unbind();
        else {
            result.fail("media/unknown-command", "Unknown video command", PlatformPayload.nullValue());
            return null;
        }
        result.complete(PlatformPayload.nullValue());
        return null;
    }
    private void unbind() {
        attachPlayer(null);
        if (session != null && session.surface == this)
            session.surface = null;
        session = null;
    }
    @Override
    public void dispose() {
        unbind();
    }
}
