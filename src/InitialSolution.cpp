#include "InitialSolution.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <rapidcsv.h>

namespace kplink {
    std::vector<std::size_t> read_initial_solution(std::filesystem::path initial_solution_file) {
        const auto csvdoc = rapidcsv::Document(initial_solution_file.string());
        const auto cols = csvdoc.GetColumnNames();

        std::string initstr{};

        bool found = (std::find(cols.begin(), cols.end(), "selected_items") != cols.end()) &&
            !(initstr = csvdoc.GetCell<std::string>("selected_items", 0u)).empty();
            
        if(!found) {
            found = (std::find(cols.begin(), cols.end(), "primal_selected_items") != cols.end()) &&
                !(initstr = csvdoc.GetCell<std::string>("primal_selected_items", 0u)).empty();
        }

        if(!found) {
            throw std::runtime_error("No initial solution found in file " + initial_solution_file.string());
        }

        if(initstr.at(0u) != '[' || initstr.at(initstr.size() - 1u) != ']') {
            throw std::runtime_error("Wrong format for initial solution \"" + initstr + "\". It should start with [ and end with ].");
        }

        if(initstr.size() < 4u || initstr.at(initstr.size() - 2u) != ',') {
            throw std::runtime_error("Wrong format for initial solution \"" + initstr + "\". Too short or does not have a trailing comma.");
        }

        initstr = initstr.substr(1u, initstr.size() - 2u);

        std::vector<std::size_t> items;
        std::size_t str_pos;
        std::string token;

        while((str_pos = initstr.find(',')) != std::string::npos) {
            token = initstr.substr(0u, str_pos);
            if(token.empty()) { continue; }

            try {
                items.push_back(std::stoul(token));
            } catch(const std::invalid_argument& exc) {
                std::cerr << "Wrong item in initial solution: " << token << "\n";
                throw;
            }

            initstr.erase(0u, str_pos + 1u);
        }

        return items;
    }
}