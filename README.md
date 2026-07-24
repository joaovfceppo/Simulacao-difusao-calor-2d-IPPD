# Simulação de Difusão de Calor 2D com MPI + OpenMP

Integrantes: João Vitor Fonseca Ceppo e Henrique Colares Versiani

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

## Disco não-compartilhado entre os nós MPI

O ambiente de execução (Xivoco) fornece múltiplos nós de cluster que não
compartilham disco entre si. O programa contorna essa limitação nas duas
pontas de dados:

- **Entrada**: apenas o processo de rank 0 lê o arquivo de configuração do
  seu próprio disco; os demais processos recebem essa configuração via
  `MPI_Bcast`, sem precisar que o arquivo exista em todos os nós.
- **Saída**: a cada intervalo de passos configurado, todos os processos
  enviam sua fatia calculada para o rank 0 via `MPI_Gather`, que monta a
  grade completa em memória e escreve um único arquivo de saída.

Na prática, isso significa que o arquivo de entrada e a pasta de saída só
precisam existir no nó onde o rank 0 é executado (tipicamente o `master`).

Isso não cobre o executável em si: o MPI não transmite o binário pela
rede, então cada nó precisa ter sua própria cópia do programa compilado
antes de rodar. O script `scripts/distribui_binario.sh` automatiza essa
cópia para os demais nós via `scp`.

## Estrutura do repositório

```
projeto/
├── src/            # código-fonte em C (MPI + OpenMP)
├── data/
│   ├── entrada/    # arquivos de configuração da simulação
│   │   ├── entrada.txt   # grade pequena (100x100), usada na análise de desempenho
│   │   └── entrada2.txt  # grade grande (1000x1000), usada na análise de desempenho
│   └── saida/      # arquivos gerados pela simulação (grade completa, por passo salvo)
├── scripts/
│   ├── distribui_binario.sh  # copia o executável compilado para os demais nós do cluster
│   ├── visualiza.py           # gera mapas de calor (PNG) e um GIF a partir da saída
│   └── requirements.txt       # dependências Python do script de visualização
└── docs/
    ├── analise_desempenho.md  # comparação sequencial x paralelo, com discussão
    ├── imagens/                # prints das execuções usadas na análise de desempenho
    └── Gif_passos/             # PNGs por passo e animação gerados por visualiza.py
```

## Como compilar e executar

Requer um compilador C com suporte a MPI e OpenMP.
Esse ambiente foi fornecido no projeto Xivoco e as execuções desse trabalho
serão feitas nele.

**1. Compilar:**
```bash
mpicc -fopenmp src/difusao_calor.c -o src/difusao_calor -lm
```

**2. Distribuir o binário para os demais nós do cluster:**
```bash
chmod +x scripts/distribui_binario.sh
./scripts/distribui_binario.sh
```

**3. Executar:**
```bash
OMP_NUM_THREADS=2 mpirun -np 4 --host master,worker-1,worker-2,worker-3 \
    ./src/difusao_calor data/entrada/entrada2.txt data/saida
```

> O exemplo usa a grade grande (`entrada2.txt`), onde a divisão do trabalho
> entre os nós compensa o custo de comunicação. Na grade pequena
> (`entrada.txt`), rodar com 4 processos chega a ser mais lento que a execução
> sequencial — o cálculo por processo é pequeno demais para pagar o overhead
> de troca de mensagens. Os detalhes dessa comparação (e de qual configuração
> escolher para cada tamanho de grade) estão em
> [`docs/analise_desempenho.md`](docs/analise_desempenho.md).

## Visualização

O script `scripts/visualiza.py` lê os arquivos `saida_passo*.txt` gerados
pela simulação e produz um mapa de calor (PNG) para cada passo salvo, além
de uma animação (GIF) mostrando a evolução da temperatura ao longo do
tempo. Ele roda no computador local (não no Xivoco, que não tem interface
gráfica), a partir de uma cópia da pasta `data/saida`.

Instalar as dependências (uma vez):
```bash
pip install -r scripts/requirements.txt
```

Rodar (usa `data/saida` como entrada e `docs/Gif_passos` como saída por
padrão):
```bash
python scripts/visualiza.py
```

Outras opções disponíveis: `--entrada`, `--saida`, `--cmap`, `--fps`,
`--vmin`/`--vmax` (para fixar manualmente a escala de temperatura) e
`--sem-gif` (gera só os PNGs). Use `python scripts/visualiza.py --help`
para a lista completa.

## Análise de desempenho

Testes comparando execução sequencial e paralela (variando número de
processos MPI e threads OpenMP, em duas grades de tamanhos diferentes)
estão documentados em [`docs/analise_desempenho.md`](docs/analise_desempenho.md),
com os prints de cada execução em `docs/imagens/`.