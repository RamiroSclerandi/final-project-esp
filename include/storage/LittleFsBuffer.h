#pragma once

#include <atomic>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "storage/ILocalBuffer.h"

/**
 * @brief Local buffer backed by the ESP32's internal flash partition.
 *
 * Always available — no extra hardware — which makes it the fallback when no SD
 * card is fitted or the card fails.
 *
 * Records are stored as newline-delimited JSON. That format was chosen over a
 * binary file with an index because a power cut mid-write corrupts at most the
 * last line, which is discarded on read; a half-updated binary index would
 * corrupt the whole file.
 *
 * Internal flash endures roughly 100k erase cycles per sector, so this is
 * contingency storage, not normal operation: records are only written when a
 * transmission has failed. With the link up, nothing is written at all.
 *
 * The sensor task (core 0) appends while the network task (core 1) peeks and
 * drops, so every public file operation is guarded by one non-recursive
 * mutex, created lazily in begin(). Private helpers assume the lock is
 * already held and never take it themselves. Before begin() runs, or if
 * the lock cannot be taken within LOCK_TIMEOUT_MS, a call fails closed
 * (returns false/0) rather than proceeding unguarded.
 */
class LittleFsBuffer : public ILocalBuffer
{
public:
    /** Cap on the record file. Roughly 2000 messages of the current size. */
    static constexpr size_t MAX_BYTES = 200 * 1024;

    /** Rewrite the file once this many bytes have been consumed from the front. */
    static constexpr size_t COMPACT_THRESHOLD = 32 * 1024;

    /** Longest wait for the lock; on timeout the operation fails, never blocks. */
    static constexpr uint32_t LOCK_TIMEOUT_MS = 5000;

    LittleFsBuffer() = default;
    LittleFsBuffer(const LittleFsBuffer &) = delete;
    LittleFsBuffer &operator=(const LittleFsBuffer &) = delete;
    ~LittleFsBuffer() override;

    bool begin() override;
    bool append(const uint8_t *payload, size_t length) override;
    bool hasPending() const override;
    size_t peekOldest(uint8_t *out, size_t outSize) override;
    /**
     * @brief Drops the record returned by the last peekOldest(). A no-op
     *        returning false if an eviction removed that record meanwhile,
     *        so a late confirmation never discards an unsent record.
     */
    bool dropOldest() override;
    uint8_t usedPercent() const override;
    uint32_t pendingCount() const override;
    uint32_t droppedCount() const override;
    void recordEmittedLoss() override;
    const char *kind() const override;

private:
    /** Mounts the filesystem and restores the read offset and pending count. */
    bool mount();

    /** Drops leading records until the file fits under MAX_BYTES. */
    void makeRoom(size_t incomingLength);
    void noteEviction();

    /**
     * @brief Logs a lock timeout distinctly from a genuinely full/empty
     *        buffer, so an operator reading serial output does not mistake
     *        contention for capacity.
     */
    void logLockTimeout(const char *operation) const;

    /** dropOldest() without the lock or the peek-token check. */
    bool dropOldestLocked();

    /** Rewrites the file without the already-consumed prefix. */
    bool compact();

    void persistOffset();
    void recountPending();

    SemaphoreHandle_t _mutex = nullptr;
    size_t _readOffset = 0;

    // Atomic so meta telemetry can read them from the other core lock-free.
    std::atomic<uint32_t> _pending{0};

    // Two separate counters, summed by droppedCount() for meta.store.drop:
    // _evicted counts makeRoom() evictions; the in-flight peeked record is
    // counted only once its send is known to have failed (next peekOldest()),
    // never counted and later undone, so droppedCount() is monotonic. Keeping it apart from
    // _emittedLoss also means a recordEmittedLoss() between peek() and dropOldest() can never be
    // mistaken for an eviction of the record that was just peeked.
    std::atomic<uint32_t> _evicted{0};
    std::atomic<uint32_t> _emittedLoss{0};
    std::atomic<bool> _mounted{false};

    // Cached so a lock timeout in usedPercent() can report the last known
    // capacity instead of misreporting contention as an empty buffer.
    // mutable: written from usedPercent(), which is otherwise a pure read.
    mutable std::atomic<uint8_t> _lastUsedPercent{0};

    // Set by makeRoom() when it evicts the record peeked by the last peek();
    // dropOldest() then knows the peeked record is gone and keeps the next one.
    bool _peekEvicted = false;
    bool _hasPeekToken = false;
};
