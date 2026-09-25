#pragma once

/**
 * @brief Firmware version, reported in every message and stored against the
 *        device row.
 *
 * It is not decoration: once nodes are deployed in the field and the payload
 * contract evolves, this is the only way to know which node speaks which
 * version — and therefore whether a parsing failure upstream is a bug or an
 * outdated node that needs reflashing.
 *
 * Bump the minor version on any payload contract change.
 */
#define FIRMWARE_VERSION "1.1.0"
