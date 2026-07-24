#!/bin/bash
WORKERS="worker-1 worker-2 worker-3"

for host in $WORKERS; do
    echo "Copiando para $host..."
    ssh "$host" "mkdir -p ~/projeto/src"
    scp src/difusao_calor "$host:~/projeto/src/difusao_calor"
    ssh "$host" "chmod +x ~/projeto/src/difusao_calor"
done

echo "Distribuicao concluida."