#include <iostream>
#include "Core/NESBoard.h" // Inclui o cabeçalho da NesBoard

int main()
{
    std::cout << "Starting R2NES v2 Emulator..." << std::endl;

    R2NES::Core::NesBoard nes; // Instancia a NesBoard

    // Caminho para a ROM que você deseja testar
    const std::string romPath = "..\\roms\\smb.nes";
    nes.insertCartridge(romPath); // Insere o cartucho

    // O reset() já é chamado no construtor da NesBoard, mas chamá-lo novamente aqui
    // garante que os testes de leitura da ROM no reset sejam executados APÓS o cartucho ser inserido.
    nes.reset();

    std::cout << "Emulator initialized. Press Enter to exit..." << std::endl;
    std::cin.get(); // Mantém a janela aberta para ver a saída
    return 0;
}