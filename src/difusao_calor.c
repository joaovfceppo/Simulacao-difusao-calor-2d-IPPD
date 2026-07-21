#include <mpi.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#define N_LINHAS   20
#define N_COLUNAS  20
#define N_PASSOS   500
#define TEMP_BORDA   0.0
#define TEMP_INICIAL 20.0 
#define TEMP_FONTE   100.0 


double **aloca_grade(int linhas, int colunas) {
    double **grade = malloc(linhas * sizeof(double *));
    double *bloco = malloc((size_t)linhas * colunas * sizeof(double));
    for (int i = 0; i < linhas; i++) {
        grade[i] = &bloco[i * colunas];
    }
    return grade;
}

void libera_grade(double **grade) {
    free(grade[0]);
    free(grade);
}

int main(int argc, char **argv) {
    int rank, n_processos;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &n_processos);

    if (N_LINHAS % n_processos != 0) {
        if (rank == 0) {
            fprintf(stderr,
                "Por enquanto N_LINHAS (%d) precisa ser divisível pelo numero "
                "de processos (%d). Ajuste um dos dois.\n",
                N_LINHAS, n_processos);
        }
        MPI_Finalize();
        return 1;
    }
    int linhas_locais = N_LINHAS / n_processos;


    double **atual = aloca_grade(linhas_locais + 2, N_COLUNAS);
    double **proxima = aloca_grade(linhas_locais + 2, N_COLUNAS);

    int linha_global_inicial = rank * linhas_locais;

    for (int i = 0; i < linhas_locais + 2; i++) {
        for (int j = 0; j < N_COLUNAS; j++) {
            int linha_global = linha_global_inicial + (i - 1);
            int eh_borda = (linha_global == 0 || linha_global == N_LINHAS - 1 ||
                             j == 0 || j == N_COLUNAS - 1);
            atual[i][j] = eh_borda ? TEMP_BORDA : TEMP_INICIAL;
        }
    }


    int centro_linha = N_LINHAS / 2;
    int centro_coluna = N_COLUNAS / 2;
    if (centro_linha >= linha_global_inicial &&
        centro_linha < linha_global_inicial + linhas_locais) {
        int i_local = (centro_linha - linha_global_inicial) + 1;
        atual[i_local][centro_coluna] = TEMP_FONTE;
    }

    int vizinho_cima  = (rank == 0) ? MPI_PROC_NULL : rank - 1;
    int vizinho_baixo = (rank == n_processos - 1) ? MPI_PROC_NULL : rank + 1;

    double inicio = MPI_Wtime();

    for (int passo = 0; passo < N_PASSOS; passo++) {


        MPI_Sendrecv(atual[1], N_COLUNAS, MPI_DOUBLE, vizinho_cima, 0,
                     atual[0], N_COLUNAS, MPI_DOUBLE, vizinho_cima, 0,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        MPI_Sendrecv(atual[linhas_locais], N_COLUNAS, MPI_DOUBLE, vizinho_baixo, 0,
                     atual[linhas_locais + 1], N_COLUNAS, MPI_DOUBLE, vizinho_baixo, 0,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);


        #pragma omp parallel for collapse(2)
        for (int i = 1; i <= linhas_locais; i++) {
            for (int j = 0; j < N_COLUNAS; j++) {
                int linha_global = linha_global_inicial + (i - 1);
                int eh_borda = (linha_global == 0 || linha_global == N_LINHAS - 1 ||
                                 j == 0 || j == N_COLUNAS - 1);
                int eh_fonte = (linha_global == centro_linha && j == centro_coluna);

                if (eh_borda) {
                    proxima[i][j] = TEMP_BORDA;
                } else if (eh_fonte) {
                    proxima[i][j] = TEMP_FONTE; 
                } else {
                    proxima[i][j] = 0.25 * (atual[i-1][j] + atual[i+1][j] +
                                             atual[i][j-1] + atual[i][j+1]);
                }
            }
        }

        double **tmp = atual;
        atual = proxima;
        proxima = tmp;
    }

    double fim = MPI_Wtime();

    double soma_local = 0.0;
    for (int i = 1; i <= linhas_locais; i++)
        for (int j = 0; j < N_COLUNAS; j++)
            soma_local += atual[i][j];
    double media_local = soma_local / (linhas_locais * N_COLUNAS);

    double media_global = 0.0;
    MPI_Reduce(&media_local, &media_global, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        media_global /= n_processos;
        printf("Simulacao concluida: %d passos, %d processos MPI, grade %dx%d\n",
               N_PASSOS, n_processos, N_LINHAS, N_COLUNAS);
        printf("Temperatura media final (global, aproximada): %.4f\n", media_global);
        printf("Tempo de execucao: %.4f segundos\n", fim - inicio);
    }

    libera_grade(atual);
    libera_grade(proxima);
    MPI_Finalize();
    return 0;
}