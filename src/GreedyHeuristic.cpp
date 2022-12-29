#include "GreedyHeuristic.h"

#include <string>
#include <ostream>
#include <iterator>
#include <cstddef>
#include <vector>
#include <chrono>
#include <optional>
#include <algorithm>
#include <cassert>

namespace kplink {
    const std::string GreedyHeuristicParams::csv_header = "algo_name";
    const std::string GreedyHeuristicSolution::csv_header =
        "n_selected_items,selected_items,profit,weight,time_elapsed";

    std::string GreedyHeuristicParams::to_csv() const {
        return algo_name;
    }

    std::string GreedyHeuristicSolution::to_csv() const {
        std::ostringstream oss;
        std::copy(selected_items.begin(), selected_items.end(),
            std::ostream_iterator<std::size_t>(oss, ","));

        return std::to_string(selected_items.size()) + "," +
               "\"[" + oss.str() + "]\"," +
               std::to_string(profit) + ", " +
               std::to_string(weight) + "," +
               std::to_string(time_elapsed);
    }

    void GreedyHeuristic::pack(std::size_t i) {
        assert(i < p.n_items);
        assert(!packed[i]);

        packed[i] = true;
        available[i] = false;

        const auto start_idx = i > p.max_distance ?
            i - p.max_distance : 0u;
        const auto end_idx = i + p.max_distance < p.n_items ?
            i + p.max_distance : p.n_items - 1u;

        for(auto j = start_idx; j <= end_idx; ++j) {
            if(!packed[j]) {
                available[j] = true;
            }
        }
    }

    GreedyHeuristicSolution GreedyHeuristic::solve() {
        using std::chrono::steady_clock, std::chrono::duration_cast, std::chrono::milliseconds;
        
        const auto start_time = steady_clock::now();
        const auto first_item_it = std::max_element(p.weights.begin(), p.weights.end());
        
        auto first_item = std::distance(p.weights.begin(), first_item_it);
        auto current_weight = *first_item_it;
        auto n_packed_items = 1u;
        pack(first_item);

        while(current_weight < p.min_weight) {
            std::optional<std::size_t> next_item = std::nullopt;
            std::optional<double> next_weight = std::nullopt;

            for(auto j = 0u; j < p.n_items; ++j) {
                if(!available[j]) {
                    continue;
                }

                if((!next_item) || (p.weights[j] > *next_weight)) {
                    next_item = j;
                    next_weight = p.weights[j];
                }
            }

            assert(next_item);
            assert(next_weight);

            pack(*next_item);
            current_weight += *next_weight;
            ++n_packed_items;
        }

        const auto end_time = steady_clock::now();
        auto packed_items = std::vector<std::size_t>{};
        packed_items.reserve(n_packed_items);

        for(auto j = 0u; j < p.n_items; ++j) {
            if(packed[j]) {
                packed_items.push_back(j);
            }
        }

        return GreedyHeuristicSolution{
            /* .selected_items = */ packed_items,
            /* .profit = */ p.profits.at(0u) * n_packed_items, // Constant profits allow this
            /* .weight = */ current_weight,
            /* .time_elapsed = */ duration_cast<milliseconds>(end_time - start_time).count() / 1000.0
        };
    }
}