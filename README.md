# Descrição

O presente código detalha a implementação de uma solução em linguagem C para simular o fluxo de veículos em um cruzamento de quatro vias (Norte, Sul, Leste e Oeste) usando a biblioteca `POSIX Pthreads` para gerenciar a execução concorrente e a sincronização. O objetivo principal é gerenciar o tráfego de carros e ambulâncias (com prioridade) para evitar colisões e _starvation_ (inanição).

# Modelagem do Problema e Sincronização

- O problema foi modelado utilizando programação concorrente, implementada com _threads_.
- O gerenciamento do cruzamento, um recurso compartilhado, foi feito com mecanismos de sincronização essenciais.
    - _Locks_ (_mutexes_) garantem a exclusão mútua em seções críticas.
    - Variáveis de condição (como `cruzamento.pode_cruzar`) permitem que _threads_ aguardem de forma eficiente até que uma condição específica seja atendida.
- A variável global `cruzamento` (do tipo `struct Cruzamento`) gerencia o fluxo, contendo a quantidade de carros e ambulâncias esperando/no cruzamento, a _flag_ `modo_emergencia`, o estado atual do fluxo (`estado_atual`), e os _mutexes_ e variáveis de condição.

# Threads Implementadas

1. **Thread Controladora (`fluxo_trafego`):** Responsável por gerenciar o estado do cruzamento, aguardando que o cruzamento esteja livre antes de mudar o fluxo.
    - **Modo Emergência:** Se `modo_emergencia` for verdadeiro, calcula a demanda de ambulâncias, altera o estado, sinaliza a passagem das ambulâncias e aguarda a desativação da _flag_.
    - **Modo Normal (Carros):** Se não for emergência, calcula a demanda de carros, alterna o estado para o fluxo com maior demanda (ou alterna em caso de empate) e calcula um tempo limite de passagem dinâmico.
2. **Thread Carros (`carros`):** Adquire o _lock_, incrementa a fila de espera e entra em um laço de espera condicional até que a passagem seja compatível (`pode_passar`). Ao sair do cruzamento, notifica o controlador com `pthread_cond_broadcast`.
3. **Thread Ambulância (`ambulancia`):** Adquire o _lock_ para definir `modo_emergencia` como verdadeiro e emite um _broadcast_ para acordar o controlador. Aguarda a passagem e, ao sair, define `modo_emergencia` como falso e notifica o controlador para o retorno ao fluxo normal.

# Conclusão

O simulador validou o sucesso do algoritmo, garantindo a segurança (ausência de colisões) e a justiça (ausência de _starvation_). O mecanismo de prioridade para ambulâncias funcionou conforme especificado, interrompendo o fluxo normal e garantindo sua passagem.
