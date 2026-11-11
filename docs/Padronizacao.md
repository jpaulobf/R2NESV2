# Planos de padronização dos emuladores

Os projetos R2SEGA8, R2BOY e R2NES continuam independentes. O objetivo é aproximar responsabilidades e comportamentos, respeitando as particularidades de cada console e preservando formatos de arquivos e funcionalidades.

## Plano 1 — Organização interna

1. **Concluído:** causas de pausa independentes e testáveis nos três projetos: pausa manual, menus, minimização e depuração quando aplicável. Há testes de causas simultâneas e mudanças em ordens diferentes.
2. **Concluído:** extrair o controle SDL de fullscreen e resolução nos três projetos. A janela restaura sua geometria e a engine mantém as decisões de áudio e tempo.
3. Separar gradualmente leitura e gravação de ROM, bateria, cheats e estados do núcleo, começando pelo R2NES e preservando compatibilidade dos arquivos.
4. Dividir responsabilidades de janela, menus, apresentação de vídeo e ferramentas de depuração onde houver fronteiras claras.
5. Definir e testar o comportamento de temporização e VSync em diferentes frequências, regiões e modos de execução. Esta etapa altera comportamento e exige medições.
6. Revisar contratos e duplicações após estabilizar as etapas anteriores. Os projetos permanecem independentes.

## Plano 2 — Preparação para um frontend futuro

1. Definir em cada projeto a camada responsável pela sessão: carregar ROM, reiniciar, pausar, salvar e encerrar.
2. Padronizar conceitos de estado, resultados e capacidades, sem pressupor que todos os consoles ofereçam os mesmos recursos.
3. Fazer menus solicitarem operações à camada de sessão, preservando a interface atual.
4. Separar progressivamente decisões de janela e persistência das operações de sessão.
5. Quando houver requisitos concretos para o frontend, definir a comunicação externa, incluindo inicialização, configuração, erros e encerramento.

Cada etapa deve preservar o comportamento observado, manter os projetos compiláveis e acrescentar testes para as situações de risco. Não se pressupõe biblioteca comum nem protocolo de comunicação nesta fase.
