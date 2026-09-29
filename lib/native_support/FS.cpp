#include "FS.h"

#include <algorithm>
#include <cctype>
#include <cstring>

#include "LittleFS.h"

fs::LittleFSFS LittleFS;

namespace fs
{
    File::File(std::shared_ptr<std::string> content, size_t position, bool isWritable)
        : _content(std::move(content)), _position(position), _isWritable(isWritable)
    {
    }

    File::operator bool() const
    {
        return _content != nullptr;
    }

    size_t File::size() const
    {
        return _content ? _content->size() : 0;
    }

    bool File::seek(uint32_t position)
    {
        if (!_content || position > _content->size())
        {
            return false;
        }
        _position = position;
        return true;
    }

    int File::available()
    {
        return _content ? (int)(_content->size() - _position) : 0;
    }

    int File::read()
    {
        if (available() <= 0)
        {
            return -1;
        }
        return (uint8_t)(*_content)[_position++];
    }

    size_t File::read(uint8_t *buffer, size_t size)
    {
        const size_t count = std::min(size, (size_t)std::max(available(), 0));
        if (count > 0)
        {
            std::memcpy(buffer, _content->data() + _position, count);
            _position += count;
        }
        return count;
    }

    size_t File::write(uint8_t value)
    {
        return write(&value, 1);
    }

    size_t File::write(const uint8_t *buffer, size_t size)
    {
        if (!_content || !_isWritable)
        {
            return 0;
        }
        _content->replace(_position, std::min(size, _content->size() - _position),
                          (const char *)buffer, size);
        _position += size;
        return size;
    }

    size_t File::print(size_t value)
    {
        const std::string text = std::to_string(value);
        return write((const uint8_t *)text.data(), text.size());
    }

    long File::parseInt()
    {
        int next = read();
        while (next != -1 && next != '-' && !std::isdigit(next))
        {
            next = read();
        }

        const bool isNegative = next == '-';
        if (isNegative)
        {
            next = read();
        }

        long value = 0;
        while (next != -1 && std::isdigit(next))
        {
            value = value * 10 + (next - '0');
            next = read();
        }
        return isNegative ? -value : value;
    }

    void File::close()
    {
        _content.reset();
        _position = 0;
    }

    File FS::open(const char *path, const char *mode, bool)
    {
        auto entry = _files.find(path);
        if (std::strcmp(mode, FILE_READ) == 0)
        {
            return entry == _files.end() ? File() : File(entry->second, 0, false);
        }

        if (entry == _files.end() || std::strcmp(mode, FILE_WRITE) == 0)
        {
            entry = _files.insert_or_assign(path, std::make_shared<std::string>()).first;
        }
        const size_t position = std::strcmp(mode, FILE_APPEND) == 0 ? entry->second->size() : 0;
        return File(entry->second, position, true);
    }

    bool FS::exists(const char *path) const
    {
        return _files.count(path) > 0;
    }

    bool FS::remove(const char *path)
    {
        return _files.erase(path) > 0;
    }

    bool FS::rename(const char *pathFrom, const char *pathTo)
    {
        auto entry = _files.find(pathFrom);
        if (entry == _files.end())
        {
            return false;
        }
        if (std::strcmp(pathFrom, pathTo) == 0)
        {
            return true;
        }
        _files[pathTo] = entry->second;
        _files.erase(pathFrom);
        return true;
    }

    bool LittleFSFS::begin(bool, const char *, uint8_t, const char *)
    {
        return true;
    }

    bool LittleFSFS::format()
    {
        _files.clear();
        return true;
    }

    void LittleFSFS::end()
    {
        // No-op: files persist in memory across remounts, like flash does.
    }
}
