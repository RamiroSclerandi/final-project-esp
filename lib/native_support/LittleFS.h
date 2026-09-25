#pragma once

// Host stand-in for the ESP32 core's LittleFS global, backed by the in-memory
// fs::FS in FS.h. Mounting always succeeds; tests call format() in setUp()
// to start every case from an empty filesystem.

#include "FS.h"

namespace fs
{
    class LittleFSFS : public FS
    {
    public:
        bool begin(bool formatOnFail = false, const char *basePath = "/littlefs",
                   uint8_t maxOpenFiles = 10, const char *partitionLabel = "spiffs");

        /** @brief Erases every file, like formatting the flash partition. */
        bool format();

        void end();
    };
}

extern fs::LittleFSFS LittleFS;

using fs::LittleFSFS;
