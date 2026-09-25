#pragma once

// In-memory host stand-in for the slice of the ESP32 core's fs::FS / fs::File
// API that LittleFsBuffer uses. Files live in a std::map for the lifetime of
// the test process; nothing touches the host disk.

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#define FILE_READ "r"
#define FILE_WRITE "w"
#define FILE_APPEND "a"

namespace fs
{
    class File
    {
    public:
        File() = default;
        File(std::shared_ptr<std::string> content, size_t position, bool isWritable);

        /** @return true if the open succeeded, mirroring the real handle. */
        operator bool() const;

        size_t size() const;
        bool seek(uint32_t position);
        int available();
        int read();
        size_t read(uint8_t *buffer, size_t size);
        size_t write(uint8_t value);
        size_t write(const uint8_t *buffer, size_t size);
        size_t print(size_t value);

        /** @brief Stream::parseInt() semantics: skips non-digits, 0 if none. */
        long parseInt();

        void close();

    private:
        // Shared so a handle stays valid even if its path is removed, as a
        // real open descriptor would.
        std::shared_ptr<std::string> _content;
        size_t _position = 0;
        bool _isWritable = false;
    };

    class FS
    {
    public:
        /** @brief "r" fails on a missing path, "w" truncates, "a" appends. */
        File open(const char *path, const char *mode = FILE_READ, bool create = false);
        bool exists(const char *path) const;
        bool remove(const char *path);
        bool rename(const char *pathFrom, const char *pathTo);

    protected:
        std::map<std::string, std::shared_ptr<std::string>> _files;
    };
}

using fs::File;
using fs::FS;
