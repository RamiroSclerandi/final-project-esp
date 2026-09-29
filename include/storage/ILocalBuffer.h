#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Durable store for payloads that could not be transmitted yet.
 *
 * This is what turns the device from something that publishes into something
 * that does not lose data: without it, a network outage longer than the in-RAM
 * queue silently discards readings.
 *
 * Records are written before transmission and removed only after a confirmed
 * send. That ordering can duplicate a record if the send succeeded but the
 * removal did not — which is the right trade, because the database deduplicates
 * on (sensor_id, timestamp) while a lost reading is gone for good.
 */
class ILocalBuffer
{
public:
    virtual ~ILocalBuffer() = default;

    /** @return true if the backing store mounted and is usable. */
    virtual bool begin() = 0;

    /** @brief Append one record. Drops the oldest records if full. */
    virtual bool append(const uint8_t *payload, size_t length) = 0;

    virtual bool hasPending() const = 0;

    /**
     * @brief Copy the oldest pending record without removing it.
     * @return Bytes written to `out`, or 0 if there is nothing pending.
     */
    virtual size_t peekOldest(uint8_t *out, size_t outSize) = 0;

    /** @brief Remove the oldest record. Call only after a confirmed send. */
    virtual bool dropOldest() = 0;

    /** @return Percentage of the store's capacity in use, 0-100. */
    virtual uint8_t usedPercent() const = 0;

    virtual uint32_t pendingCount() const = 0;

    /**
     * @return Records discarded from local storage: evicted because the
     *         store filled up, or lost after networkTask's send failed and
     *         its own re-append also failed (see recordEmittedLoss()).
     *
     * A datalogger keeps recent data over old data, but the loss must be
     * visible and counted rather than silent.
     */
    virtual uint32_t droppedCount() const = 0;

    /**
     * @brief Counts a record whose `seq` was already stamped (handed to
     *        transport/buffer, G-10) but that networkTask could not persist
     *        after its send failed too.
     *
     * This is a post-emission device-side loss — the same category as an
     * eviction (both leave the record gone from local storage), so it is
     * folded into the same counter as droppedCount() instead of the
     * pre-emission `meta.lost` count: `seq` was already advanced for this
     * record, so the loss must be visible as `meta.store.drop`, not `lost`.
     */
    virtual void recordEmittedLoss() = 0;

    /** @return "sd", "littlefs" or "none". Reported in every message so a node
     *          that fell back to internal flash says so. */
    virtual const char *kind() const = 0;
};
