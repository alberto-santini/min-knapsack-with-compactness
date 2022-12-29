#include "Problem.h"
#include "Labelling.h"
#include "CompactModel.h"
#include "BranchAndCut.h"
#include "GreedyHeuristic.h"
#include "InitialSolution.h"
#include "UnitProfitDP.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cassert>
#include <chrono>
#include <cxxopts.hpp>
#include <date.h>

template<typename Params, typename Results>
void export_solution_to_csv(std::filesystem::path csv_file_path, const kplink::Problem& p, const Params& params, const Results& results) {
    std::ofstream ofs{csv_file_path};

    if(ofs.fail()) {
        std::cerr << "Cannot write solution to " << csv_file_path << ": skipping!\n";
        return;
    }

    assert(ofs.good());

    ofs << kplink::Problem::csv_header << "," << Params::csv_header << "," << Results::csv_header << "\n";
    ofs << p.to_csv() << "," << params.to_csv() << "," << results.to_csv() << "\n";
}

int main(int argc, char** argv) {
    using namespace kplink;
    using namespace cxxopts;
    using std::chrono::system_clock;

    const std::filesystem::path default_data_folder = std::filesystem::path{".."} / "data";
    const std::filesystem::path default_results_folder = std::filesystem::path{".."} / "results";

    Options opts{"kplink", "Solves the Knapsack-with-Linking Problem"};
    opts.add_options()
        ("p,problem",         "Path of the problem file.", value<std::string>())
        ("i,initial",         "Path to solution file which contains an initial solution. "
                              "Must be a csv file with solution under column 'selected_items' or 'primal_selected_items'. "
                              "Only available with algorithms 'bc', 'compact_lp', 'compact_mip'.", value<std::string>())
        ("a,algorithm",       "Algorithm to use. One of: labelling, compact_mip, compact_lp, bc, greedy, unit_dp. "
                              "Algorithm unit_dp can only be used with instances with all profits == 1.", value<std::string>())
        ("v,validineq",       "Use valid inequalities. Available with algorithms 'bc', 'compact_mip', 'compact_lp'.", value<bool>()->default_value("false"))
        ("f,liftcc",          "Lift compactness constraints. Available with algorithm 'bc', 'compact_mip' and 'compact_lp'.", value<bool>()->default_value("false"))
        ("t,threads",         "If using a Gurobi-based algorithm, number of threads to use.", value<int>()->default_value("1"))
        ("l,timelimit",       "If using a Gurobi-based algorithm, the time limit in seconds.", value<double>()->default_value("3600"))
        ("s,disablepresolve", "If using a Gurobi-based algorithm, disables presolve. "
                              "Available with algorithm 'compact_mip' because presolve is always off for B&C and LP problems.", value<bool>()->default_value("false"))
        ("o,output",          "Save results (in .csv format) in this file. Overwrites previous contents.", value<std::string>())
        ("h,help",            "Prints usage message.");

    const auto res = opts.parse(argc, argv);

    if(res.count("help")) {
        std::cout << opts.help() << "\n";
        return EXIT_SUCCESS;
    }

    if(!res.count("algorithm")) {
        std::cerr << "You must specify an algorithm!\n";
        std::cout << opts.help() << "\n";
        std::exit(EXIT_FAILURE);
    }

    if(!res.count("problem")) {
        std::cerr << "You must specify a problem file!\n";
        std::cout << opts.help() << "\n";
        std::exit(EXIT_FAILURE);
    }

    auto problem_file = std::filesystem::path{res["problem"].as<std::string>()};
    if(!std::filesystem::exists(problem_file)) {
        const auto second_problem_file = default_data_folder / problem_file;

        if(!std::filesystem::exists(second_problem_file)) {
            std::cerr << "File not found: " << problem_file << "\n";
            return(EXIT_FAILURE);
        }

        problem_file = second_problem_file;
    }

    auto out = std::filesystem::path{};
    if(!res.count("output")) {
        out = default_results_folder /
            problem_file.stem().concat(date::format("%Y%m%d%H%M", system_clock::now())).concat(".csv");
    } else {
        out = std::filesystem::path{res["output"].as<std::string>()};
    }

    auto initial_sol_file = std::optional<std::filesystem::path>{};
    if(res.count("initial")) {
        initial_sol_file = std::filesystem::path{res["initial"].as<std::string>()};

        if(!std::filesystem::exists(*initial_sol_file)) {
            std::cerr << "Initial solution file not found: " << *initial_sol_file << "\n";
            return(EXIT_FAILURE);
        }
    }

    const auto p = Problem{problem_file};
    const auto algorithm = res["algorithm"].as<std::string>();

    if(res.count("threads") && res["threads"].as<int>() < 1) {
        std::cerr << "Invalid number of threads: " << res["threads"].as<int>() << "\n";
        std::exit(EXIT_FAILURE);
    }

    if(res.count("timelimit") && res["timelimit"].as<double>() < 0.0) {
        std::cerr << "Invalid time limit: " << res["timelimit"].as<double>() << "\n";
        std::exit(EXIT_FAILURE);
    }

    if(algorithm == "labelling") {
        const auto params = LabellingParams{
            /* .algo_name = */ algorithm,
            /* .time_limit = */ res["timelimit"].as<double>()
        };
        auto labelling = Labelling{
            /* .p = */ p,
            /* .params = */ params 
        };
        const auto solution = labelling.solve();

        export_solution_to_csv(out, p, params, solution);
    } else if (algorithm == "unit_dp") {
        const auto params = UnitDPParams{
            /* .algo_name = */ algorithm
        };
        auto unit_dp = UnitDP{
            /* .p = */ p,
            /* .params = */ params
        };
        const auto solution = unit_dp.solve();

        export_solution_to_csv(out, p, params, solution);
    } else if(algorithm == "compact_mip") {
        const auto params = CompactModelParams{
            /* .algo_name = */ algorithm,
            /* .n_threads = */ res["threads"].as<int>(),
            /* .time_limit = */ res["timelimit"].as<double>(),
            /* .use_vi1 = */ res["validineq"].as<bool>(),
            /* .lift_cc = */ res["liftcc"].as<bool>(),
            /* .use_presolve = */ !res["disablepresolve"].as<bool>()
        };
        auto solver = CompactModel{p, params};

        if(initial_sol_file) {
            solver.load_initial_solution(
                read_initial_solution(*initial_sol_file)
            );
        }

        const auto solution = solver.solve_integer();

        export_solution_to_csv(out, p, params, solution);
    } else if(algorithm == "compact_lp") {
        const auto params = CompactModelParams{
            /* .algo_name = */ algorithm,
            /* .n_threads = */ res["threads"].as<int>(),
            /* .time_limit = */ res["timelimit"].as<double>(),
            /* .use_vi1 = */ res["validineq"].as<bool>(),
            /* .lift_cc = */ res["liftcc"].as<bool>()
        };
        auto solver = CompactModel{p, params};
        const auto solution = solver.solve_continuous_relaxation();

        export_solution_to_csv(out, p, params, solution);
    } else if(algorithm == "bc") {
        const auto params = BranchAndCutParams {
            /* .algo_name = */ algorithm,
            /* .n_threads = */ res["threads"].as<int>(),
            /* .time_limit = */ res["timelimit"].as<double>(),
            /* .use_vi1 = */ res["validineq"].as<bool>(),
            /* .lift_cc = */ res["liftcc"].as<bool>()
        };
        auto solver = BranchAndCut{p, params};

        if(initial_sol_file) {
            solver.load_initial_solution(
                read_initial_solution(*initial_sol_file)
            );
        }

        const auto solution = solver.solve();

        export_solution_to_csv(out, p, params, solution);
    } else if(algorithm == "greedy") {
        const auto params = GreedyHeuristicParams{
            /* .algo_name = */ algorithm
        };
        auto solver = GreedyHeuristic{p};
        const auto solution = solver.solve();

        export_solution_to_csv(out, p, params, solution);
    } else {
        std::cerr << "Algorithm not supported: " << algorithm << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}