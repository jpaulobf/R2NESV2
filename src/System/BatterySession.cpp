#include "BatterySession.h"
#include "Core/Cartridge/Cartridge.h"
#include "System/BatterySave.h"
#include <iostream>
#include <utility>

namespace R2NES::System
{
    BatterySession::BatterySession(std::shared_ptr<Core::Cartridge> cart, const std::filesystem::path &directory)
        : cartridge_(std::move(cart))
    {
        if (!cartridge_ || !cartridge_->isValid() || !cartridge_->getMapper() || directory.empty())
            return;
        auto mapper = cartridge_->getMapper();
        auto data = mapper->getBatteryData();
        if (data.empty())
            return;
        path_ = directory / (cartridge_->getRomHash() + ".sav");
        std::string error;
        const auto result = BatterySave::load(path_, data, error);
        if (result == BatterySave::LoadResult::Error || !mapper->setBatteryData(data))
        {
            loadFailed_ = true;
            std::vector<uint8_t> source, converted;
            std::string description, readError;
            if (BatterySave::readFile(path_, 132096, source, readError))
                importAvailable_ = mapper->convertBatteryData(source, converted, description);
            message_ = "Nao foi possivel carregar a bateria: " + path_.string() + "\n" + error +
                       (description.empty() ? "" : "\n" + description) +
                       "\nA gravacao automatica desta bateria esta desativada ate reabrir ou importar o arquivo.";
            std::cerr << "Battery load failed ('" << path_.string() << "'): " << error << '\n';
            return;
        }
        lastData_ = std::move(data);
    }
    BatterySession::~BatterySession()
    {
        try
        {
            flush();
        }
        catch (const std::exception &error)
        {
            std::cerr << "Battery save failed: " << error.what() << '\n';
        }
    }
    bool BatterySession::flush()
    {
        if (loadFailed_)
            return false;
        if (path_.empty() || !cartridge_ || !cartridge_->getMapper())
            return true;
        auto data = cartridge_->getMapper()->getBatteryData();
        if (data == lastData_)
            return true;
        std::string error;
        if (!BatterySave::save(path_, data, error))
        {
            message_ = "Nao foi possivel gravar a bateria: " + path_.string() + "\n" + error;
            std::cerr << "Battery save failed ('" << path_.string() << "'): " << error << '\n';
            return false;
        }
        lastData_ = std::move(data);
        return true;
    }
    bool BatterySession::importBattery()
    {
        if (!importAvailable_ || !cartridge_ || !cartridge_->getMapper())
            return false;
        auto mapper = cartridge_->getMapper();
        std::vector<uint8_t> source, converted;
        std::string error;
        if (!BatterySave::readFile(path_, 132096, source, error) ||
            !mapper->convertBatteryData(source, converted, error) ||
            converted.size() != mapper->getBatteryData().size())
        {
            importAvailable_ = false;
            message_ = "Importacao de bateria recusada. O arquivo foi preservado.\n" + error;
            return false;
        }
        std::filesystem::path backup;
        if (!BatterySave::backup(path_, backup, error) || !BatterySave::save(path_, converted, error))
        {
            message_ = "Nao foi possivel importar a bateria.\n" + error;
            return false;
        }
        if (!mapper->setBatteryData(converted))
        {
            message_ = "O mapper recusou os dados importados. Backup: " + backup.string();
            return false;
        }
        loadFailed_ = importAvailable_ = false;
        lastData_ = std::move(converted);
        message_ = "Bateria importada. Original preservado em: " + backup.string();
        return true;
    }
}