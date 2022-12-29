#include "UnitProfitDP.h"

#include <string>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <cassert>
#include <iostream>
#include <chrono>

namespace kplink {
    const std::string UnitDPParams::csv_header =
        "algo_name";
    const std::string UnitDPSolution::csv_header =
        "n_selected_items,selected_items,profit,weight,time_elapsed";

    std::string UnitDPParams::to_csv() const {
        return algo_name;
    }

    std::string UnitDPSolution::to_csv() const {
        std::ostringstream oss;
        std::copy(selected_items.begin(), selected_items.end(),
            std::ostream_iterator<std::size_t>(oss, ","));

        return std::to_string(selected_items.size()) + "," +
               "\"[" + oss.str() + "]\"," +
               std::to_string(profit) + "," +
               std::to_string(weight) + "," +
               std::to_string(time_elapsed);
    }

    UnitDPSolution UnitDP::solve() {
        table.resize(p.n_items * (p.n_items + 1u) / 2 - 1u);
        predecessor.resize(p.n_items * (p.n_items + 1u) / 2 - 1u);

        using std::chrono::steady_clock;
        using std::chrono::duration_cast;
        using std::chrono::milliseconds;

        const auto start_time = steady_clock::now();

        for(auto i = 0u; i < p.n_items; ++i) {
            #ifdef DEBUG
                std::cout << "W(" << i << ",0) = " << p.weights[i] << "\n";
            #endif
            
            W(i, 0u) = p.weights[i];
        }

        for(auto l = 1u; l < p.n_items; ++l) {
            for(auto i = l; i < p.n_items; ++i) {
                const std::size_t start_idx =
                    (i >= p.max_distance) ?
                    i - p.max_distance :
                    0u;

                double maxW = std::numeric_limits<double>::lowest();
                std::size_t pred = 0u;

                #ifdef DEBUG
                    std::cout << "W(" << i << "," << l << ") = max { ";
                #endif

                for(auto j = start_idx; j <= i - 1u; ++j) {
                    const auto w = W(j, l - 1u);

                    assert(w);

                    #ifdef DEBUG
                        std::cout << *w << ((j < i - 1u) ? ", " : " ");
                    #endif

                    if(*w > maxW) {
                        maxW = *w;
                        pred = j;
                    }
                }

                #ifdef DEBUG
                    std::cout << "} + " << p.weights[i] << " = ";
                    std::cout << maxW << " + " << p.weights[i] << " = ";
                    std::cout << (maxW + p.weights[i]) << "\n";
                #endif

                assert(maxW < std::numeric_limits<double>::max());

                W(i, l) = maxW + p.weights[i];
                P(i, l) = pred;
            }
        }

        std::size_t min_sz = p.n_items;
        std::size_t min_i = p.n_items;
        double weight = 0.0;

        for(auto i = 0u; i < p.n_items; ++i) {
            for(auto l = 0u; l <= i; ++l) {
                assert(W(i, l));

                if(*W(i, l) >= p.min_weight && l < min_sz) {
                    min_sz = l;
                    min_i = i;
                    weight = *W(i, l);
                }
            }
        }

        std::vector<std::size_t> selected_items = {{ min_i }};
        std::size_t current_l = min_sz;
        std::size_t current_i = *P(min_i, current_l);

        while(current_l >= 1u) {
            selected_items.push_back(current_i);
            current_i = *P(current_i, --current_l);
        }

        const auto end_time = steady_clock::now();
        const auto time_elapsed = duration_cast<milliseconds>(end_time - start_time).count() / 1000.0;

        return UnitDPSolution{
            /* .selected_items = */ selected_items,
            /* .profit = */ (double) (min_sz + 1u),
            /* .weight = */ weight,
            /* .time_elapsed = */ time_elapsed
        };
    }
}

