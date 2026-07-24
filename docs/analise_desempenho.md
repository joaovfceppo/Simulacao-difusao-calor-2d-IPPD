# Análise de Desempenho

## Metodologia

Os testes foram feitos em duas grades de tamanhos diferentes, para observar
como o tamanho do problema afeta o ganho de paralelismo:

- **Grade pequena**: `data/entrada/entrada.txt` (100x100 pontos)
- **Grade grande**: `data/entrada/entrada2.txt` (1000x1000 pontos)

Em ambos os casos, o número de passos de tempo (2000) e os demais
parâmetros físicos foram mantidos fixos, variando apenas o número de
processos MPI e/ou threads OpenMP. Isso garante que as diferenças de tempo
medidas reflitam só o paralelismo, não mudanças no problema em si.

## Resultados — Grade 100x100

| Configuração | Tempo (s) | Speedup vs sequencial |
|---|---|---|
| 1 processo (sequencial) | 0.7024 | 1.00x |
| 2 processos | 0.5374 | 1.31x |
| 4 processos | 2.5844 | 0.27x |

1 processo, grade 100x100<img width="1859" height="413" alt="Image" src="https://github.com/user-attachments/assets/b4550707-1a2c-476c-874f-32735efccd36" />
2 processos, grade 100x100<img width="1870" height="443" alt="Image" src="https://github.com/user-attachments/assets/37df27b7-1892-4120-9f7f-3ed109c85054" />
4 processos, grade 100x100<img width="1876" height="478" alt="Image" src="https://github.com/user-attachments/assets/3a80ed32-4e03-4cc3-b270-fccef1a332e1" />

## Resultados — Grade 1000x1000

| Configuração | Tempo (s) | Speedup vs sequencial |
|---|---|---|
| 1 processo, 1 thread (sequencial) | 64.5775 | 1.00x |
| 2 processos, 2 threads | 35.3493 | 1.83x |
| 4 processos, 1 thread | 44.9373 | 1.44x |
| 4 processos, 2 threads | 44.8011 | 1.44x |
| 4 processos, 4 threads | 44.8564 | 1.44x |

1 processo, grade 1000x1000<img width="1872" height="426" alt="Image" src="https://github.com/user-attachments/assets/b05ee1a8-cde0-46fd-9d65-9e9ad13318c4" />
2 processos, grade 1000x1000<img width="1872" height="425" alt="Image" src="https://github.com/user-attachments/assets/b849ac8b-f608-4099-a99e-e558cafb42ab" />
4 processos 2 threads, grade 1000x1000<img width="1859" height="483" alt="Image" src="https://github.com/user-attachments/assets/0795359a-249d-4dca-b33a-4d89d3bf6a34" />
4 processos 1 thread, grade 1000x1000<img width="1869" height="471" alt="Image" src="https://github.com/user-attachments/assets/493c1ae0-d401-4785-a99f-c721c6c9fb96" />
4 processos 4 threads, grade 1000x1000<img width="1866" height="489" alt="Image" src="https://github.com/user-attachments/assets/d4952ca0-e241-4a9c-8b27-469d3a303c34" />

## Discussão

### Grade pequena: paralelismo pode até atrapalhar

Na grade 100x100, usar 4 processos foi **mais lento** que rodar
sequencialmente (2.58s vs 0.70s). O trabalho de cálculo por processo é
pequeno demais para compensar o custo fixo de comunicação entre os nós MPI
(troca de bordas a cada passo, junção da grade via `MPI_Gather` a cada
intervalo de salvamento). Com apenas 2 processos, o ganho ainda aparece
(1.31x), mas se perde completamente ao dobrar para 4.

### Grade grande: mais processos não significaram mais velocidade

Na grade 1000x1000, o melhor resultado (1.83x) foi obtido com 2 processos e
2 threads. Comparando com a configuração de 4 processos e **2 threads** — ou
seja, mantendo a quantidade de threads fixa e variando só o número de
processos MPI —, o tempo piora de 35.35s para 44.80s. Como as threads são
as mesmas nas duas configurações, essa diferença é atribuível ao número de
processos, não ao OpenMP: dobrar os processos MPI, aqui, deixou a simulação
mais lenta, mesmo com um problema grande o suficiente para que o cálculo por
processo seja substancial.

