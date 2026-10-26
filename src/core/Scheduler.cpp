// INTERNAL. See Scheduler.hpp.
#include "Scheduler.hpp"

namespace pbuf::detail {

namespace {

// ---------------------------------------------------------------------------
// LegacyScheduler: DERIVED from Predictive Buffer v4.6 (m_waitTicks machine).
// ---------------------------------------------------------------------------
class LegacyScheduler final : public IScheduler {
public:
    bool inFlight() const override { return wait_ >= 0; }
    bool busy() const override { return wait_ >= 0 || wait_ == -2; }

    bool absorb(bool push, Tick) override {
        // v4.6: remember only whether the LAST input differs from the buffered one.
        // This is why press->release->press collapses into a single press.
        queuedOpposite_ = (push != bufferedPush_);
        return true;
    }

    bool takeDueOpposite(bool& pushOut) override {
        if (wait_ != -2) return false;
        wait_ = -1;
        queuedOpposite_ = false;
        pushOut = !bufferedPush_;
        return true;
    }

    void dropQueuedOpposite() override { queuedOpposite_ = false; }

    void schedule(bool push, Tick delay, Tick) override {
        bufferedPush_ = push;
        wait_ = delay;
    }

    bool tick(Tick, IFireSink& sink) override {
        if (wait_ == 0) {
            sink.fire(bufferedPush_);
            if (queuedOpposite_) { wait_ = -2; queuedOpposite_ = false; }
            else                 { wait_ = -1; }
            return false;
        }
        if (wait_ > 0) { --wait_; return false; }
        if (wait_ == -2) {
            sink.fire(!bufferedPush_);
            queuedOpposite_ = false;
            wait_ = -1;
            return false;
        }
        return true; // wait_ == -1: idle
    }

    void clear() override { wait_ = -1; queuedOpposite_ = false; }

    void flush(IFireSink& sink) override {
        bool const pendingMain = wait_ >= 0;
        bool const pendingOpp  = queuedOpposite_ || wait_ == -2;
        bool const mainPush = bufferedPush_;
        clear();
        if (pendingMain) sink.fire(mainPush);
        if (pendingOpp)  sink.fire(!mainPush);
    }

private:
    int  wait_ = -1;            // -1 idle, >=0 countdown, -2 queued opposite due
    bool bufferedPush_ = true;
    bool queuedOpposite_ = false;
};

// ---------------------------------------------------------------------------
// OrderedScheduler: NEW. Fixed-capacity FIFO, uniform shift, no allocation.
// ---------------------------------------------------------------------------
class OrderedScheduler final : public IScheduler {
public:
    bool inFlight() const override { return count_ > 0; }
    bool busy() const override { return count_ > 0; }

    bool absorb(bool push, Tick now) override {
        if (count_ == kCapacity) return false;
        // Same shift as the input that opened the burst => spacing is preserved.
        pushItem(now + shift_ + 1, push);
        return true;
    }

    bool takeDueOpposite(bool&) override { return false; }
    void dropQueuedOpposite() override {}

    void schedule(bool push, Tick delay, Tick now) override {
        count_ = 0; head_ = 0;
        shift_ = delay;
        // An input arriving before the update numbered now+1 is applied before the
        // update numbered now+delay+1, i.e. after `delay` full updates (matches the
        // probe, which applies the input after `delay` simulated ticks).
        pushItem(now + delay + 1, push);
    }

    bool tick(Tick now, IFireSink& sink) override {
        bool const idle = count_ == 0;
        while (count_ > 0 && items_[head_].due <= now) {
            bool const p = items_[head_].push;
            head_ = (head_ + 1) % kCapacity;
            --count_;
            sink.fire(p);
        }
        return idle;
    }

    void clear() override { count_ = 0; head_ = 0; }

    void flush(IFireSink& sink) override {
        while (count_ > 0) {
            bool const p = items_[head_].push;
            head_ = (head_ + 1) % kCapacity;
            --count_;
            sink.fire(p);
        }
        head_ = 0;
    }

private:
    static constexpr int kCapacity = 32; // 32 inputs inside one buffer window is > 100 Hz of input
    struct Item { Tick due; bool push; };

    void pushItem(Tick due, bool push) {
        items_[(head_ + count_) % kCapacity] = {due, push};
        ++count_;
    }

    Item items_[kCapacity] = {};
    int  head_ = 0;
    int  count_ = 0;
    Tick shift_ = 0;
};

} // namespace

std::unique_ptr<IScheduler> makeScheduler(QueueMode mode) {
    if (mode == QueueMode::LegacySingleSlot) return std::make_unique<LegacyScheduler>();
    return std::make_unique<OrderedScheduler>();
}

} // namespace pbuf::detail
