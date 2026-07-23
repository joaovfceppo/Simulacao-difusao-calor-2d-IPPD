#include <mpi.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int i0, j0, i1, j1;
    double temperatura;
} FonteCalor;

typedef struct {
    int n_linhas, n_colunas;
    double alpha;
    double dx, dy, dt;
    int n_passos;
    int intervalo_salvamento;

    double temp_topo, temp_base, temp_esquerda, temp_direita;
    double temp_inicial;

    int n_fontes;
    FonteCalor *fontes;
} Configuracao;

typedef struct {
    int n_linhas, n_colunas;
    double alpha, dx, dy, dt;
    int n_passos, intervalo_salvamento;
    double temp_topo, temp_base, temp_esquerda, temp_direita, temp_inicial;
    int n_fontes;
} ConfiguracaoEscalar;

int proxima_linha_util(FILE *arquivo, char *buf, size_t tamanho) {
    while (fgets(buf, (int)tamanho, arquivo) != NULL) {
        char *p = buf;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\0') {
            continue;
        }
        return 1;
    }
    return 0;
}

int le_configuracao(const char *caminho, Configuracao *cfg) {
    FILE *arquivo = fopen(caminho, "r");
    if (arquivo == NULL) {
        return -1;
    }

    char linha[512];
    int ok = 1;

    ok &= proxima_linha_util(arquivo, linha, sizeof(linha));
    ok &= (sscanf(linha, "%d %d", &cfg->n_linhas, &cfg->n_colunas) == 2);

    ok &= proxima_linha_util(arquivo, linha, sizeof(linha));
    ok &= (sscanf(linha, "%lf", &cfg->alpha) == 1);

    ok &= proxima_linha_util(arquivo, linha, sizeof(linha));
    ok &= (sscanf(linha, "%lf %lf %lf", &cfg->dx, &cfg->dy, &cfg->dt) == 3);

    ok &= proxima_linha_util(arquivo, linha, sizeof(linha));
    ok &= (sscanf(linha, "%d %d", &cfg->n_passos, &cfg->intervalo_salvamento) == 2);

    ok &= proxima_linha_util(arquivo, linha, sizeof(linha));
    ok &= (sscanf(linha, "%lf %lf %lf %lf", &cfg->temp_topo, &cfg->temp_base,
                   &cfg->temp_esquerda, &cfg->temp_direita) == 4);

    ok &= proxima_linha_util(arquivo, linha, sizeof(linha));
    ok &= (sscanf(linha, "%lf", &cfg->temp_inicial) == 1);

    ok &= proxima_linha_util(arquivo, linha, sizeof(linha));
    ok &= (sscanf(linha, "%d", &cfg->n_fontes) == 1);

    if (!ok || cfg->n_fontes < 0) {
        fclose(arquivo);
        return -1;
    }

    cfg->fontes = malloc(sizeof(FonteCalor) * (size_t)(cfg->n_fontes > 0 ? cfg->n_fontes : 1));
    for (int k = 0; k < cfg->n_fontes; k++) {
        if (!proxima_linha_util(arquivo, linha, sizeof(linha))) { ok = 0; break; }
        int lidos = sscanf(linha, "%d %d %d %d %lf",
                            &cfg->fontes[k].i0, &cfg->fontes[k].j0,
                            &cfg->fontes[k].i1, &cfg->fontes[k].j1,
                            &cfg->fontes[k].temperatura);
        if (lidos != 5) { ok = 0; break; }
    }

    fclose(arquivo);
    if (!ok) {
        free(cfg->fontes);
        return -1;
    }
    return 0;
}

void distribui_configuracao(Configuracao *cfg, int rank) {
    ConfiguracaoEscalar base;

    if (rank == 0) {
        base.n_linhas = cfg->n_linhas;
        base.n_colunas = cfg->n_colunas;
        base.alpha = cfg->alpha;
        base.dx = cfg->dx;
        base.dy = cfg->dy;
        base.dt = cfg->dt;
        base.n_passos = cfg->n_passos;
        base.intervalo_salvamento = cfg->intervalo_salvamento;
        base.temp_topo = cfg->temp_topo;
        base.temp_base = cfg->temp_base;
        base.temp_esquerda = cfg->temp_esquerda;
        base.temp_direita = cfg->temp_direita;
        base.temp_inicial = cfg->temp_inicial;
        base.n_fontes = cfg->n_fontes;
    }

    MPI_Bcast(&base, sizeof(ConfiguracaoEscalar), MPI_BYTE, 0, MPI_COMM_WORLD);

    if (rank != 0) {
        cfg->n_linhas = base.n_linhas;
        cfg->n_colunas = base.n_colunas;
        cfg->alpha = base.alpha;
        cfg->dx = base.dx;
        cfg->dy = base.dy;
        cfg->dt = base.dt;
        cfg->n_passos = base.n_passos;
        cfg->intervalo_salvamento = base.intervalo_salvamento;
        cfg->temp_topo = base.temp_topo;
        cfg->temp_base = base.temp_base;
        cfg->temp_esquerda = base.temp_esquerda;
        cfg->temp_direita = base.temp_direita;
        cfg->temp_inicial = base.temp_inicial;
        cfg->n_fontes = base.n_fontes;

        cfg->fontes = malloc(sizeof(FonteCalor) * (size_t)(cfg->n_fontes > 0 ? cfg->n_fontes : 1));
    }

    if (cfg->n_fontes > 0) {
        MPI_Bcast(cfg->fontes, (int)(sizeof(FonteCalor) * (size_t)cfg->n_fontes),
                  MPI_BYTE, 0, MPI_COMM_WORLD);
    }
}

