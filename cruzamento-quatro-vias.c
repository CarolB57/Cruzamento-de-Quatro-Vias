/**
 * 
 * Descrição do problema resolvido pelo presente código:
 * 
 *      Em um determinado cruzamento de quatro vias, carros e ambulâncias chegam
 * de diferentes direções (Norte, Sul, Leste e Oeste) e precisam cruzar com segurança.
 * O objetivo é gerenciar o fluxo de tráfego para evitar colisões (condição de corrida)
 * e também evitar starvation (carros de uma via nunca conseguem passar). Para a
 * definição de passagem, apenas carros de direções compatíveis (como Norte e Sul em
 * linha reta) podem cruzar simultaneamente, e carros de direções conflitantes (como
 * Norte e Leste) não podem estar no cruzamento ao mesmo tempo. Além disso, as
 * ambulâncias possuem prioridade máxima, ou seja, quando uma ambulância chega,
 * o sistema precisa:
 *      (a) fechar todas as outras vias de forma segura;
 *      (b) garantir que o cruzamento esteja vazio;
 *      (c) permitir a passagem da(s) ambulância(s);
 *      (d) retornar ao funcionamento normal;
 * 
 * @version 0.1
 * @date 2025-10-17
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>


#define T_MINIMO 5                      // Tempo mínimo que um fluxo fica aberto
#define T_MAXIMO 20                     // Tempo máximo que um fluxo fica aberto

// Parâmetros para a fórmula do cálculo de tempo que cada fluxo fica aberto
#define T_BASE 1.8
#define FATOR_CARRO 2.2

// Número de Carros em cada direção
#define CARROS_NORTE 15
#define CARROS_SUL 3
#define CARROS_LESTE 8
#define CARROS_OESTE 8
#define TOTAL_CARROS (CARROS_NORTE + CARROS_SUL + CARROS_LESTE + CARROS_OESTE)

// Número de Ambulâncias em cada direção
#define AMBULANCIA_NORTE 2
#define AMBULANCIA_SUL 1
#define AMBULANCIA_LESTE 3
#define AMBULANCIA_OESTE 1
#define TOTAL_AMBULANCIAS (AMBULANCIA_NORTE + AMBULANCIA_SUL + AMBULANCIA_LESTE + AMBULANCIA_OESTE)

#define TOTAL_VEICULOS (TOTAL_CARROS + TOTAL_AMBULANCIAS)

/**
 * @brief Direções dos veículos
 * 
 */
typedef enum Direcao { NORTE, SUL, LESTE, OESTE, NUM_DIRECOES } Direcao;

const char* nome_direcao[] = {"Norte", "Sul", "Leste", "Oeste"};

/**
 * @brief Struct para definir qual fluxo de carros/ambulâncias (Norte-Sul ou Leste-Oeste) está passando no cruzamento no momento
 * 
 */
typedef enum{
    FLUXO_NS,                           // Carros nas direções Norte e Sul
    FLUXO_LO,                           // Carros nas direções Leste e Oeste
    AMBULANCIA_NS,                      // Ambulancias nas direções Norte e Sul
    AMBULANCIA_LO                       // Ambulancias nas direções Leste e Oeste 
} EstadoFluxo;

/**
 * @brief Tipos de Veículos presentes no cruzamento
 * 
 */
typedef enum{
    TIPO_CARRO,
    TIPO_AMBULANCIA
} TipoVeiculo;

/**
 * @brief Direção de cada veículo
 * 
 */
typedef struct {
    Direcao direcao;
} VeiculoArgs;

/**
 * @brief Struct que define parâmetros importantes para o controle do fluxo de veículos no cruzamento
 * 
 */
