#include "Problem.h"

#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <string>
#include <json.hpp>

namespace kplink {
    const std::string Problem::csv_header =
        "problem_name,problem_n_items,problem_max_distance,problem_min_weight,problem_constant_profits";

    std::string Problem::to_csv() const {
        return problem_name + "," +
               std::to_string(n_items) + "," +
               std::to_string(max_distance) + "," +
               std::to_string(min_weight) + "," +
               std::to_string(constant_profits);
    }

    Problem::Problem(std::filesystem::path problem_file) :
        problem_file{problem_file},
        problem_name{problem_file.stem()}
    {
        std::ifstream ifs{problem_file};

        if(ifs.fail()) {
            std::cerr << "Cannot read from problem file " << problem_file << "\n";
            std::exit(EXIT_FAILURE);
        }

        nlohmann::json obj;

        ifs >> obj;

        n_items = obj["n_items"];
        max_distance = obj["max_distance"];
        min_weight = obj["min_weight"];
        weights.resize(n_items);
        weights = obj["weights"].get<std::vector<double>>();
        profits.resize(n_items);
        profits = obj["profits"].get<std::vector<double>>();

        const auto first_profit = profits.at(0u);
        const auto equal = [] (double x, double y) { return std::abs(x - y) < 1e-12; };
        constant_profits = std::all_of(profits.begin(), profits.end(), [&] (double profit) { return equal(profit, first_profit); });
    }

    std::ostream& operator<<(std::ostream& out, const Problem& problem) {
        out << "Problem[ n_items = " << problem.n_items << ", "
            << "max_distance = " << problem.max_distance << ", "
            << "min_weight = " << problem.min_weight << ", "
            << "constant_profits = " << std::boolalpha << problem.constant_profits << " ]";
        return out;
    }
}