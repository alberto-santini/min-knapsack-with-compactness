import csv
import random
import string
import json
from typing import List
from os.path import realpath, dirname, join
from glob import glob


def generate_from_csv(csv_file: str) -> None:
    with open(csv_file, newline='') as f:
        reader = csv.reader(f)

        # Skip header
        next(reader)

        for row in reader:
            weights = [float(x) for x in row]
            
            for min_weight in [0.9, 0.95]:
                for max_distance in [2, 3, 5]:
                    generate_instance(
                        weights=weights,
                        min_weight=min_weight,
                        max_distance=max_distance
                    )


def generate_instance(weights: List[float], min_weight: float, max_distance: int) -> None:
    prefix = ''.join(
        random.choice(string.ascii_lowercase + string.digits)
        for _ in range(6)
    )

    n_items = len(weights)
    profits = [1.0] * n_items
    name = f"{prefix}_{n_items}_{min_weight:.2f}_{max_distance}.json"
    obj = dict(
        n_items=n_items,
        weights=weights,
        profits=profits,
        min_weight=min_weight,
        max_distance=max_distance
    )

    filename = join(
        dirname(realpath(__file__)),
        'cappello-instances',
        name
    )

    with open(filename, 'w') as f:
        json.dump(obj, f)


if __name__ == '__main__':
    raw_data_glob = join(
        dirname(realpath(__file__)),
        'cappello-raw-data',
        '*.csv'
    )

    for csv_file in glob(raw_data_glob):
        generate_from_csv(csv_file=csv_file)