typedef struct{
    int carros_esperando[NUM_DIRECOES], ambulancias_esperando[NUM_DIRECOES];            // Quantidade de carros e ambulâncias esperando em uma dada direção respectivamente
    int carros_no_cruzamento, ambulancias_no_cruzamento;                                // Quantidade de carros e ambulâncias que estão no cruzamento respectivamente
    bool modo_emergencia;                                                               // Flag para sinalizar que há ambulâncias querendo entrar no cruzamento (Modo Emergência)
    EstadoFluxo estado_atual;                                                           // Estado atual do fluxo de veículos no cruzamento
    pthread_mutex_t lock;                                                               // Mutex para garantir exclusão mútua entre threads em seções críticas do código
    pthread_mutex_t lock_rand;                                                          // Mutex para proteger as chamadas da função rand()
    pthread_cond_t pode_cruzar;                                                         // Variável condicional para permitir que as threads aguardem de forma eficiente até que uma condição específica seja atendida
    int contadores_id_carros[NUM_DIRECOES], contadores_id_ambulancias[NUM_DIRECOES];    // Arrays para guardar o próximo id de cada direção de carros e ambulâncias respectivamente
    pthread_mutex_t lock_contadores_id;                                                 // Mutex para proteger os arrays acima
} Cruzamento;

Cruzamento cruzamento;                                                                  // Variável global para gerir todo o fluxo do cruzamento


int pode_passar(Direcao dir, EstadoFluxo estado, TipoVeiculo tipo){
    // Se está liberado para ambulâncias
    if(estado == AMBULANCIA_NS || estado == AMBULANCIA_LO){
        // apenas ambulâncias podem sequer considerar passar.
        if(tipo != TIPO_AMBULANCIA) return 0;
    }

    // Se uma emergência geral foi declarada, barra os carros.
    if(cruzamento.modo_emergencia && tipo == TIPO_CARRO) return 0;

    // Verificação de fluxo e direção para quem sobrou
    if((dir == NORTE || dir == SUL) && (estado == FLUXO_NS || estado == AMBULANCIA_NS)) return 1;
    if((dir == LESTE || dir == OESTE) && (estado == FLUXO_LO || estado == AMBULANCIA_LO)) return 1;
    return 0;
}

/**
 * @brief Função da Thread de Carro. Opera em um loop infinito, simulando o comportamento contínuo de um veículo no sistema:
 * se aproximar do cruzamento, esperar pela sua vez, atravessar, e então reiniciar o ciclo. A função gerencia toda a sincronização 
 * necessária para interagir de forma segura com o estado compartilhado do cruzamento.
 *
 * @param arg Um ponteiro genérico (void*) para uma estrutura VeiculoArgs alocada dinamicamente. A estrutura deve conter a direção 
 * de origem do carro.
 *
 * @return void* Sempre retorna NULL
 */