int verifica_estabilidade(const Configuracao *cfg) {
    double r = cfg->alpha * cfg->dt * (1.0 / (cfg->dx * cfg->dx) + 1.0 / (cfg->dy * cfg->dy));
    return r <= 0.5;
}

int ponto_em_fonte(const Configuracao *cfg, int linha_global, int coluna, double *temp) {
    for (int k = 0; k < cfg->n_fontes; k++) {
        FonteCalor *f = &cfg->fontes[k];
        if (linha_global >= f->i0 && linha_global <= f->i1 &&
            coluna >= f->j0 && coluna <= f->j1) {
            *temp = f->temperatura;
            return 1;
        }
    }
    return 0;
}

int obtem_temp_borda(const Configuracao *cfg, int linha_global, int coluna, double *temp) {
    if (linha_global == 0) { *temp = cfg->temp_topo; return 1; }
    if (linha_global == cfg->n_linhas - 1) { *temp = cfg->temp_base; return 1; }
    if (coluna == 0) { *temp = cfg->temp_esquerda; return 1; }
    if (coluna == cfg->n_colunas - 1) { *temp = cfg->temp_direita; return 1; }
    return 0;
}

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

void escreve_saida_global(const Configuracao *cfg, const double *grade_completa,
                           int passo, const char *dir_saida) {
    char caminho[512];
    snprintf(caminho, sizeof(caminho), "%s/saida_passo%06d.txt", dir_saida, passo);

    FILE *arquivo = fopen(caminho, "w");
    if (arquivo == NULL) {
        fprintf(stderr,
            "Aviso: nao foi possivel escrever '%s'. O diretorio de saida existe?\n",
            caminho);
        return;
    }

    fprintf(arquivo, "# n_linhas=%d n_colunas=%d passo=%d\n",
            cfg->n_linhas, cfg->n_colunas, passo);

    for (int i = 0; i < cfg->n_linhas; i++) {
        for (int j = 0; j < cfg->n_colunas; j++) {
            fprintf(arquivo, "%.4f", grade_completa[i * cfg->n_colunas + j]);
            if (j < cfg->n_colunas - 1) fprintf(arquivo, " ");
        }
        fprintf(arquivo, "\n");
    }

    fclose(arquivo);
}

