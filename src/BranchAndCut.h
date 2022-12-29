#ifndef _BRANCH_AND_CUT_H
#define _BRANCH_AND_CUT_H

#include <string>
#include <optional>
#include <gurobi_c++.h>
#include "Problem.h"

namespace kplink {
    struct BranchAndCutParams {
        /** Human-readable algorithm name. */
        std::string algo_name;

        /**
         * Number of threads Gurobi can use. 
         * 
         * It is of type (signed) int because this is the type
         * accepted by Gurobi.
        */
        int n_threads = 1;

        /** Gurobi time limit. */
        double time_limit = 3600.0;

        /** Use valid inequality 1. */
        bool use_vi1 = false;

        /** Lift compactness constraints. */
        bool lift_cc = false;

        /** Rescaling factor for the capacity constraint.
         * 
         *  We multiply LHS and RHS of the capacity constraint by this number,
         * to mitigate numerical problems due to most weights being very small.
         */
        double weights_rescaling_factor = 1.0e3;

        /** Header for csv files. */
        static const std::string csv_header;

        /** Export to comma-separated list. */
        [[nodiscard]] std::string to_csv() const;
    };

    struct BranchAndCutSolutionStats {
        /** Number of cats added by separation on an infeasible integer solution. */
        std::size_t n_cuts_added_on_integer;

        /** Number of cats added by separation on an infeasible fractional solution. */
        std::size_t n_cuts_added_on_fractional;

        /** Whether a feasible integer solution was produced. */
        bool feasible_integer_solution;

        /** Whether the optimal solution was found. */
        bool optimal_solution;

        /** Whether the integer_version is proven infeasible. */
        bool proven_infeasible;

        /** Selected items in the best feasible solution found, if any. */
        std::optional<std::vector<std::size_t>> primal_selected_items;

        /** Profit collected by the best feasible solution found, if any. */
        std::optional<double> primal_profit;
        
        /** Weight collected by the best feasible solution found, if any. */
        std::optional<double> primal_weight;

        /** Best dual bound at timeout. */
        double best_dual_bound;

        /** Total time elapsed, in seconds. */
        double time_elapsed;

        /** Time spent in the separation callback. */
        double separation_cb_time_elapsed;

        /** Best primal bound at root node, if any. */
        std::optional<double> root_node_primal_bound;

        /** Best dual bound at the root node, after adding cuts. */
        double root_node_dual_bound;

        /** Time elapsed at the root node. */
        double root_node_time_elapsed;

        /** Number of B&B nodes visited. */
        std::size_t n_bb_nodes_visited;

        /** Header for csv files. */
        static const std::string csv_header;

        /** Export to comma-separated list. */
        [[nodiscard]] std::string to_csv() const;
    };

    struct BranchAndCut {
        /** Problem instance. */
        const Problem& p;

        /** Solver parameters. */
        BranchAndCutParams params;

        /** Gurobi environment. */
        GRBEnv env;

        /** Gurobi model. */
        GRBModel model;

        /** Only set of (KP) variables for the model. */
        GRBVar* x;

        /** Variable types for the x variables. */
        std::vector<char> x_type;
        
        /** Lower bounds of the x variables. */
        std::vector<double> x_lb;

        /** Upper bounds of the x variables. */
        std::vector<double> x_ub;

        /** Name of the x variables. */
        std::vector<std::string> x_name;

        /** Build model for a problem. */
        BranchAndCut(const Problem& p, BranchAndCutParams params = BranchAndCutParams());

        /** Loads an initial solution into the model. */
        void load_initial_solution(const std::vector<std::size_t>& initial_solution);

        /** Solves the integer programme via branch-and-cut. */
        [[nodiscard]] BranchAndCutSolutionStats solve();

        private:

        /** Finds an appropriate rescaling factor for the capacity constraint.
         * 
         *  It sets params.weights_rescaling_factor, searching for the largest factor, attempting
         *  to bring the smallest weight to at least 1e-3, but making sure that the largest
         *  weight is not larger than 1e4.
         */
        [[nodiscard]] double compute_best_weights_rescaling_factor() const;
    };
}

#endif