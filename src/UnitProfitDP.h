#ifndef _UNIT_PROFIT_H
#define _UNIT_PROFIT_H

#include "Problem.h"
#include <string>
#include <vector>
#include <algorithm>
#include <optional>

namespace kplink {
    struct UnitDPParams {
        /** Algorithm name. */
        std::string algo_name;

        /** Header for csv files. */
        static const std::string csv_header;

        /** Export to comma-separated list. */
        [[nodiscard]] std::string to_csv() const;
    };

    struct UnitDPSolution {
        /** Selected items. */
        std::vector<std::size_t> selected_items;

        /** Profit collected (== number of items). */
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

    struct UnitDP {
        /** Problem instance. */
        const Problem& p;

        /** Labelling algorithm parameters. */
        const UnitDPParams params;

        /** Builds the algorithm object from the problem instance. */
        UnitDP(const Problem& p, const UnitDPParams params) : p{p}, params{params} {
            if(std::any_of(p.profits.begin(), p.profits.end(), [&] (auto pr) { return pr != 1.0; })) {
                throw std::logic_error("Trying to use the Unit-Profit DP on an instance which does not have unit profits.");
            }
        }

        /** Executes the labelling algorithm. */
        [[nodiscard]] UnitDPSolution solve();

    private:
        /** Data structure used to store the Dynamic Programming table.
         * 
         *  This is a lower-triangular square matrix indexed with (i,l)
         *  for i = 0, ..., p.n_items-1 and l = 0, ..., i-1.
         *  Entry (i, l) is the highest weight achievable with a subset
         *  of items {0, ..., i} of size l+1 and such that its
         *  highest-index element has index i.
         * 
         *  The matrix is stored flat and auxiliary function W must be
         *  used to access its elements.
         */
        using DPTable = std::vector<std::optional<double>>;

        /** Similar to DPTable, but used to reconstruct the optimal solution.
         * 
         *  Entry (i, l) stores the index of the item which achieves the
         *  maximum in the DP recursion for W(i, l).
         */
        using DPPred = std::vector<std::optional<std::size_t>>;

        /** Dynamic Programming table of weights. */
        DPTable table;

        /** Dynamic Programming table of predecessors. */
        DPPred predecessor;

        /** Access an element of the Dynamic Programming weights table. */
        [[nodiscard]] std::optional<double>& W(std::size_t i, std::size_t l) {
            return table[(i + 1) * i / 2 + l];
        }

        /** Access an element of the Dynamic Programming predecessors table. */
        [[nodiscard]] std::optional<std::size_t>& P(std::size_t i, std::size_t l) {
            return predecessor[(i + 1) * i / 2 + l];
        }
    };
}

#endif