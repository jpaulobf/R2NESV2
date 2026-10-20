#include "System/SnapshotBuffer.h"
#include <iostream>
#include <sstream>
#include <stdexcept>

static void check(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

static void writeSnapshot(std::ostream &output, const std::string &payload)
{
    // Exercita put() e write(), incluindo todos os valores possíveis de um byte.
    for (int value = 0; value < 256; ++value)
        output.put(static_cast<char>(value));
    output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    output.put('\0');
    output.flush();
}

int main()
{
    try
    {
        std::string storage;
        // Estados grandes, menores e maiores no mesmo buffer não podem reter sufixos.
        for (const auto size : {300000, 0, 4096, 500000, 3})
        {
            std::string payload(size, '\0');
            for (std::size_t i = 0; i < payload.size(); ++i)
                payload[i] = static_cast<char>(i % 256);

            std::ostringstream reference(std::ios::binary | std::ios::out);
            writeSnapshot(reference, payload);
            const auto previousCapacity = storage.capacity();
            R2NES::System::SnapshotBuffer buffer(storage);
            std::ostream output(&buffer);
            writeSnapshot(output, payload);

            check(output.good(), "Snapshot write failed");
            check(storage == reference.str(), "Snapshot bytes differ from ostringstream");
            check(storage.capacity() >= previousCapacity, "Snapshot discarded reusable storage");
        }
        std::cout << "Snapshot buffer tests passed (binary identity, resize, reuse)\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
