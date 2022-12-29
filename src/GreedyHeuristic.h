#ifndef _GREEDY_HEURISTIC_H
#define _GREEDY_HEURISTIC_H

#include <cstddef>
#include <vector>
#include <string>
#include <optional>
#include "Problem.h"

namespace kplink {
    struct GreedyHeuristicParams {
        /** Algorithm name. */
        std::string algo_name;
        
        /** Header for csv files. */
        static const std::string csv_header;

        /** Export to comma-separated list. */
        [[nodiscard]] std::string to_csv() const;
    };

    struct GreedyHeuristicSolution {
        /** Selected items. */
        std::vector<std::size_t> selected_items;

        /** Profit collected. */
        double profit;

        /** Weight collected. */
        double weight;

        /** Time elapsed in seconds. */
        double time_elapsed;

        /** Header for csv files. */
        static const std::string csv_header;

        /** Export to comma-separated list. */
        [[nodiscard]] std::string to_csv() const;
    };

    struct GreedyHeuristic {
        /** Problem instance. */
        const Problem& p;

    private:
        /** Keeps track of which items are being packed. */
        std::vector<bool> packed;

        /** Keeps track of which items are available to be packed,
         *  i.e., they are not already packed and they are within
         *  p.max_distance from a packed item.
         */
        std::vector<bool> available;

        /** Packs item i and updates `packed` and `available`. */
        void pack(std::size_t i);

    public:

        /** Builds the greedy heuristic object. */
        explicit GreedyHeuristic(const Problem& p) :
            p{p}, packed(p.n_items, false), available(p.n_items, false)
        {
            if(!p.constant_profits) {
                throw std::invalid_argument("The GreedyHeuristic algorithm can only be used with constant-profit instances.");
            }
        }

        /** Executes the greedy algorithm. */
        [[nodiscard]] GreedyHeuristicSolution solve();
    };
}

#endif