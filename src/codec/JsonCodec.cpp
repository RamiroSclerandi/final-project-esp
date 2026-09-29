#include "codec/JsonCodec.h"

#include "core/DeviceInfo.h"
#include "core/Version.h"

#include <ArduinoJson.h>

size_t JsonCodec::encode(const MeasurementAccumulator &readings,
                         const PayloadMeta &meta,
                         uint8_t *out, size_t outSize) const
{
    JsonDocument doc;

    doc["v"] = CONTRACT_VERSION;
    doc["dev"] = DeviceInfo::deviceId();
    doc["ts"] = DeviceInfo::epochNow();
    doc["seq"] = meta.sequence;

    JsonObject metaObj = doc["meta"].to<JsonObject>();
    metaObj["rssi"] = meta.rssi;
    metaObj["fw"] = FIRMWARE_VERSION;

    // Sequence numbers restart at zero on every boot, so on its own `seq` would
    // appear to run backwards after a restart. The boot counter orders the
    // sessions, which also makes unexpected reboots visible to the server.
    metaObj["boot"] = meta.bootCount;

    // Always emitted, including 0: the server cannot tell "no loss" from
    // "field absent" otherwise. Pre-emission only (seq was never assigned to
    // these), so it never explains a seq gap by itself — meta.store.drop is
    // the field to subtract from a gap to isolate true transport/broker loss
    // (G-10).
    metaObj["lost"] = meta.lostCount;

    // Lets the server flag readings whose timestamp is the arrival time rather
    // than the acquisition time, which are lower-quality data points.
    metaObj["ts_src"] = DeviceInfo::isClockSynced() ? "device" : "server";

    // Sent only on the first message after a restart. Distinguishes a brownout
    // (power supply) from a watchdog reset (hung software), which for an
    // unreachable node is the difference between checking the wiring and
    // checking the code.
    if (meta.resetReason != nullptr)
    {
        metaObj["rst"] = meta.resetReason;
    }

    if (meta.storeKind != nullptr)
    {
        JsonObject store = metaObj["store"].to<JsonObject>();
        store["k"] = meta.storeKind;
        store["pct"] = meta.storeUsedPct;
        store["pend"] = meta.storePending;

        // Post-emission loss (seq already assigned): eviction under space
        // pressure, or networkTask's send-then-reappend double failure. Both
        // open a seq gap, so subtracting this from a gap isolates true
        // transport/broker loss (G-10) — meta.lost never contributes here.
        store["drop"] = meta.storeDropped;
    }

    JsonArray channels = doc["ch"].to<JsonArray>();

    for (uint8_t i = 0; i < readings.channelCount(); i++)
    {
        const AggregatedChannel ch = readings.channelAt(i);

        JsonObject entry = channels.add<JsonObject>();
        entry["c"] = ch.channel;
        entry["u"] = unitToString(ch.unit);

        // Omitted when empty: an absent tag and an empty one mean the same
        // thing to the worker, and on LoRaWAN every byte counts.
        if (ch.tag != nullptr && ch.tag[0] != '\0')
        {
            entry["t"] = ch.tag;
        }

        entry["ok"] = ch.count > 0;
        entry["src"] = ch.source;

        // A failed channel is reported without a value rather than with a
        // placeholder, so no consumer can mistake a failure for a reading.
        if (ch.count > 0)
        {
            entry["val"] = ch.value;

            // Statistics are meaningful only when the window held more than one
            // sample. With one, they would repeat `val` and waste bytes.
            if (ch.count > 1)
            {
                entry["min"] = ch.minimum;
                entry["max"] = ch.maximum;
                entry["n"] = ch.count;
            }
        }
    }

    const size_t written = serializeJson(doc, (char *)out, outSize);

    // serializeJson truncates on overflow, which would emit malformed JSON.
    if (written == 0 || written >= outSize)
    {
        out[0] = '\0';
        return 0;
    }

    return written;
}

const char *JsonCodec::name() const
{
    return "json";
}
