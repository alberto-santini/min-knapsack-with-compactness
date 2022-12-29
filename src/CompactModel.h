#ifndef _COMPACT_MODEL_H
#define _COMPACT_MODEL_H

#include "Problem.h"

#include <gurobi_c++.h>
#include <cstddef>
#include <vector>
#include <optional>
#include <string>

namespace kplink {
    struct CompactModelParams {
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

        /** Use presolve for the MIP model? */
        bool use_presolve = true;

        /** Rescaling factor for the capacity constraint.
         * 
         *  We multiply LHS and RHS of the capacity constraint by this number,
         *  to mitigate numerical problems due to most weights being very small.
         */
        double weights_rescaling_factor = 1.0e3;

        /** Header for csv files. */
        static const std::string csv_header;

        /** Export to comma-separated list. */
        [[nodiscard]] std::string to_csv() const;
    };

    struct CompactModelSolutionStats {
        /** Number of variables in the model. */
        std::size_t n_variables;

        /** Number of constraints in the model. */
        std::size_t n_constraints;

        /** Number of non-zero entries in the constraint matrix of the model. */
        std::size_t n_non_zero;

        /** Time needed to build the model, in seconds. */
        double time_to_build_model;

        /** Time elapsed solving the model (excluding the build time), in seconds. */
        double time_to_solve_model;

        /** Whether a feasible integer solution was produced. */
        bool feasible_integer_solution;

        /** Header for csv files. */
        static const std::string csv_header;

        /** Export to comma-separated list. */
        [[nodiscard]] std::string to_csv() const;
    };

    struct CompactModelLinearRelaxationSolutionStats : CompactModelSolutionStats {
        /** Whether the optimal solution to the integer relaxation was found. */        
        bool optimal_linear_relaxation_solution;

        /** Whether the continuous relaxation is proven infeasible. */
        bool linear_relaxation_proven_infeasible;

        /**
         * (Fractionally) selected items in the optimal solution of the linear relaxation,
         * if found.
         * 
         * The value at index i correspond to the coefficient of variable x[i] in
         * the optimal solution of the linear relaxation.
         * */
        std::optional<std::vector<double>> linear_relaxation_selected_items;

        /** Objective value of the optimal solution of the linear relaxation, if found. */
        std::optional<double> linear_relaxation_profit;

        /** Weight of the optimal solution of the linear relaxation, if found. */
        std::optional<double> linear_relaxation_weight;

        /** Header for csv files. */
        static const std::string csv_header;

        /** Export to comma-separated list. */
        [[nodiscard]] std::string to_csv() const;
    };

    struct CompactModelIntegerSolutionStats : CompactModelSolutionStats {
        /** Whether the optimal solution was found. */
        bool optimal_solution;

        /** Whether the integer_version is proven infeasible. */
        bool proven_infeasible;
        
        /** Selected items in the best feasible solution found, if any. */
        std::optional<std::vector<std::size_t>> primal_selected_items;

        /** Objective function of the best feasible solution found, if any. */
        std::optional<double> primal_profit;
        
        /** Weight collected by the best feasible solution found, if any. */
        std::optional<double> primal_weight;

        /** Best dual bound at timeout. */
        double best_dual_bound;

        /** Best primal bound at root node, if any. */
        std::optional<double> root_node_primal_bound;

        /**
         * Best dual bound at the root node.
         * 
         * This might be different from the optimal solution of the linear relaxation
         * because when solving the linear relaxation explicitly we disable cuts and
         * preprocessing. On the other hand, they are enabled when solving the root
         * node during B&B.
         * 
         * Furthermore, this can be left to std::nullopt in case presolve completely
         * solves the model and we never solve the root node.
         */
        std::optional<double> root_node_dual_bound;

        /** Time elapsed at the root node. */
        double root_node_time_elapsed;

        /** Number of columns eliminated by presolve. */
        std::size_t presolve_removed_cols;

        /** Number of rows eliminated by presolve. */
        std::size_t presolve_removed_rows;

        /** Number of B&B nodes visited. */
        std::size_t n_bb_nodes_visited;

        /** Header for csv files. */
        static const std::string csv_header;

        /** Export to comma-separated list. */
        [[nodiscard]] std::string to_csv() const;
    };

    struct CompactModel {
        /** Problem instance. */
        const Problem& p;

        /** Solver parameters. */
        CompactModelParams params;

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

        /** Time needed to build the model, in seconds. */
        double time_to_build_model;

        /** Build model for a problem. */
        CompactModel(const Problem& p, CompactModelParams params = CompactModelParams{});

        /** Deleter to clean up variable pointers. */
        ~CompactModel() { delete x; }

        /** Solves the integer version of the compact model using Gurobi. */
        [[nodiscard]] CompactModelIntegerSolutionStats solve_integer();

        /** Solves the linear relaxation of the compact model using Gurobi. */
        [[nodiscard]] CompactModelLinearRelaxationSolutionStats solve_continuous_relaxation();

        /** Loads an initial solution into the model. */
        void load_initial_solution(const std::vector<std::size_t>& initial_solution);

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