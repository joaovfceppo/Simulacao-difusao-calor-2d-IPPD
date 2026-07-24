#!/usr/bin/env python3

import argparse
import re
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")

import matplotlib.pyplot as plt
import numpy as np
from PIL import Image

PADRAO_ARQUIVO = "saida_passo*.txt"
PADRAO_PASSO = re.compile(r"saida_passo(\d+)\.txt$")


def coleta_arquivos(dir_entrada: Path):
    arquivos = []
    for caminho in dir_entrada.glob(PADRAO_ARQUIVO):
        casamento = PADRAO_PASSO.search(caminho.name)
        if casamento is None:
            continue
        arquivos.append((int(casamento.group(1)), caminho))
    arquivos.sort(key=lambda par: par[0])
    return arquivos


def le_grade(caminho: Path) -> np.ndarray:
    grade = np.loadtxt(caminho, comments="#", dtype=float)
    if grade.ndim == 1:  # grade de uma unica linha
        grade = grade.reshape(1, -1)
    return grade


def faixa_global(arquivos, vmin_forcado, vmax_forcado):
    if vmin_forcado is not None and vmax_forcado is not None:
        return vmin_forcado, vmax_forcado

    minimo, maximo = np.inf, -np.inf
    for _, caminho in arquivos:
        grade = le_grade(caminho)
        minimo = min(minimo, float(grade.min()))
        maximo = max(maximo, float(grade.max()))

    vmin = vmin_forcado if vmin_forcado is not None else minimo
    vmax = vmax_forcado if vmax_forcado is not None else maximo

    if vmin == vmax:
        vmax = vmin + 1.0
    return vmin, vmax


def desenha_frame(grade, passo, vmin, vmax, cmap, dpi, caminho_png):
    altura, largura = grade.shape
    proporcao = largura / altura

    fig, ax = plt.subplots(figsize=(6 * proporcao, 6), dpi=dpi)

    imagem = ax.imshow(grade, cmap=cmap, vmin=vmin, vmax=vmax,
                       origin="upper", interpolation="nearest")

    ax.set_title(f"Difusao de calor - passo {passo}")
    ax.set_xlabel("coluna (j)")
    ax.set_ylabel("linha (i)")

    barra = fig.colorbar(imagem, ax=ax, fraction=0.046, pad=0.04)
    barra.set_label("Temperatura")

    fig.tight_layout()
    fig.savefig(caminho_png)
    plt.close(fig)


def monta_gif(caminhos_png, caminho_gif, fps):
    """Junta os PNGs num GIF animado usando o Pillow."""
    if len(caminhos_png) < 2:
        print("Aviso: menos de 2 imagens, GIF nao gerado.", file=sys.stderr)
        return False

    duracao_ms = int(1000 / fps)
    quadros = [Image.open(p).convert("P", palette=Image.ADAPTIVE)
               for p in caminhos_png]
    quadros[0].save(
        caminho_gif,
        save_all=True,
        append_images=quadros[1:],
        duration=duracao_ms,
        loop=0,
    )
    for quadro in quadros:
        quadro.close()
    return True


def main():
    parser = argparse.ArgumentParser(
        description="Gera mapas de calor (PNG) e um GIF a partir das saidas da simulacao.")
    parser.add_argument("--entrada", default="data/saida", type=Path,
                        help="pasta com os arquivos saida_passo*.txt (padrao: data/saida)")
    parser.add_argument("--saida", default="docs/Gif_passos", type=Path,
                        help="pasta onde salvar as imagens (padrao: docs/Gif_passos)")
    parser.add_argument("--cmap", default="inferno",
                        help="mapa de cores do matplotlib (padrao: inferno)")
    parser.add_argument("--dpi", default=110, type=int,
                        help="resolucao das imagens (padrao: 110)")
    parser.add_argument("--fps", default=4.0, type=float,
                        help="quadros por segundo do GIF (padrao: 4)")
    parser.add_argument("--gif", default="animacao.gif",
                        help="nome do GIF gerado dentro da pasta de saida")
    parser.add_argument("--sem-gif", action="store_true",
                        help="gera apenas os PNGs, sem montar o GIF")
    parser.add_argument("--vmin", type=float, default=None,
                        help="forca a temperatura minima da escala de cores")
    parser.add_argument("--vmax", type=float, default=None,
                        help="forca a temperatura maxima da escala de cores")
    args = parser.parse_args()

    if not args.entrada.is_dir():
        print(f"Erro: pasta de entrada '{args.entrada}' nao encontrada.", file=sys.stderr)
        return 1

    arquivos = coleta_arquivos(args.entrada)
    if not arquivos:
        print(f"Erro: nenhum arquivo '{PADRAO_ARQUIVO}' em '{args.entrada}'. "
              "Rode a simulacao antes (ou copie data/saida do no master).",
              file=sys.stderr)
        return 1

    args.saida.mkdir(parents=True, exist_ok=True)

    print(f"{len(arquivos)} passos encontrados em '{args.entrada}'.")
    vmin, vmax = faixa_global(arquivos, args.vmin, args.vmax)
    print(f"Escala de cores fixa em [{vmin:.2f}, {vmax:.2f}] para todos os passos.")

    caminhos_png = []
    for passo, caminho in arquivos:
        grade = le_grade(caminho)
        caminho_png = args.saida / f"passo{passo:06d}.png"
        desenha_frame(grade, passo, vmin, vmax, args.cmap, args.dpi, caminho_png)
        caminhos_png.append(caminho_png)
        print(f"  passo {passo:>6}  grade {grade.shape[0]}x{grade.shape[1]}  -> {caminho_png}")

    if not args.sem_gif:
        caminho_gif = args.saida / args.gif
        if monta_gif(caminhos_png, caminho_gif, args.fps):
            print(f"GIF gerado: {caminho_gif}")

    return 0


if __name__ == "__main__":
    sys.exit(main())