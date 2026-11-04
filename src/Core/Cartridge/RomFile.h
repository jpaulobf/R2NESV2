#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace R2NES::Core
{
    // Limites de tamanho para arquivos de ROM e arquivos ZIP, em bytes.
    constexpr size_t MaxRomFileSize = 64u * 1024 * 1024;
    constexpr size_t MaxZipFileSize = 128u * 1024 * 1024;

    // Lê o arquivo binário em path, respeitando o limite de bytes informado.
    // Em caso de sucesso, substitui data pelo conteúdo e retorna true;
    // em caso de erro ou tamanho excedido, retorna false e preserva data.
    bool readRomFile(const std::string &path, size_t limit, std::vector<uint8_t> &data);

    // Extrai a primeira entrada com extensão .nes, .unf ou .unif de zip.
    // Aceita entradas armazenadas ou comprimidas com DEFLATE e verifica o CRC.
    // Retorna true e substitui rom em caso de sucesso; caso contrário, retorna
    // false e preserva rom. Arquivos ZIP64 e divididos em discos não são aceitos.
    bool extractRomFromZip(const std::vector<uint8_t> &zip, std::vector<uint8_t> &rom);
}