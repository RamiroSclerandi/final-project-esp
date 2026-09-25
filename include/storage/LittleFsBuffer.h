#pragma once

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
 */
class LittleFsBuffer : public ILocalBuffer
{
public:
    /** Cap on the record file. Roughly 2000 messages of the current size. */
    static constexpr size_t MAX_BYTES = 200 * 1024;

    /** Rewrite the file once this many bytes have been consumed from the front. */
    static constexpr size_t COMPACT_THRESHOLD = 32 * 1024;

    bool begin() override;
    bool append(const uint8_t *payload, size_t length) override;
    bool hasPending() const override;
    size_t peekOldest(uint8_t *out, size_t outSize) override;
    bool dropOldest() override;
    uint8_t usedPercent() const override;
    uint32_t pendingCount() const override;
    uint32_t droppedCount() const override;
    const char *kind() const override;

private:
    /** Drops leading records until the file fits under MAX_BYTES. */
    void makeRoom(size_t incomingLength);

    /** Rewrites the file without the already-consumed prefix. */
    bool compact();

    void persistOffset();
    void recountPending();

    size_t _readOffset = 0;
    uint32_t _pending = 0;
    uint32_t _dropped = 0;
    bool _mounted = false;
};