int main(int argc, char **argv) {
    int rank, n_processos;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &n_processos);

    if (argc < 2) {
        if (rank == 0) {
            fprintf(stderr, "Uso: %s <arquivo_entrada> [diretorio_saida]\n", argv[0]);
            fprintf(stderr, "Exemplo: %s data/entrada/entrada.txt data/saida\n", argv[0]);
        }
        MPI_Finalize();
        return 1;
    }
    const char *dir_saida = (argc >= 3) ? argv[2] : ".";

    Configuracao cfg;
    memset(&cfg, 0, sizeof(cfg));
    int leitura_ok = 1;
    if (rank == 0) {
        leitura_ok = (le_configuracao(argv[1], &cfg) == 0);
    }
    MPI_Bcast(&leitura_ok, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (!leitura_ok) {
        if (rank == 0) {
            fprintf(stderr,
                "[processo 0] Erro ao ler o arquivo de entrada '%s'. "
                "Verifique se o arquivo existe e segue o formato esperado.\n",
                argv[1]);
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    distribui_configuracao(&cfg, rank);

    if (!verifica_estabilidade(&cfg)) {
        if (rank == 0) {
            double r = cfg.alpha * cfg.dt * (1.0/(cfg.dx*cfg.dx) + 1.0/(cfg.dy*cfg.dy));
            fprintf(stderr,
                "Configuracao instavel: alpha*dt*(1/dx^2+1/dy^2) = %.4f (precisa ser <= 0.5).\n"
                "Reduza o dt ou aumente dx/dy no arquivo de entrada.\n", r);
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    if (cfg.n_linhas % n_processos != 0) {
        if (rank == 0) {
            fprintf(stderr,
                "Por enquanto N_LINHAS (%d) precisa ser divisivel pelo numero "
                "de processos (%d). Ajuste um dos dois.\n",
                cfg.n_linhas, n_processos);
        }
        MPI_Finalize();
        return 1;
    }
    int linhas_locais = cfg.n_linhas / n_processos;

    double **atual = aloca_grade(linhas_locais + 2, cfg.n_colunas);
    double **proxima = aloca_grade(linhas_locais + 2, cfg.n_colunas);

    int linha_global_inicial = rank * linhas_locais;

    for (int i = 0; i < linhas_locais + 2; i++) {
        int linha_global = linha_global_inicial + (i - 1);
        for (int j = 0; j < cfg.n_colunas; j++) {
            double temp_borda;
            double temp_fonte;
            if (obtem_temp_borda(&cfg, linha_global, j, &temp_borda)) {
                atual[i][j] = temp_borda;
            } else if (ponto_em_fonte(&cfg, linha_global, j, &temp_fonte)) {
                atual[i][j] = temp_fonte;
            } else {
                atual[i][j] = cfg.temp_inicial;
            }
        }
    }

    int vizinho_cima  = (rank == 0) ? MPI_PROC_NULL : rank - 1;
    int vizinho_baixo = (rank == n_processos - 1) ? MPI_PROC_NULL : rank + 1;

    double dx2 = cfg.dx * cfg.dx;
    double dy2 = cfg.dy * cfg.dy;

    double *grade_completa = NULL;
    if (rank == 0) {
        grade_completa = malloc((size_t)cfg.n_linhas * cfg.n_colunas * sizeof(double));
    }
    int tamanho_fatia = linhas_locais * cfg.n_colunas;

    MPI_Gather(atual[1], tamanho_fatia, MPI_DOUBLE,
               grade_completa, tamanho_fatia, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    if (rank == 0) {
        escreve_saida_global(&cfg, grade_completa, 0, dir_saida);
    }

    double inicio = MPI_Wtime();

    for (int passo = 0; passo < cfg.n_passos; passo++) {

        MPI_Sendrecv(atual[1], cfg.n_colunas, MPI_DOUBLE, vizinho_cima, 0,
                     atual[0], cfg.n_colunas, MPI_DOUBLE, vizinho_cima, 0,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        MPI_Sendrecv(atual[linhas_locais], cfg.n_colunas, MPI_DOUBLE, vizinho_baixo, 0,
                     atual[linhas_locais + 1], cfg.n_colunas, MPI_DOUBLE, vizinho_baixo, 0,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        #pragma omp parallel for collapse(2)
        for (int i = 1; i <= linhas_locais; i++) {
            for (int j = 0; j < cfg.n_colunas; j++) {
                int linha_global = linha_global_inicial + (i - 1);
                double temp_borda, temp_fonte;

                if (obtem_temp_borda(&cfg, linha_global, j, &temp_borda)) {
                    proxima[i][j] = temp_borda;
                } else if (ponto_em_fonte(&cfg, linha_global, j, &temp_fonte)) {
                    proxima[i][j] = temp_fonte;
                } else {
                    double laplaciano_x = (atual[i+1][j] - 2.0*atual[i][j] + atual[i-1][j]) / dx2;
                    double laplaciano_y = (atual[i][j+1] - 2.0*atual[i][j] + atual[i][j-1]) / dy2;
                    proxima[i][j] = atual[i][j] + cfg.alpha * cfg.dt * (laplaciano_x + laplaciano_y);
                }
            }
        }

        double **tmp = atual;
        atual = proxima;
        proxima = tmp;

        int passo_atual = passo + 1;
        int eh_intervalo = (cfg.intervalo_salvamento > 0) &&
                            (passo_atual % cfg.intervalo_salvamento == 0);
        int eh_ultimo = (passo_atual == cfg.n_passos);
        if (eh_intervalo || eh_ultimo) {
            MPI_Gather(atual[1], tamanho_fatia, MPI_DOUBLE,
                       grade_completa, tamanho_fatia, MPI_DOUBLE, 0, MPI_COMM_WORLD);
            if (rank == 0) {
                escreve_saida_global(&cfg, grade_completa, passo_atual, dir_saida);
            }
        }
    }

    double fim = MPI_Wtime();

    double soma_local = 0.0;
    for (int i = 1; i <= linhas_locais; i++)
        for (int j = 0; j < cfg.n_colunas; j++)
            soma_local += atual[i][j];
    double media_local = soma_local / (linhas_locais * cfg.n_colunas);

    double media_global = 0.0;
    MPI_Reduce(&media_local, &media_global, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        media_global /= n_processos;
        printf("Simulacao concluida: %d passos, %d processos MPI, grade %dx%d\n",
               cfg.n_passos, n_processos, cfg.n_linhas, cfg.n_colunas);
        printf("Configuracao lida de: %s (%d fontes de calor), apenas pelo rank 0\n",
               argv[1], cfg.n_fontes);
        printf("Saida (unica, via MPI_Gather) escrita em: %s (a cada %d passos)\n",
               dir_saida, cfg.intervalo_salvamento);
        printf("Temperatura media final (global, aproximada): %.4f\n", media_global);
        printf("Tempo de execucao: %.4f segundos\n", fim - inicio);
        free(grade_completa);
    }

    free(cfg.fontes);
    libera_grade(atual);
    libera_grade(proxima);
    MPI_Finalize();
    return 0;
}