void * carros(void *arg){
    int id;                                 // Declaração da variável de id local para a thread
    int tempo;                              // Variável para calcular o tempo que será usado no sleep com rand
    VeiculoArgs *args = (VeiculoArgs*) arg; // Converter o argumento genérico para o tipo esperado (VeiculoArgs)
    Direcao direcao_carro = args->direcao;  // Extrair a direção, definida pela thread main
    free(arg);                              // Liberar a memória alocada na main para os argumentos, uma vez que os dados já foram copiados

    // Adquire o lock específico dos contadores para garantir que a leitura e o incremento
    // do id sejam uma operação que evite com que dois carros da mesma direção peguem o mesmo id
    pthread_mutex_lock(&cruzamento.lock_contadores_id);
    // Pega o próximo id disponível para esta direção
    id = cruzamento.contadores_id_carros[direcao_carro];
    // Incrementa o contador para a próxima thread da mesma direção
    cruzamento.contadores_id_carros[direcao_carro]++;
    pthread_mutex_unlock(&cruzamento.lock_contadores_id);

	while(1){
        // Simula o tempo que o carro leva para percorrer o trajeto até chegar ao cruzamento
        printf("Carro %d da direcao %s esta se aproximando do cruzamento.\n", id, nome_direcao[direcao_carro]);
        fflush(stdout); // Força a escrita imediata no terminal para depuração concorrente.

        pthread_mutex_lock(&cruzamento.lock_rand);
        tempo = 2 + (rand() % 8);
        pthread_mutex_unlock(&cruzamento.lock_rand);
        sleep(tempo);

        // Adquire o lock principal para interagir com o estado do cruzamento
		pthread_mutex_lock(&cruzamento.lock);
        // Incrementa o contador da fila de espera para sua direção.
		cruzamento.carros_esperando[direcao_carro]++;

        // Loop de espera condicional em que a thread só prossegue se 'pode_passar' retornar true. Essencial para se proteger contra despertares inadequados
		while(!pode_passar(direcao_carro, cruzamento.estado_atual, TIPO_CARRO)){
            printf("Carro %d da direcao %s esta esperando para passar.\n", id, nome_direcao[direcao_carro]);
            fflush(stdout);
            // libera o 'lock' e põe a thread para dormir. Ao acordar, ela readquire o 'lock' antes de reavaliar a condição
            pthread_cond_wait(&cruzamento.pode_cruzar, &cruzamento.lock);
        }

        // Se saiu do loop, a passagem foi liberada. Atualiza o estado:
        cruzamento.carros_esperando[direcao_carro]--;   // Deixa de estar "esperando"
        cruzamento.carros_no_cruzamento++;              // Agora está "no cruzamento"
        printf("Carro %d da direcao %s entrou no cruzamento.\n", id, nome_direcao[direcao_carro]);
        fflush(stdout);

        // Libera o lock antes de simular o tempo de travessia. Isso é feito para permitir que outros carros do mesmo fluxo entrem no cruzamento concorrentemente
	    pthread_mutex_unlock(&cruzamento.lock);

        // Simula o tempo que o carro leva para atravessar fisicamente o cruzamento
        sleep(3);

        // Readquire o lock para atualizar o estado de saída de forma segura
        pthread_mutex_lock(&cruzamento.lock);
        cruzamento.carros_no_cruzamento--;
        printf("Carro %d da direcao %s saiu do cruzamento.\n", id, nome_direcao[direcao_carro]);
        fflush(stdout);

        // Notifica todas as outras threads (especialmente a controladora) que o estado mudou.Essencial para que athread 'fluxo_trafego' possa verificar se o cruzamento esvaziou
        pthread_cond_broadcast(&cruzamento.pode_cruzar);
        pthread_mutex_unlock(&cruzamento.lock);
    }
    return NULL;
}

/**
 * @brief Função da Thread de Ambulância. Implementa um comportamento de alta prioridade que interrompe o fluxo normal de tráfego.
 * Seu funcionamento se dá da seguinte forma:
 *  1. Anunciar a emergência ao sistema, forçando a thread controladora a reagir;
 *  2. Aguardar o controlador limpar o cruzamento e abrir a passagem para apenas as ambulâncias;
 *  3. Atravessar o cruzamento rapidamente;
 *  4. Sinalizar o fim da emergência, permitindo que o sistema retorne à operação normal;
 *
 * @param arg Um ponteiro genérico (void*) para uma estrutura VeiculoArgs alocada dinamicamente, contendo a direção de origem da ambulância.
 * @return void* Sempre retorna NULL.
 */
