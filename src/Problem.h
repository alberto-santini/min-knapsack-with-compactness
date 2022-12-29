#ifndef _PROBLEM_H
#define _PROBLEM_H

#include <cstddef>
#include <vector>
#include <filesystem>
#include <string>
#include <iostream>

namespace kplink {
    struct Problem {
        /** Path to the problem file. */
        std::filesystem::path problem_file;

        /** Problem name for reporting uses. */
        std::string problem_name;

        /** Number of items in the instance. */
        std::size_t n_items;
        
        /**
         * Maximum distance between two items, before they
         * are considered disconnected.
         * 
         * Two items i, j with |i-j| <= max_distance are
         * connected. If |i-j| > max_distance, they are
         * disconnected.
         */
        std::size_t max_distance;

        /** Minimum weight to collect. */
        double min_weight;

        /** Weights of the items. Length: n_items. */
        std::vector<double> weights;

        /** Profits of the items. Length: n_items. */
        std::vector<double> profits;

        /** True if all profits are constant. */
        bool constant_profits;

        /** Header for csv files. */
        static const std::string csv_header;

        /** Export to comma-separated list. */
        std::string to_csv() const;

        /** Read problem from json file. */
        explicit Problem(std::filesystem::path problem_file);
    };

    std::ostream& operator<<(std::ostream& out, const Problem& problem);
}

#endif