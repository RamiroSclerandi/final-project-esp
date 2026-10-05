#pragma once

#include <algorithm>
#include <iterator>
#include <stdint.h>

#include "transport/MqttTimeouts.h"

/**
 * @brief Splits one MQTT connection attempt into stages that each fit between
 *        two task-watchdog feeds.
 *
 * Run back to back with a single feed, DNS + TCP + TLS + CONNECT/CONNACK +
 * the status publish and config subscribe can block longer than the 30 s
 * watchdog on a bad network and panic the board. Feeding before every stage
 * bounds each unfed stretch to one stage instead of the whole attempt.
 */
namespace ConnectSequence
{
    enum class Stage : uint8_t
    {
        ResolveHost,   ///< DNS lookup of the broker host.
        OpenTls,       ///< TCP connect + TLS handshake (certificate checked against the host).
        MqttHandshake, ///< MQTT CONNECT write + CONNACK wait.
        Announce,      ///< Retained "online" status publish + config subscribe.
    };

    constexpr Stage STAGES[] = {Stage::ResolveHost, Stage::OpenTls, Stage::MqttHandshake,
                                Stage::Announce};

    /**
     * @return Worst-case seconds `stage` can block, from the bounds in
     *         MqttTimeouts.h. Writes are bounded by the TLS client's socket
     *         timeout, which WiFiClientSecure takes from TCP_CONNECT_TIMEOUT_S.
     */
    inline uint32_t worstCaseS(Stage stage)
    {
        switch (stage)
        {
        case Stage::ResolveHost:
            return MqttTimeouts::DNS_RESOLVE_MAX_S;
        case Stage::OpenTls:
            return MqttTimeouts::TCP_CONNECT_TIMEOUT_S + MqttTimeouts::TLS_HANDSHAKE_TIMEOUT_S;
        case Stage::MqttHandshake:
            return MqttTimeouts::TCP_CONNECT_TIMEOUT_S + MqttTimeouts::SOCKET_TIMEOUT_S;
        case Stage::Announce:
            return 2 * MqttTimeouts::TCP_CONNECT_TIMEOUT_S;
        }
        return 0;
    }

    /**
     * @brief Runs every stage in order, feeding the watchdog right before each
     *        one, and stops at the first stage that fails.
     * @return true only if every stage succeeded.
     */
    template <typename RunStage, typename FeedWatchdog>
    bool run(RunStage runStage, FeedWatchdog feedWatchdog)
    {
        // std::all_of stops at the first stage that returns false.
        return std::all_of(std::begin(STAGES), std::end(STAGES),
                           [&runStage, &feedWatchdog](Stage stage) {
                               feedWatchdog();
                               return runStage(stage);
                           });
    }
}
