#ifndef _INITIAL_SOLUTION_H
#define _INITIAL_SOLUTION_H

#include <vector>
#include <filesystem>

namespace kplink {
    std::vector<std::size_t> read_initial_solution(std::filesystem::path initial_solution_file);
}

#endif