void * ambulancia(void* arg){
    int id;                                     // Declaração da variável de id local para a thread
    int tempo;                                  // Variável para calcular o tempo que será usado no sleep com rand
    VeiculoArgs *args = (VeiculoArgs*) arg;     // Converte e extrai os argumentos passados pela thread main
    Direcao direcao_ambulancia = args->direcao; // Libera a memória dos argumentos, uma vez que os dados já foram copiados localmente
    free(arg);

    // Adquire o lock específico dos contadores para garantir que a leitura e o incremento
    // do id sejam uma operação que evite com que dois carros da mesma direção peguem o mesmo id
    pthread_mutex_lock(&cruzamento.lock_contadores_id);
    // Pega o próximo id disponível para esta direção
    id = cruzamento.contadores_id_ambulancias[direcao_ambulancia];
    // Incrementa o contador para a próxima thread da mesma direção
    cruzamento.contadores_id_ambulancias[direcao_ambulancia]++;
    pthread_mutex_unlock(&cruzamento.lock_contadores_id);

    while(1){
        // Notifica o sistema sobre a aproximação de um veículo de alta prioridade.
        printf("AMBULANCIA DA DIRECAO %s SE APROXIMANDO EM EMERGENCIA!\n", nome_direcao[direcao_ambulancia]);
        fflush(stdout);

        // Adquire o lock principal para alterar o estado global
        pthread_mutex_lock(&cruzamento.lock);
        cruzamento.modo_emergencia = true;  // Ativa a flag de emergência
        // Acorda todas as threads em espera, especialmente a thread 'fluxo_trafego', para que possa detectar a flag modo_emergencia e iniciar o protocolo
        pthread_cond_broadcast(&cruzamento.pode_cruzar);
        // Libera o lock imediatamente para evitar deadlock com a thread controladora
        pthread_mutex_unlock(&cruzamento.lock);
        
        // Pequena pausa para a thread controladora possa ter tempo de reagir e começar a limpar o cruzamento
        sleep(1);

        // Adquire o lock principal para se entrar na fila de espera
        pthread_mutex_lock(&cruzamento.lock);
        cruzamento.ambulancias_esperando[direcao_ambulancia]++;

        // Loop de espera condicional queaguarda até que o controlador mude o estado para um fluxo de ambulância compatível com sua direção
        while(!pode_passar(direcao_ambulancia, cruzamento.estado_atual, TIPO_AMBULANCIA)){
            printf("AMBULANCIA %d (%s) ESPERANDO PARA PASSAR.\n", id, nome_direcao[direcao_ambulancia]);
            fflush(stdout);
            pthread_cond_wait(&cruzamento.pode_cruzar, &cruzamento.lock);
        }

        // Se saiu do loop, a passagem foi liberada
        cruzamento.ambulancias_esperando[direcao_ambulancia]--;
        cruzamento.ambulancias_no_cruzamento++;
        printf("AMBULANCIA %d (%s) ENTROU NO CRUZAMENTO.\n", id, nome_direcao[direcao_ambulancia]);
        fflush(stdout);
        
        // Libera o lock antes de simular a travessia, permitindo que outras ambulâncias do mesmo fluxo entrem concorrentemente
        pthread_mutex_unlock(&cruzamento.lock);

        // Simula a travessia rápida do cruzamento
        sleep(2);

        // Readquire o lock para finalizar a emergência de forma segura
        pthread_mutex_lock(&cruzamento.lock);
        cruzamento.ambulancias_no_cruzamento--;
        cruzamento.modo_emergencia = false;     // Desativa a flag de emergência.
        printf("AMBULANCIA %d (%s) SAIU DO CRUZAMENTO.\n", id, nome_direcao[direcao_ambulancia]);
        fflush(stdout);
        
        // Notifica todas as threads que a emergência acabou. Isso é feito para "liberar" a thread 'fluxo_trafego', que estava aguardando esta condição
        pthread_cond_broadcast(&cruzamento.pode_cruzar);
        pthread_mutex_unlock(&cruzamento.lock);

        // Simula um tempo de percurso longo e aleatório antes de iniciar uma nova emergência, tornando estes eventos mais esporádicos e realistas na simulação.
        pthread_mutex_lock(&cruzamento.lock_rand);
        tempo = 30 + (rand() % 30);
        pthread_mutex_unlock(&cruzamento.lock_rand);
        sleep(tempo);
    }
    return NULL;
}

