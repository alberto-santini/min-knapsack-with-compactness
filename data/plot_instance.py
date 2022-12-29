import matplotlib.pyplot as plt
import json
import sys
from matplotlib.figure import Figure
from typing import Tuple, List, Optional
from os.path import basename, splitext, dirname, realpath, join, isdir
from glob import glob


def _get_instance_plot(weights: List[float], instance: str) -> Tuple[Figure, plt.Axes]:
    fig, ax = plt.subplots(figsize=(12,6))
    ax.bar(x=range(len(weights)), height=weights, color='C0', width=1, linewidth=0)
    ax.set_xlabel('Item num', fontsize=14)
    ax.set_ylabel('Weight', fontsize=14)
    ax.set_title(f"Instance {instance}")
    return fig, ax

def _add_solution(ax: plt.Axes, weights: List[float], items: List[int]) -> plt.Axes:
    heights = [p if i in items else 0 for i, p in enumerate(weights)]
    ax.bar(x=range(len(weights)), height=heights, color='C1', width=1, linewidth=0, label='Selected items')
    ax.legend()
    return ax

def plot_instance(instance: str, solution: Optional[str]) -> str:
    inst_dir = dirname(realpath(instance))
    inst_name = splitext(basename(realpath(instance)))[0]
    
    with open(instance) as f:
        obj = json.load(f)

    weights = obj['weights']

    fig, ax = _get_instance_plot(weights=weights, instance=inst_name)

    if solution is not None:
        items = list(map(int, solution.split(',')))
        ax = _add_solution(ax=ax, weights=weights, items=items)

    fig_name = join(inst_dir, f"{inst_name}.pdf")

    fig.savefig(fig_name)

    return fig_name


if __name__ == '__main__':
    path = sys.argv[1]

    if len(sys.argv) == 2:
        if isdir(path):
            plot_instance(instance=path)
        else:
            for instance in glob(join(path, '*.json')):
                plot_instance(instance=instance)
    elif len(sys.argv) == 3:
        plot_instance(instance=path, solution=sys.argv[2])