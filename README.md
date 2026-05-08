# R2NES (v.2) 🕹️

**R2NES (v.2)** é um emulador de Nintendo Entertainment System (NES) de segunda geração, desenvolvido com foco em performance bruta, precisão de ciclo e fidelidade ao hardware original.
Após uma primeira implementação bem-sucedida em Java, a versão 2 nasce do desejo de explorar o desenvolvimento de baixo nível e o controle total sobre o hardware emulado, migrando a arquitetura para **[C/C++]**.

## 🎯 Objetivos do Projeto
 * **Fidelidade ao Hardware:** Implementação rigorosa do processador Ricoh 2A03 e da PPU 2C02.
 * **Performance:** Minimizar a latência de áudio e vídeo através de gerenciamento de memória eficiente.
 * **Portabilidade:** Estrutura de código modular para facilitar o porte entre diferentes sistemas e bibliotecas de interface (como SDL2 ou SFML).
 * **Aprendizado:** Documentar o processo de transição de uma linguagem de alto nível (Java) para o "bare metal" da emulação.

## 🏗️ Arquitetura Proposta
O projeto segue a anatomia clássica do "Famicom", organizada em torno de uma **NesBoard** que gerencia a sincronização dos componentes:
 * **CPU:** Emulação completa do conjunto de instruções 6502 (sem o modo decimal).
 * **PPU:** Processamento de imagem baseado em scanlines e ciclos.
 * **BUS:** Barramento unificado para mapeamento de memória e espelhamento (mirroring).
 * **Mappers:** Suporte extensível para diferentes cartuchos através de uma interface modular.

## 🛠️ Tecnologias
 * **Linguagem:** C++17
 * **Build System:** CMake
 * **Gráficos/Input:** SDL2
 * **Interface de Debug:** Dear ImGui

## 🚀 Status do Desenvolvimento
- [x] Barramento Principal (Bus) e Endereçamento de RAM.
- [x] Leitura de arquivos `.nes` (iNES).
- [x] Infraestrutura de Mappers (NROM funcional).
- [x] Ciclo de Reset da CPU.
- [x] Conjunto de Instruções da CPU (6502 Completo + Instruções Ilegais).
- [x] PPU (Renderização de Background, Sprites, Sprite 0 Hit e Overflow).
- [x] Otimização de Performance: Sprite Evaluation e Cache por scanline.
- [x] Sistema de Scrolling (Horizontal e Vertical).
- [x] Suporte a Controles (Xbox, 8BitDo, etc.) via SDL_GameController.
- [x] Opções de Vídeo: VSync dinâmico e modo "Unlimited Sprites".
- [ ] Suporte a Áudio (APU).