/**
 * @brief Função da Thread controladora do cruzamento.Opera em um loop infinito, implementando uma máquina de estados que gerencia o fluxo de
 * tráfego. A cada ciclo, ela avalia o estado do cruzamento e decide qual ação tomar, alternando entre dois modos principais:
 *      1. Modo de Emergência: Ativado quando uma ambulância chega. Este modo tem prioridade máxima, interrompe o fluxo normal, esvazia o 
 * cruzamento e libera a passagem para a ambulância.
 *      2.  Modo Normal: Operação padrão que calcula a demanda de carros em cada fluxo, abre o sinal para a via mais congestionada por um 
 * tempo dinâmico e previne starvation.
 *
 * @param arg Não utilizado nesta implementação (NULL é passado na criação da thread).
 * @return void* Sempre retorna NULL, pois a thread nunca termina.
 */
void * fluxo_trafego(void* arg){
    int i;      // Variável do laço for

    // Declaração de variáveis locais para o ciclo de decisão
    EstadoFluxo proximo_estado;
    float tempo_calculado;
    int demanda_ns, demanda_lo, num_carros, tempo_final, demanda_amb_ns, demanda_amb_lo, num_ambulancias;
    bool fila_ativa_esvaziou = false;

    while(1){
        // Pausa inicial em cada ciclo para permitir que as filas de veículos se formem antes de tomar uma decisão, evitando alternâncias de fluxo 
        // muito rápidas com o cruzamento vazio
        sleep(2);

        // Adquire o lock principal para garantir acesso exclusivo a todas as variáveis compartilhadas na struct cruzamento
        pthread_mutex_lock(&cruzamento.lock);

        // Verifica a flag de emergência para decidir qual protocolo seguir
        if(cruzamento.modo_emergencia){

            // Garante que o cruzamento esteja livre de carros normais antes de liberar a passagem para a ambulância
            while(cruzamento.carros_no_cruzamento > 0){
                printf("---------------- ESPERANDO %d CARRO(S) SAIREM PARA TOMAR A PROXIMA DECISAO ----------------\n", cruzamento.carros_no_cruzamento);
                pthread_cond_wait(&cruzamento.pode_cruzar, &cruzamento.lock);
            }
            
            // Calcula a demanda de ambulâncias para priorizar o fluxo correto
            demanda_amb_ns = cruzamento.ambulancias_esperando[NORTE] + cruzamento.ambulancias_esperando[SUL];
            demanda_amb_lo = cruzamento.ambulancias_esperando[LESTE] + cruzamento.ambulancias_esperando[OESTE];

            if(demanda_amb_ns >= demanda_amb_lo) proximo_estado = AMBULANCIA_NS;
            else proximo_estado = AMBULANCIA_LO;

            cruzamento.estado_atual = proximo_estado;

            printf("---------------- !!! EMERGENCIA !!! ----------------\n");
            printf("---------------- !!! ABERTO PARA: AMBULANCIA(S) %s !!! ----------------\n", (proximo_estado == AMBULANCIA_NS) ? "NORTE-SUL" : "LESTE-OESTE");

            // Notifica as ambulâncias e aguarda o fim da emergência
            pthread_cond_broadcast(&cruzamento.pode_cruzar);

            // O controlador entra em um estado de espera passiva. Prosseguirá quando a última ambulância a sair definir modo_emergencia para false e der broadcast
            while(cruzamento.modo_emergencia){
                pthread_cond_wait(&cruzamento.pode_cruzar, &cruzamento.lock);
            }
            printf("---------------- !!! EMERGENCIA FINALIZADA !!! ----------------\n");
            printf("---------------- VOLTANDO AO MODO NORMAL ----------------\n");

            // Libera o lock no final do ciclo de emergência
            pthread_mutex_unlock(&cruzamento.lock);
        }
        else{
            // Garante que o cruzamento esteja livre antes de abrir para um novo fluxo
            while(cruzamento.carros_no_cruzamento > 0){
                printf("---------------- ESPERANDO %d CARRO(S) SAIREM PARA MUDAR O FLUXO ----------------\n", cruzamento.carros_no_cruzamento);
                pthread_cond_wait(&cruzamento.pode_cruzar, &cruzamento.lock);
            }

            // Calcula a demanda de carros para decidir o próximo fluxo
            demanda_ns = cruzamento.carros_esperando[NORTE] + cruzamento.carros_esperando[SUL];
            demanda_lo = cruzamento.carros_esperando[LESTE] + cruzamento.carros_esperando[OESTE];

            // Lógica de decisão para o próximo estado
            if(demanda_ns >= demanda_lo){
                proximo_estado = FLUXO_NS;
                num_carros = demanda_ns;
            } 
            else{
                proximo_estado = FLUXO_LO;
                num_carros = demanda_lo;
            }
            cruzamento.estado_atual = proximo_estado;
            
            // Cálculo de Tempo Dinâmico: Define a duração da passagem que cada fluco possui
            if(num_carros > 0) tempo_calculado = T_BASE + ((num_carros - 1) * FATOR_CARRO);
            else tempo_calculado = T_BASE;

            tempo_final = (int) tempo_calculado;

            // Aplica os limites de tempo mínimo e máximo para garantir fluidez e prevenir starvation
            if(tempo_final > T_MAXIMO) tempo_final = T_MAXIMO;
            else if (tempo_final < T_MINIMO) tempo_final = T_MINIMO;
            
            printf("---------------- FLUXO %s ABERTO POR ATE %d SEGUNDOS PARA %d CARROS ----------------\n", 
                cruzamento.estado_atual == FLUXO_NS ? "NORTE-SUL" : "LESTE-OESTE", tempo_final, num_carros);

            // Notifica os carros e libera o lock antes da espera
            pthread_cond_broadcast(&cruzamento.pode_cruzar);
            pthread_mutex_unlock(&cruzamento.lock);
            
            // A thread dorme em incrementos de 1 segundo, verificando se a fila esvaziou
            for(i = 0; i < tempo_final; i++){
                sleep(1);
                
                // Readquire o lock brevemente apenas para a verificação
                pthread_mutex_lock(&cruzamento.lock);

                fila_ativa_esvaziou = false;
                
                if(proximo_estado == FLUXO_NS){
                    if(cruzamento.carros_esperando[NORTE] == 0 && cruzamento.carros_esperando[SUL] == 0) fila_ativa_esvaziou = true;
                } 
                else{
                    if(cruzamento.carros_esperando[LESTE] == 0 && cruzamento.carros_esperando[OESTE] == 0) fila_ativa_esvaziou = true;
                }
                
                pthread_mutex_unlock(&cruzamento.lock);
                
                // Se a fila esvaziou, interrompe a espera para otimizar o fluxo
                if(fila_ativa_esvaziou){
                    printf("---------------- FILA ATUAL DE CARROS (%s) ESVAZIOU, ENCERRANDO PASSAGEM ----------------\n", 
                        proximo_estado == FLUXO_NS ? "NORTE-SUL" : "LESTE-OESTE");
                    break;  // Sai do laço for e inicia um novo ciclo de decisão.
                }
            }
        }
    }
    return NULL;
}

