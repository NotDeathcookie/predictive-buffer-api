// INTERNAL. Holds back inputs and releases them on schedule.
//
// Two implementations behind one interface:
//
//  * LegacyScheduler  - DERIVED from Predictive Buffer v4.6: the integer state
//    machine `m_waitTicks` (-1 idle, >=0 countdown, -2 "queued opposite due")
//    plus `m_bufferedPush` / `m_queuedOpposite`. Kept bit-faithful (including its
//    defects) so Config::classic() can be verified against the original.
//
//  * OrderedScheduler - NEW. A small fixed-capacity FIFO. Every input arriving
//    while another is held back is shifted by the same delay, so order and
//    spacing are preserved. No heap allocation.
#pragma once

#include <pbuf/Types.hpp>

#include <memory>

namespace pbuf::detail {

struct IFireSink {
    virtual ~IFireSink() = default;
    virtual void fire(bool push) = 0;
};

class IScheduler {
public:
    virtual ~IScheduler() = default;

    /// An input is currently being held back (new inputs must queue behind it).
    virtual bool inFlight() const = 0;
    /// inFlight(), or legacy "opposite action due next tick". Used for the
    /// player-2 swallow rule.
    virtual bool busy() const = 0;

    /// A player-1 input arrived while inFlight(). Returns true if it was queued
    /// (caller swallows it); false if the queue is full (caller must flush()).
    virtual bool absorb(bool push, Tick now) = 0;

    /// Legacy only: if a queued opposite action is due, consume it and report
    /// which action to fire first. Ordered: always false.
    virtual bool takeDueOpposite(bool& pushOut) = 0;

    /// Legacy only: forget a not-yet-armed queued opposite.
    virtual void dropQueuedOpposite() = 0;

    /// Begin holding back `push` for `delay` ticks. `now` = Engine tick counter
    /// at arrival. Precondition: !inFlight().
    virtual void schedule(bool push, Tick delay, Tick now) = 0;

    /// Advance one physics tick (called at the start of each player-1 update with
    /// the already-incremented counter). Fires due inputs via `sink`.
    /// Returns true if the scheduler was idle at entry (failsafe may run).
    virtual bool tick(Tick now, IFireSink& sink) = 0;

    /// Forget everything pending without delivering it.
    virtual void clear() = 0;

    /// Deliver everything pending immediately, in order, then clear.
    virtual void flush(IFireSink& sink) = 0;
};

std::unique_ptr<IScheduler> makeScheduler(QueueMode mode);

} // namespace pbuf::detail
