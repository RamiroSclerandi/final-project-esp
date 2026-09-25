#pragma once

#include "codec/IPayloadCodec.h"

/**
 * @brief Encodes readings as `datalogger.v1` JSON.
 *
 * Every channel becomes one array element, which maps one-to-one onto a row of
 * the `measurements` table, so the ingest worker needs no per-sensor knowledge.
 *
 * Human-readable and self-describing, at the cost of size: a full message does
 * not fit in a LoRaWAN uplink. A compact binary codec covers that case.
 */
class JsonCodec : public IPayloadCodec
{
public:
    static constexpr uint8_t CONTRACT_VERSION = 1;

    size_t encode(const MeasurementAccumulator &readings, const PayloadMeta &meta,
                  uint8_t *out, size_t outSize) const override;

    const char *name() const override;
};
