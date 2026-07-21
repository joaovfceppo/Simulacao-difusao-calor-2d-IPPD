# Simulação de Difusão de Calor 2D com MPI + OpenMP

Trabalho da disciplina de Introdução ao Processamento Paralelo e Distribuído (IPPD).

## Problema

Este projeto simula como o calor se espalha ao longo do tempo em uma chapa
representada por uma grade 2D de pontos. Cada ponto da grade guarda uma
temperatura. Repetindo o cálculo abaixo muitas vezes, obtemos a evolução do
calor se espalhando pela chapa, de forma parecida com o que aconteceria
fisicamente em uma placa de metal aquecida em um ponto.

## Modelo físico

A simulação resolve a equação do calor, que descreve como a temperatura
varia no tempo em função de como ela está distribuída no espaço ao redor de
cada ponto. Para poder calcular isso em um computador, a equação é
discretizada (aproximada por diferenças entre pontos vizinhos), resultando
na seguinte fórmula aplicada a cada ponto da grade, a cada passo de tempo:

```
T_novo[i][j] = T[i][j] + alpha * dt * (
    (T[i+1][j] - 2*T[i][j] + T[i-1][j]) / dx^2 +
    (T[i][j+1] - 2*T[i][j] + T[i][j-1]) / dy^2
)
```

Onde:
- `T[i][j]` é a temperatura atual do ponto na linha `i`, coluna `j`.
- `alpha` é a difusividade térmica do material (o quão rápido o calor se
  espalha nele).
- `dx` e `dy` são as distâncias entre pontos vizinhos da grade.
- `dt` é o tamanho do passo de tempo simulado a cada iteração.

Esse método (diferenças finitas explícitas) só produz resultados
numericamente corretos se os parâmetros `alpha`, `dx`, `dy` e `dt`
respeitarem a condição de estabilidade:

```
alpha * dt * (1/dx^2 + 1/dy^2) <= 0.5
```

Se essa condição não for satisfeita, o erro de aproximação cresce a cada
passo em vez de diminuir, e a simulação diverge (os valores de temperatura
"explodem"). Por isso, o programa verifica essa condição antes de iniciar a
simulação e interrompe a execução com um aviso caso ela não seja atendida.

As bordas da grade têm temperatura fixa (condição de contorno de
Dirichlet), definida separadamente para cada lado (topo, base, esquerda,
direita). Também é possível definir regiões retangulares internas com
temperatura fixa, representando fontes de calor (ou de resfriamento, se a
temperatura for negativa).

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

## Como compilar e executar

Requer um compilador C com suporte a MPI e OpenMP.
Esse ambiente foi fornecido no projeto Xivoco e as execuções desse trabalho
serão feitas nele.

Compilar:
```bash
mpicc -fopenmp src/difusao_calor.c -o src/difusao_calor -lm
```

```bash
OMP_NUM_THREADS=2 mpirun -np 4 --host master,worker-1,worker-2,worker-3 ./src/difusao_calor data/entrada/entrada.txt
```


Integrantes: João Vitor Fonseca Ceppo