package org.huxerui.lib.mediaplayer;

import androidx.media3.exoplayer.DefaultLoadControl;

/**
 * 带「最多预取多少秒」上限的 LoadControl（2026-09-29，B4 第一步）。
 *
 * <p>为什么需要它：{@link DefaultLoadControl} 的默认目标是
 * {@code minBufferMs = maxBufferMs = 50_000}（约 50 秒媒体），这对**用户正在看的那一条**是对的，
 * 对「先装好、等着被晋升」的邻居会话是错的 —— 一条**暂停**的会话照样会把那 50 秒拉下来。
 * 连刷的牌堆里同一时刻有两幅是这种邻居，它们把带宽吃满，正在播的那一条就卡。
 *
 * <p>给这种会话一个小上限，它拉够就停；晋升成主会话时把上限放开（{@code 0} = 不额外限制），
 * 它会自己接着拉。这正是 Media3 给预加载角色准备的那套语义（独立的小字节预算 + 动态让步），
 * 只是入口在 ExoPlayer 这边一直没暴露出来。
 */
final class BudgetLoadControl extends DefaultLoadControl {

    /** 上限（微秒）；0 = 不额外限制，完全交给父类的默认策略。 */
    private volatile long budgetUs;

    BudgetLoadControl(long budgetUs) {
        this.budgetUs = budgetUs;
    }

    void setBudgetUs(long value) {
        budgetUs = Math.max(0L, value);
    }

    @Override
    public boolean shouldContinueLoading(Parameters parameters) {
        // 先让父类跑一遍：它要在内部记账（isLoading / 分配器），跳过它会让后面的判断失真。
        final boolean wanted = super.shouldContinueLoading(parameters);
        final long cap = budgetUs;
        if (cap > 0L && parameters.bufferedDurationUs >= cap) {
            return false;  // 预算到顶：先不拉了（已经拉到的还留着，播放不受影响）
        }
        return wanted;
    }
}
