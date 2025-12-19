#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

// Definições globais
#define MAX_PROC 100
#define FILENAME_IN "EntradaProcessos.txt"
#define FILENAME_OUT "SaidaResultados.txt"

// Estrutura do Processo
typedef struct {
    int id;
    int chegada;
    int prioridade;
    int t_cpu_total;
    int t_restante;
    int t_conclusao;
} Processo;

// Estrutura de Fila para o Round Robin
typedef struct {
    int itens[MAX_PROC * 1000]; // Tamanho seguro para a simulação
    int frente;
    int tras;
} Fila;

// --- Funções Auxiliares de Fila ---
void inicializarFila(Fila* f) {
    f->frente = 0;
    f->tras = -1;
}

int filaVazia(Fila* f) {
    return f->tras < f->frente;
}

void enfileirar(Fila* f, int id_proc) {
    f->tras++;
    f->itens[f->tras] = id_proc;
}

int desenfileirar(Fila* f) {
    int id = f->itens[f->frente];
    f->frente++;
    return id;
}

// --- Função de Comparação para o qsort ---
// Garante que processamos IDs menores primeiro em caso de empate (requisito do projeto)
int compararPorID(const void *a, const void *b) {
    Processo *pA = (Processo *)a;
    Processo *pB = (Processo *)b;
    return pA->id - pB->id;
}

// Reseta o estado dos processos para rodar o segundo algoritmo
void resetarProcessos(Processo dest[], Processo orig[], int n) {
    for (int i = 0; i < n; i++) {
        dest[i] = orig[i];
        dest[i].t_restante = orig[i].t_cpu_total;
        dest[i].t_conclusao = 0;
    }
}

// ============================================================================
// ALGORITMO 1: PRIORIDADE PREEMPTIVA
// ============================================================================
void rodarPrioridade(Processo procs[], int n, int tTroca, FILE *out) {
    fprintf(out, "***** ALGORITMO: PRIORIDADE *****\n");
    
    int tempo = 0;
    int completados = 0;
    int id_atual = -1; 
    int chaveamentos = 0;
    int t_troca_acumulado = 0;
    
    // Loop de simulação (milissegundo a milissegundo)
    while (completados < n) {
        // 1. Escolher o processo
        int id_melhor = -1;
        int prioridade_melhor = INT_MAX; // Menor valor = Maior prioridade

        // Varre lista (já ordenada por ID pelo qsort)
        for (int i = 0; i < n; i++) {
            if (procs[i].chegada <= tempo && procs[i].t_restante > 0) {
                if (procs[i].prioridade < prioridade_melhor) {
                    prioridade_melhor = procs[i].prioridade;
                    id_melhor = i;
                }
                // Se prioridade for igual, mantemos o primeiro encontrado (menor ID)
            }
        }

        // 2. Verificar Troca de Contexto
        if (id_melhor != -1) {
            if (id_atual != id_melhor) {
                // Se não é o primeiro start no tempo 0 (ou se consideramos custo inicial se tempo > 0)
                if (id_atual != -1 || tempo > 0) { 
                    fprintf(out, "%d-%d: Escalonador\n", tempo, tempo + tTroca);
                    tempo += tTroca;
                    t_troca_acumulado += tTroca;
                    chaveamentos++;
                }
                id_atual = id_melhor;
            }

            // 3. Executar
            fprintf(out, "%d-%d: Processo %d\n", tempo, tempo + 1, procs[id_atual].id);
            procs[id_atual].t_restante--;
            tempo++;

            // 4. Verificar fim
            if (procs[id_atual].t_restante == 0) {
                procs[id_atual].t_conclusao = tempo;
                completados++;
                // Não reseta id_atual aqui para permitir comparação na próxima iteração
                // Se ele acabou, na próxima ele não será eleito
            }
        } else {
            // CPU Ociosa
            fprintf(out, "%d-%d: Ocioso\n", tempo, tempo + 1);
            tempo++;
        }
    }

    // Relatório
    double soma_retorno = 0;
    for (int i = 0; i < n; i++) {
        soma_retorno += (procs[i].t_conclusao - procs[i].chegada);
    }
    double overhead = (tempo > 0) ? (double)t_troca_acumulado / tempo : 0.0;

    fprintf(out, "----------------------------------\n");
    fprintf(out, "Tempo total de simulacao: %d\n", tempo);
    fprintf(out, "Numero de chaveamentos: %d\n", chaveamentos);
    fprintf(out, "Tempo medio de retorno: %.2f\n", soma_retorno / n);
    fprintf(out, "Overhead: %.4f (%.2f%%)\n", overhead, overhead * 100);
    fprintf(out, "\n\n");
}

