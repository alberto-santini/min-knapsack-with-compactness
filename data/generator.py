from enum import Enum
from dataclasses import dataclass
from numpy.typing import ArrayLike
from os.path import realpath, dirname, join
from scipy.stats import truncnorm
from itertools import product
from typing import Optional
import numpy as np
import random
import string
import json


class WeightsType(Enum):
    NOISE = 1
    ONE_PEAK = 2
    TWO_PEAKS = 3


class ProfitsType(Enum):
    CONSTANT = 1
    RANDOM = 2
    FEW = 3


@dataclass
class Generator:
    weights_type: WeightsType
    profits_type: ProfitsType
    n_items: int
    min_weight: float
    max_distance: int
    peak_strength: float

    EPS = 1e-12

    def generate(self, override_name: Optional[str] = None) -> str:
        if self.weights_type == WeightsType.NOISE:
            weights = self._generate_noise_weights()
        elif self.weights_type == WeightsType.ONE_PEAK:
            weights = self._generate_one_peak_weights()
        elif self.weights_type == WeightsType.TWO_PEAKS:
            weights = self._generate_two_peaks_weights()
        else:
            raise NotImplementedError('Weights type not yet implemented')

        if self.profits_type == ProfitsType.CONSTANT:
            profits = self._generate_constant_profits()
        elif self.profits_type == ProfitsType.RANDOM:
            profits = self._generate_random_profits()
        elif self.profits_type == ProfitsType.FEW:
            profits = self._generate_few_profits(weights=weights)
        else:
            raise NotImplementedError('Profits type not yet implemented')

        return self._output_instance(weights=weights, profits=profits, override_name=override_name)

    def _generate_noise_weights(self) -> ArrayLike:
        base_weight = 1.0 / self.n_items
        weights = np.full(self.n_items, base_weight)
        weights += np.random.normal(loc=0.0, scale=base_weight/4.0, size=self.n_items)
        weights = weights.clip(min=self.EPS)
        weights /= weights.sum()

        return weights

    def _generate_one_peak_weights(self) -> ArrayLike:
        if self.peak_strength is None:
            raise ValueError(f"I need peak_strength to generate one-peak instances")

        # Random location of the peak
        loc = np.random.normal(loc=self.n_items/2, scale=self.n_items/4)
        loc = min(loc, self.n_items)
        loc = max(0, loc)

        # Truncated normal distribution which gives the shape to the weights
        low, high = 0, self.n_items
        scale = self.n_items / self.peak_strength
        a, b = (low - loc) / scale, (high - loc) / scale
        pdist = truncnorm.rvs(a=a, b=b, loc=loc, scale=scale, size=10000)

        # Obtain the weights from the cdf of the truncated normal
        weights = np.histogram(pdist, bins=range(self.n_items))[0]
        weights = self._check_weights_rounding_error(weights)
        weights = weights.clip(min=self.EPS)
        weights /= weights.sum()

        return weights

    def _generate_two_peaks_weights(self) -> ArrayLike:
        if self.peak_strength is None:
            raise ValueError(f"I need peak_strength to generate one-peak instances")

        # Random location of the two peaks
        def get_peak(base: float) -> float:
            loc = np.random.normal(loc=base, scale=self.n_items/6)
            loc = min(loc, self.n_items)
            loc = max(0, loc)
            return loc
        loc1, loc2 = get_peak(self.n_items / 3), get_peak(2 * self.n_items / 3)

        # Truncated normal distributions which give the shape to the weights
        low, high = 0, self.n_items
        scale = self.n_items / (self.peak_strength * 2)
        a1, b1 = (low - loc1) / scale, (high - loc1) / scale
        a2, b2 = (low - loc2) / scale, (high - loc2) / scale
        pdist = np.append(
            truncnorm.rvs(a=a1, b=b1, loc=loc1, scale=scale, size=5000),
            truncnorm.rvs(a=a2, b=b2, loc=loc2, scale=scale, size=5000)
        )

        # Obtain the weights from the cdf of the truncated normal
        weights = np.histogram(pdist, bins=range(self.n_items))[0]
        weights = self._check_weights_rounding_error(weights)
        weights = weights.clip(min=self.EPS)
        weights /= weights.sum()

        return weights

    def _generate_constant_profits(self) -> ArrayLike:
        return np.ones(self.n_items)

    def _generate_random_profits(self) -> ArrayLike:
        return np.random.uniform(low=1, high=10, size=self.n_items)

    def _generate_few_profits(self, weights: ArrayLike) -> ArrayLike:
        def roulette_wheel(ps: ArrayLike) -> int:
            current_sum, total_sum = 0, ps.sum()

            for idx, val in enumerate(ps):
                current_sum += val

                if np.random.uniform(0.0, total_sum) < current_sum:
                    return idx

            return len(ps)

        n_profits = int(self.n_items / 100)
        roulette_weights = weights.copy()
        profit_indices = list()

        while len(profit_indices) < n_profits:
            index = roulette_wheel(roulette_weights)
            roulette_weights = np.delete(roulette_weights, index)
            profit_indices.append(index)

        profits = np.ones(shape=self.n_items)
        profits[profit_indices] = 0.1

        return profits

    def _check_weights_rounding_error(self, weights: ArrayLike) -> ArrayLike:
        # Rounding errors:
        if len(weights) == self.n_items - 1:
            weights = np.append(weights, 0.0)

        if len(weights) != self.n_items:
            raise ValueError(f"Wrong provits length {len(weights)} vs. expected {self.n_items}")
    
        return weights

    def _output_instance(self, weights: ArrayLike, profits: ArrayLike, override_name: Optional[str]) -> str:
        assert len(weights) == self.n_items

        postfix = ''.join(
            random.choice(string.ascii_lowercase + string.digits)
            for _ in range(6)
        )

        ps = self.peak_strength or 0
        name = f"gen_{self.weights_type.name.lower()}_" +\
               f"{self.profits_type.name.lower()}_{self.n_items}_" +\
               f"{self.min_weight:.2f}_{self.max_distance}_" +\
               f"{ps}_{postfix}.json"

        obj = dict(
            n_items=self.n_items,
            weights=weights.tolist(),
            profits=profits.tolist(),
            min_weight=self.min_weight,
            max_distance=self.max_distance
        )

        filename = join(
            dirname(realpath(__file__)),
            'synthetic-instances',
            name            
        )

        if override_name is not None:
            filename = override_name

        with open(filename, 'w') as f:
            json.dump(obj, f)

        return filename


if __name__ == '__main__':
    weights_type = [WeightsType.NOISE, WeightsType.ONE_PEAK, WeightsType.TWO_PEAKS]
    profits_type = [ProfitsType.CONSTANT, ProfitsType.RANDOM, ProfitsType.FEW]
    n_items = [200, 400, 600]
    min_weight = [0.95]
    max_distance = [2, 3, 5]
    peak_strength = [8, 16, 32]
    
    for wt, pt, ni, mw, md, ps in product(weights_type, profits_type, n_items, min_weight, max_distance, peak_strength):
        # For noise, there is no peak_strength (only use one value)
        if wt == WeightsType.NOISE and ps == min(peak_strength):
            Generator(weights_type=wt, profits_type=pt, n_items=ni, min_weight=mw, max_distance=md, peak_strength=None).generate()
        elif wt == WeightsType.NOISE and ps != min(peak_strength):
            continue
        else:
            Generator(weights_type=wt, profits_type=pt, n_items=ni, min_weight=mw, max_distance=md, peak_strength=ps).generate()