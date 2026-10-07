package org.huxerui.lib.mediaplayer;

import android.content.Context;
import androidx.media3.database.StandaloneDatabaseProvider;
import androidx.media3.datasource.cache.LeastRecentlyUsedCacheEvictor;
import androidx.media3.datasource.cache.SimpleCache;
import org.huxerui.HuxerUIPlatformChannel;
import org.huxerui.HuxerUIPlatformModule;
import org.huxerui.PlatformPayload;
import java.io.File;
import java.util.HashMap;
import java.util.Map;

/** 播放过的字节跨播放器复用；同一目录只能有一个 SimpleCache，租约归还后才释放目录锁。 */
public final class NativeHttpCache {
    private static final Map<String, Entry> entries = new HashMap<>();

    static final class Entry implements AutoCloseable {
        final String path;
        final SimpleCache cache;
        private int references;

        Entry(Context context, String path, long budgetBytes) {
            this.path = path;
            cache = new SimpleCache(new File(path), new LeastRecentlyUsedCacheEvictor(budgetBytes),
                    new StandaloneDatabaseProvider(context.getApplicationContext()));
        }

        @Override
        public void close() {
            synchronized (entries) {
                if (--references == 0) {
                    entries.remove(path);
                    cache.release();
                }
            }
        }
    }

    static Entry acquire(Context context, String directory, long budgetBytes) {
        final String path = new File(directory).getAbsolutePath();
        synchronized (entries) {
            Entry entry = entries.get(path);
            if (entry == null) {
                entry = new Entry(context, path, Math.max(1L, budgetBytes));
                entries.put(path, entry);
            }
            ++entry.references;
            return entry;
        }
    }

    /** 清缓存必须经 SimpleCache 同步索引，不能在它活着时从 C++ 直接删 .exo 文件。 */
    public static final class Factory implements HuxerUIPlatformModule.Factory {
        @Override
        public HuxerUIPlatformModule create(Context context, PlatformPayload options,
                HuxerUIPlatformChannel.Events events) {
            return new HuxerUIPlatformModule() {
                @Override
                public HuxerUIPlatformChannel.Cancellation invoke(String method, PlatformPayload args,
                        HuxerUIPlatformChannel.Result result) {
                    if (!method.equals("clear")) {
                        result.fail("cache/unknown", "Unknown cache command", PlatformPayload.nullValue());
                        return null;
                    }
                    final String directory = args.requireString();
                    if (new File(directory).isDirectory()) {
                        try (Entry entry = acquire(context, directory, 256L * 1024L * 1024L)) {
                            for (String key : entry.cache.getKeys()) entry.cache.removeResource(key);
                        } catch (RuntimeException error) {
                            result.fail("cache/clear", error.toString(), PlatformPayload.nullValue());
                            return null;
                        }
                    }
                    result.complete(PlatformPayload.nullValue());
                    return null;
                }

                @Override
                public void dispose() {}
            };
        }
    }
}
