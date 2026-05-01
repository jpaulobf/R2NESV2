# Changelog

Todas as mudanças notáveis neste projeto serão documentadas neste arquivo.

## [0.1.0] - 2024-05-24

### Adicionado
- Estrutura inicial do projeto (Core Architecture).
- Sistema de build com CMake.
- Implementação do Barramento (Bus) com suporte a Mirroring.
- Esqueleto da CPU (6502) e abstração da RAM.

### Adicionado (Sessão Atual)
- Abstração completa de Cartucho separando `PRG-ROM`, `CHR-ROM` e `Mappers`.
- Implementação do suporte inicial a Mappers com o `Mapper 000` (NROM).
- Sistema de carregamento de arquivos `.nes` (iNES Format) com validação de cabeçalho.
- Evolução da CPU: Implementação real do ciclo de `Reset` buscando o vetor em `0xFFFC`.
- Primeira leitura bem-sucedida de dados de uma ROM real (SMB).