// ============================================================================
// ALGORITMO 2: ROUND ROBIN
// ============================================================================
void rodarRoundRobin(Processo procs[], int n, int quantum, int tTroca, FILE *out) {
    fprintf(out, "***** ALGORITMO: ROUND ROBIN (Quantum: %d) *****\n", quantum);

    int tempo = 0;
    int completados = 0;
    int chaveamentos = 0;
    int t_troca_acumulado = 0;
    
    Fila fila;
    inicializarFila(&fila);

    int id_atual = -1; 
    int quantum_restante = 0;
    
    while (completados < n) {

        // 1. CHEGADAS: Novos processos entram na fila (Ordenados por ID via array base)
        for (int i = 0; i < n; i++) {
            if (procs[i].chegada == tempo) {
                enfileirar(&fila, i);
            }
        }

        // 2. CARREGAR PROCESSO (Se ocioso)
        if (id_atual == -1) {
            if (!filaVazia(&fila)) {
                id_atual = desenfileirar(&fila);
                quantum_restante = quantum;
                // Assumindo sem custo de troca se estava ocioso e é o primeiro start
            }
        }

        // 3. EXECUÇÃO
        if (id_atual != -1) {
            fprintf(out, "%d-%d: Processo %d\n", tempo, tempo + 1, procs[id_atual].id);
            procs[id_atual].t_restante--;
            quantum_restante--;
            tempo++;

            int precisa_trocar = 0;
            int processo_acabou = 0;

            // Analisar estado pós-execução
            if (procs[id_atual].t_restante == 0) {
                procs[id_atual].t_conclusao = tempo;
                completados++;
                processo_acabou = 1;
                precisa_trocar = 1; 
            }
            else if (quantum_restante == 0) {
                precisa_trocar = 1;
            }

            if (precisa_trocar) {
                // CRÍTICO: Checar se alguém chegou EXATAMENTE durante este ms
                // Regra: Quem chega tem prioridade sobre quem volta pra fila
                for (int i = 0; i < n; i++) {
                    if (procs[i].chegada == tempo) {
                        enfileirar(&fila, i);
                    }
                }

                // Devolver processo atual à fila se não acabou
                if (!processo_acabou) {
                    enfileirar(&fila, id_atual);
                }

                // Realizar Troca se houver próximo
                if (!filaVazia(&fila)) {
                    // Custo da troca
                    fprintf(out, "%d-%d: Escalonador\n", tempo, tempo + tTroca);
                    
                    // Avançar tempo da troca (e verificar chegadas teóricas nesse intervalo)
                    // Simplificação: apenas avança o tempo. Chegadas exatas no fim da troca
                    // serão pegas no início do próximo loop while.
                    tempo += tTroca;
                    t_troca_acumulado += tTroca;
                    chaveamentos++;

                    // Carregar próximo
                    id_atual = desenfileirar(&fila);
                    quantum_restante = quantum;
                } else {
                    // Fila vazia
                    if (processo_acabou) {
                        id_atual = -1; // CPU Livre
                    } else {
                        // Quantum estourou mas só tem ele. Renova sem overhead.
                        quantum_restante = quantum;
                    }
                }
            }
        } else {
            // Ocioso
            fprintf(out, "%d-%d: Ocioso\n", tempo, tempo + 1);
            tempo++;
        }
    }

    // Relatório
    double soma_retorno = 0;
    for (int i = 0; i < n; i++) {
        soma_retorno += (procs[i].t_conclusao - procs[i].chegada);
    }
    double overhead = (tempo > 0) ? (double)t_troca_acumulado / tempo : 0.0;

    fprintf(out, "----------------------------------\n");
    fprintf(out, "Tempo total de simulacao: %d\n", tempo);
    fprintf(out, "Numero de chaveamentos: %d\n", chaveamentos);
    fprintf(out, "Tempo medio de retorno: %.2f\n", soma_retorno / n);
    fprintf(out, "Overhead: %.4f (%.2f%%)\n", overhead, overhead * 100);
}

// ============================================================================
// MAIN
// ============================================================================
int main() {
    FILE *fin = fopen(FILENAME_IN, "r");
    if (!fin) {
        printf("ERRO FATAL: Arquivo '%s' nao encontrado.\n", FILENAME_IN);
        printf("Certifique-se de criar o arquivo no mesmo diretorio do executavel.\n");
        return 1;
    }

    int nProc, quantum, tTroca;
    // Leitura segura da primeira linha (ignora espaços extras se houver)
    if (fscanf(fin, "%d , %d , %d", &nProc, &quantum, &tTroca) != 3) {
        printf("ERRO: Formato da primeira linha invalido (esperado: nProc,Quantum,Troca).\n");
        fclose(fin);
        return 1;
    }

    Processo *processos_orig = (Processo*) malloc(nProc * sizeof(Processo));
    Processo *processos_sim = (Processo*) malloc(nProc * sizeof(Processo));

    printf("Lendo %d processos do arquivo...\n", nProc);
    for (int i = 0; i < nProc; i++) {
        if (fscanf(fin, "%d , %d , %d , %d", 
            &processos_orig[i].id, 
            &processos_orig[i].chegada, 
            &processos_orig[i].prioridade, 
            &processos_orig[i].t_cpu_total) != 4) {
            printf("ERRO: Falha ao ler processo na linha %d (verifique as virgulas).\n", i+2);
            fclose(fin);
            return 1;
        }
        processos_orig[i].t_restante = processos_orig[i].t_cpu_total;
    }
    fclose(fin);

    // CRUCIAL: Ordenar por ID antes de começar a simulação
    // Isso garante que em empates de prioridade/chegada, o menor ID ganha.
    qsort(processos_orig, nProc, sizeof(Processo), compararPorID);

    FILE *fout = fopen(FILENAME_OUT, "w");
    if (!fout) {
        printf("ERRO: Nao foi possivel criar arquivo de saida.\n");
        return 1;
    }

    // Executar Prioridade
    resetarProcessos(processos_sim, processos_orig, nProc);
    rodarPrioridade(processos_sim, nProc, tTroca, fout);

    // Executar Round Robin
    resetarProcessos(processos_sim, processos_orig, nProc);
    rodarRoundRobin(processos_sim, nProc, quantum, tTroca, fout);

    printf("Simulacao concluida com sucesso!\n");
    printf("Resultados salvos em: %s\n", FILENAME_OUT);

    fclose(fout);
    free(processos_orig);
    free(processos_sim);

    return 0;
}