/**
 * @brief Thread principal main responsáve pela inicialização de variáveis e criação das threads que serão utilizadas ao longo do código.
 * 
 * @param argc inteiro que indica o número de argumentos de linha de comando fornecidos quando o programa é executado, incluindo o próprio nome do programa
 * @param argv parâmetro passado para a função principal, permitindo que o programa receba argumentos de linha de comando
 * @return int declaração usada para indicar a execução bem-sucedida do programa para o sistema operacional
 */
int main(int argc, char * argv[]){
    int i;                                       // Variável do laço for
    int thread_idx = 0;                          // Contador para gerar os ids únicos de cada thread de veículos                       
    pthread_t veiculos_t[TOTAL_VEICULOS], fluxo; // Threads dos veículos envolvidos no cruzamento e de controle do cruzamento respectivamente

    // Inicialização dos elementos de threads (locks, condicionais), contadores e identificadores utilizados no código
    pthread_mutex_init(&cruzamento.lock, NULL);
    pthread_mutex_init(&cruzamento.lock_rand, NULL);
    pthread_mutex_init(&cruzamento.lock_contadores_id, NULL);
    pthread_cond_init(&cruzamento.pode_cruzar, NULL);
    cruzamento.estado_atual = FLUXO_NS;
    cruzamento.carros_no_cruzamento = 0;
    cruzamento.ambulancias_no_cruzamento = 0;
    cruzamento.modo_emergencia = false;
    for(i = 0; i < NUM_DIRECOES; i++){
        cruzamento.carros_esperando[i] = 0;
        cruzamento.contadores_id_carros[i] = 1;
        cruzamento.contadores_id_ambulancias[i] = 1;
        cruzamento.ambulancias_esperando[i] = 0;
    }
    
    // Criação da thread controladora (fluxo_trafego)
    pthread_create(&fluxo, NULL, fluxo_trafego, NULL);

    // Criação das threads dos carros em todas as direções
    for(i = 0; i < CARROS_NORTE; i++){
        VeiculoArgs *args = malloc(sizeof(VeiculoArgs));
        args->direcao = NORTE;
        pthread_create(&veiculos_t[thread_idx], NULL, carros, args);
        thread_idx++;
    }

    for(i = 0; i < CARROS_SUL; i++){
        VeiculoArgs *args = malloc(sizeof(VeiculoArgs));
        args->direcao = SUL;
        pthread_create(&veiculos_t[thread_idx], NULL, carros, args);
        thread_idx++;
    }

    for(i = 0; i < CARROS_LESTE; i++){
        VeiculoArgs *args = malloc(sizeof(VeiculoArgs));
        args->direcao = LESTE;
        pthread_create(&veiculos_t[thread_idx], NULL, carros, args);
        thread_idx++;
    }

    for(i = 0; i < CARROS_OESTE; i++){
        VeiculoArgs *args = malloc(sizeof(VeiculoArgs));
        args->direcao = OESTE;
        pthread_create(&veiculos_t[thread_idx], NULL, carros, args);
        thread_idx++;
    }



    // Criação das threads das ambulâncias em todas as direções
    for(i = 0; i < AMBULANCIA_NORTE; i++){
        VeiculoArgs *args = malloc(sizeof(VeiculoArgs));
        args->direcao = NORTE;
        pthread_create(&veiculos_t[thread_idx], NULL, ambulancia, args);
        thread_idx++;
    }

    for(i = 0; i < AMBULANCIA_SUL; i++){
        VeiculoArgs *args = malloc(sizeof(VeiculoArgs));
        args->direcao = SUL;
        pthread_create(&veiculos_t[thread_idx], NULL, ambulancia, args);
        thread_idx++;
    }

    for(i = 0; i < AMBULANCIA_LESTE; i++){
        VeiculoArgs *args = malloc(sizeof(VeiculoArgs));
        args->direcao = LESTE;
        pthread_create(&veiculos_t[thread_idx], NULL, ambulancia, args);
        thread_idx++;
    }

    for(i = 0; i < AMBULANCIA_OESTE; i++){
        VeiculoArgs *args = malloc(sizeof(VeiculoArgs));
        args->direcao = OESTE;
        pthread_create(&veiculos_t[thread_idx], NULL, ambulancia, args);
        thread_idx++;
    }

    // Juntar as threads
    for(i = 0; i < TOTAL_VEICULOS; i++) pthread_join(veiculos_t[i], NULL);


    return 0;
}