A explicação mais provável é o custo de comunicação de rede entre nós
físicos, que cresce com o número de vizinhos: com 2 processos há apenas 1
par trocando bordas por passo, enquanto com 4 processos há 3 pares, cada um
sujeito à latência de rede entre máquinas virtuais diferentes do Xivoco.
Vale registrar, porém, que esta é a hipótese mais plausível diante dos
dados, e não uma medição direta — nesta bateria não separamos o tempo gasto
em cálculo do tempo gasto em comunicação.

### Threads OpenMP: sem ganho observável nesta bateria

Mantendo 4 processos MPI fixos e variando as threads OpenMP (1, 2 e 4), o
tempo ficou praticamente constante (44.94s, 44.80s e 44.86s,
respectivamente — menos de 0.2s de diferença entre os extremos). Ou seja,
*com 4 processos*, adicionar threads não teve impacto perceptível.

É importante ser honesto sobre o que esses números permitem concluir. Eles
mostram que o OpenMP não ajudou nessa configuração específica, mas **não
isolam** a contribuição do OpenMP, porque não há execução com 1 processo e
várias threads para comparar. Três explicações são compatíveis com o
observado, e apenas com esta tabela não dá para decidir entre elas:

1. o tempo total já está dominado pela comunicação MPI, e o cálculo — parte
   onde o OpenMP atua — é uma fração pequena do total;
2. *oversubscription*: se cada VM do Xivoco expõe poucos núcleos, 2–4 threads
   passam a competir pelo mesmo core e não escalam;
3. o laço do stencil pode ser mais limitado por acesso à memória e pelo
   overhead por célula (as checagens `obtem_temp_borda` e `ponto_em_fonte`
   rodam para cada ponto, a cada passo) do que por capacidade de cálculo.

Para separar essas hipóteses seria necessária uma bateria com **1 processo e
1/2/4 threads**, isolando o efeito do OpenMP sem a comunicação MPI no meio.

### Speedup total: sequencial vs paralelo

Comparando a versão puramente sequencial (1 processo, 1 thread: 64.58s) com
a configuração paralela padrão do trabalho (4 processos, 2 threads: 44.80s),
obtivemos um speedup de aproximadamente **1.44x**.

Para interpretar esse número é preciso escolher um teto de referência
coerente com o que foi observado. O teto *nominal* de 4 processos × 2 threads
seria 8x, mas como as threads não trouxeram ganho mensurável neste ambiente
(seção anterior), usar 8x como referência contradiz os próprios dados. O teto
realmente relevante aqui é o dos 4 processos: **4x**. Mesmo contra esse teto
mais modesto, 1.44x fica bem abaixo do ideal — o que é esperado e consistente
com a discussão acima: o overhead de comunicação entre nós MPI consome parte
significativa do ganho teórico da decomposição espacial, especialmente em um
cluster virtualizado como o Xivoco, onde a latência de rede entre nós é maior
do que em um cluster HPC dedicado.

### Conclusão geral

Os resultados mostram que paralelismo não é sempre sinônimo de mais
velocidade: seu benefício depende do tamanho do problema (a grade precisa
ser grande o suficiente para que o cálculo por processo compense o custo
de comunicação) e do número de nós físicos envolvidos (mais processos
significam mais pares de comunicação, cujo custo pode superar o ganho de
dividir o trabalho). Esse comportamento está relacionado à Lei de Amdahl,
mas evidencia também o papel da latência de comunicação entre nós físicos
distintos, que a formulação clássica da lei não modela diretamente.

Cabe uma ressalva metodológica sobre o alcance dessas conclusões. As
baterias atuais variam mais de um fator ao mesmo tempo (processos e threads)
em algumas linhas e não incluem uma execução com 1 processo e várias threads,
o que impede isolar a contribuição do OpenMP de forma limpa. Um teste de
*strong scaling* variando **um fator por vez** fecharia essas lacunas: (a)
fixar 1 thread e variar os processos (1, 2, 4) para isolar o MPI; e (b) fixar
1 processo e variar as threads (1, 2, 4) para isolar o OpenMP. Com esses dois
eixos separados, as afirmações sobre "2 processos superam 4" e sobre a
ausência de ganho com threads passariam de argumentação plausível para
conclusão sustentada por medição direta.