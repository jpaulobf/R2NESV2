#pragma once

#include <streambuf>
#include <string>

namespace R2NES::System
{
    // Escrita sequencial diretamente na string do snapshot, sem cópia via str().
    class SnapshotBuffer : public std::streambuf
    {
    public:
        explicit SnapshotBuffer(std::string &destination) : destination(destination)
        {
            destination.clear();
        }

    protected:
        std::streamsize xsputn(const char *data, std::streamsize count) override
        {
            if (count > 0)
                destination.append(data, static_cast<std::size_t>(count));
            return count;
        }

        int_type overflow(int_type value) override
        {
            if (!traits_type::eq_int_type(value, traits_type::eof()))
                destination.push_back(traits_type::to_char_type(value));
            return traits_type::not_eof(value);
        }

    private:
        std::string &destination;
    };
}
