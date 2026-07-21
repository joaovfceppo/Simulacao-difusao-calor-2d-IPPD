# Simulação de Difusão de Calor 2D com MPI + OpenMP

Trabalho da disciplina de Introdução ao Processamento Paralelo e Distribuído (IPPD) — UFPel.

## Problema

Este projeto simula como o calor se espalha ao longo do tempo em uma chapa
representada por uma grade 2D de pontos. Cada ponto da grade guarda uma
temperatura. A cada instante de tempo, a temperatura de um ponto tende à
média das temperaturas dos seus vizinhos (cima, baixo, esquerda, direita).
Repetindo esse cálculo muitas vezes, obtemos a evolução do calor se
espalhando pela chapa, de forma parecida com o que aconteceria fisicamente
em uma placa de metal aquecida em um ponto.


## Estrutura do repositório

```
projeto/
├── src/            # código-fonte em C (MPI + OpenMP)
├── data/
│   ├── entrada/    # arquivos de configuração da simulação
│   └── saida/      # arquivos gerados pela simulação (por processo/passo)
├── scripts/        # scripts auxiliares (ex: juntar as saídas dos processos)
└── docs/           # relatório, resultados, análise de desempenho
```

Integrantes: João Vitor Fonseca Ceppo