#pragma once
#include <istream>

namespace R2NES::Core
{
    // R2NK uses native one-byte bools. Never copy untrusted bytes into a bool:
    // other representations are undefined behavior when subsequently read.
    inline void readStateBool(std::istream &input, bool &value)
    {
        static_assert(sizeof(bool) == 1, "R2NK requires one-byte bools");
        const int byte = input.get();
        if (byte != 0 && byte != 1)
            input.setstate(std::ios::failbit);
        else
            value = byte != 0;
    }
}