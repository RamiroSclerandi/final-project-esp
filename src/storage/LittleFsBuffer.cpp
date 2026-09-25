#include "storage/LittleFsBuffer.h"

#include <Arduino.h>
#include <LittleFS.h>

namespace
{
    constexpr char RECORDS_PATH[] = "/buffer.jsonl";
    constexpr char OFFSET_PATH[] = "/buffer.off";
    constexpr char TEMP_PATH[] = "/buffer.tmp";
}

bool LittleFsBuffer::begin()
{
    // true = format if the partition is not mountable. A corrupted buffer is
    // worth losing; an unusable filesystem would disable persistence entirely.
    if (!LittleFS.begin(true))
    {
        Serial.println("[Buffer] LittleFS no monta. Sin respaldo local.");
        _mounted = false;
        return false;
    }

    _mounted = true;
    _readOffset = 0;

    File offsetFile = LittleFS.open(OFFSET_PATH, FILE_READ);
    if (offsetFile)
    {
        _readOffset = (size_t)offsetFile.parseInt();
        offsetFile.close();
    }

    recountPending();

    Serial.printf("[Buffer] LittleFS montado. %lu registros pendientes, %u%% usado.\n",
                  (unsigned long)_pending, usedPercent());
    return true;
}

void LittleFsBuffer::recountPending()
{
    _pending = 0;

    File file = LittleFS.open(RECORDS_PATH, FILE_READ);
    if (!file)
    {
        _readOffset = 0;
        return;
    }

    // A stale offset past the end means the file was replaced or truncated.
    if (_readOffset > file.size())
    {
        _readOffset = 0;
    }

    file.seek(_readOffset);
    while (file.available())
    {
        if (file.read() == '\n')
        {
            _pending++;
        }
    }

    file.close();
}

void LittleFsBuffer::persistOffset()
{
    File file = LittleFS.open(OFFSET_PATH, FILE_WRITE);
    if (file)
    {
        file.print(_readOffset);
        file.close();
    }
}

void LittleFsBuffer::makeRoom(size_t incomingLength)
{
    File file = LittleFS.open(RECORDS_PATH, FILE_READ);
    if (!file)
    {
        return;
    }

    const size_t size = file.size();
    file.close();

    if (size + incomingLength <= MAX_BYTES)
    {
        return;
    }

    // Full: discard the oldest records. Recent data is worth more than old data
    // in a datalogger, but the loss is counted so it never goes unnoticed.
    while (hasPending())
    {
        const size_t before = _readOffset;
        if (!dropOldest())
        {
            break;
        }
        _dropped++;

        if (_readOffset == before)
        {
            break; // No progress; avoid spinning on a malformed file.
        }

        File check = LittleFS.open(RECORDS_PATH, FILE_READ);
        if (!check)
        {
            break;
        }
        const size_t remaining = check.size() - _readOffset;
        check.close();

        if (remaining + incomingLength <= MAX_BYTES)
        {
            break;
        }
    }

    compact();
    Serial.printf("[Buffer] Almacenamiento lleno. Total descartados: %lu\n",
                  (unsigned long)_dropped);
}

bool LittleFsBuffer::append(const uint8_t *payload, size_t length)
{
    if (!_mounted || length == 0)
    {
        return false;
    }

    makeRoom(length + 1);

    File file = LittleFS.open(RECORDS_PATH, FILE_APPEND);
    if (!file)
    {
        return false;
    }

    const size_t written = file.write(payload, length);
    file.write('\n');
    file.close();

    if (written != length)
    {
        return false;
    }

    _pending++;
    return true;
}

bool LittleFsBuffer::hasPending() const
{
    return _mounted && _pending > 0;
}

size_t LittleFsBuffer::peekOldest(uint8_t *out, size_t outSize)
{
    if (!hasPending())
    {
        return 0;
    }

    File file = LittleFS.open(RECORDS_PATH, FILE_READ);
    if (!file)
    {
        return 0;
    }

    file.seek(_readOffset);

    size_t length = 0;
    while (file.available() && length < outSize - 1)
    {
        const int c = file.read();
        if (c == '\n')
        {
            break;
        }
        out[length++] = (uint8_t)c;
    }

    file.close();
    out[length] = '\0';
    return length;
}

bool LittleFsBuffer::dropOldest()
{
    if (!hasPending())
    {
        return false;
    }

    File file = LittleFS.open(RECORDS_PATH, FILE_READ);
    if (!file)
    {
        return false;
    }

    file.seek(_readOffset);

    size_t consumed = 0;
    bool sawNewline = false;
    while (file.available())
    {
        consumed++;
        if (file.read() == '\n')
        {
            sawNewline = true;
            break;
        }
    }
    file.close();

    // A record without a terminating newline is a partial write from a power
    // cut. Consuming it discards exactly that damaged tail.
    if (!sawNewline && consumed == 0)
    {
        return false;
    }

    _readOffset += consumed;
    _pending--;
    persistOffset();

    if (_readOffset >= COMPACT_THRESHOLD)
    {
        compact();
    }

    return true;
}

bool LittleFsBuffer::compact()
{
    if (_readOffset == 0)
    {
        return true;
    }

    File source = LittleFS.open(RECORDS_PATH, FILE_READ);
    if (!source)
    {
        return false;
    }

    File temp = LittleFS.open(TEMP_PATH, FILE_WRITE);
    if (!temp)
    {
        source.close();
        return false;
    }

    source.seek(_readOffset);

    uint8_t chunk[256];
    while (source.available())
    {
        const size_t read = source.read(chunk, sizeof(chunk));
        temp.write(chunk, read);
    }

    source.close();
    temp.close();

    LittleFS.remove(RECORDS_PATH);
    LittleFS.rename(TEMP_PATH, RECORDS_PATH);

    _readOffset = 0;
    persistOffset();
    return true;
}

uint8_t LittleFsBuffer::usedPercent() const
{
    if (!_mounted)
    {
        return 0;
    }

    File file = LittleFS.open(RECORDS_PATH, FILE_READ);
    if (!file)
    {
        return 0;
    }

    const size_t size = file.size();
    file.close();

    const size_t live = size > _readOffset ? size - _readOffset : 0;
    const uint32_t percent = (uint32_t)((live * 100) / MAX_BYTES);
    return percent > 100 ? 100 : (uint8_t)percent;
}

uint32_t LittleFsBuffer::pendingCount() const
{
    return _pending;
}

uint32_t LittleFsBuffer::droppedCount() const
{
    return _dropped;
}

const char *LittleFsBuffer::kind() const
{
    return _mounted ? "littlefs" : "none";
}
