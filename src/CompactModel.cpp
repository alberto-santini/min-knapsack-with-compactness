#include "CompactModel.h"
#include "Problem.h"

#include <algorithm>
#include <vector>
#include <cmath>
#include <string>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <chrono>
#include <cassert>
#include <stdexcept>
#include <gurobi_c++.h>

namespace kplink {
    const std::string CompactModelParams::csv_header =
        "algo_name,n_threads,time_limit,weights_rescaling_factor,use_vi1,"
        "lift_cc,use_presolve";
    const std::string CompactModelSolutionStats::csv_header =
        "n_variables,n_constraints,n_non_zero,time_to_build_model,"
        "time_to_solve_model,feasible_integer_solution";
    const std::string CompactModelLinearRelaxationSolutionStats::csv_header =
        CompactModelSolutionStats::csv_header + "," +
        "optimal_linear_relaxation_solution,linear_relaxation_proven_infeasible,"
        "linear_relaxation_selected_items,linear_relaxation_profit,"
        "linear_relaxation_weight";
    const std::string CompactModelIntegerSolutionStats::csv_header =
        CompactModelSolutionStats::csv_header + "," +
        "optimal_solution,proven_infeasible,n_primal_selected_items,primal_selected_items,"
        "primal_profit,primal_weight,best_dual_bound,root_node_primal_bound,root_node_dual_bound,"
        "root_node_time_elapsed,presolve_removed_cols,presolve_removed_rows,"
        "presolve_completely_solved,n_bb_nodes_visited";

    std::string CompactModelParams::to_csv() const {
        return algo_name + "," +
               std::to_string(n_threads) + "," +
               std::to_string(time_limit) + "," +
               std::to_string(weights_rescaling_factor) + "," +
               std::to_string(use_vi1) + "," +
               std::to_string(lift_cc) + "," +
               std::to_string(use_presolve);
    }

    std::string CompactModelSolutionStats::to_csv() const {
        using std::to_string;
        return to_string(n_variables) + "," +
               to_string(n_constraints) + "," +
               to_string(n_non_zero) + "," +
               to_string(time_to_build_model) + "," +
               to_string(time_to_solve_model) + "," +
               to_string(feasible_integer_solution);
    }

    std::string CompactModelLinearRelaxationSolutionStats::to_csv() const {
        using std::to_string;

        std::string s_linear_relaxation_selected_items,
                    s_linear_relaxation_profit,
                    s_linear_relaxation_weight;

        if(linear_relaxation_selected_items) {
            std::ostringstream oss;
            oss << "\"{";
            for(auto i = 0u; i < linear_relaxation_selected_items->size(); ++i) {
                if(const auto val = (*linear_relaxation_selected_items)[i]; val > 1e-9) {
                    oss << i << ":" << val << ",";
                }
            }
            oss << "}\"";
            s_linear_relaxation_selected_items = oss.str();
        } else {
            s_linear_relaxation_selected_items = "none";
        }

        if(linear_relaxation_profit) {
            s_linear_relaxation_profit = std::to_string(*linear_relaxation_profit);
        } else {
            s_linear_relaxation_profit = "none";
        }

        if(linear_relaxation_weight) {
            s_linear_relaxation_weight = std::to_string(*linear_relaxation_weight);
        } else {
            s_linear_relaxation_weight = "none";
        }

        return CompactModelSolutionStats::to_csv() + "," +
               to_string(optimal_linear_relaxation_solution) + "," +
               to_string(linear_relaxation_proven_infeasible) + "," +
               s_linear_relaxation_selected_items + "," +
               s_linear_relaxation_profit + "," +
               s_linear_relaxation_weight;
    }

    std::string CompactModelIntegerSolutionStats::to_csv() const {
        using std::to_string;

        std::string s_primal_selected_items, s_primal_profit, s_primal_weight,
            s_root_node_primal_bound, s_root_node_dual_bound;
        std::size_t n_primal_selected_items = 0u;

        if(primal_selected_items) {
            std::ostringstream oss;
            std::copy(primal_selected_items->begin(), primal_selected_items->end(),
                std::ostream_iterator<std::size_t>(oss, ","));
            s_primal_selected_items = "\"[" + oss.str() + "]\"";
            n_primal_selected_items = primal_selected_items->size();
        } else {
            s_primal_selected_items = "none";
        }

        if(primal_profit) {
            s_primal_profit = to_string(*primal_profit);
        } else {
            s_primal_profit = "none";
        }

        if(primal_weight) {
            s_primal_weight = to_string(*primal_weight);
        } else {
            s_primal_weight = "none";
        }

        const bool presolve_completely_solved = (presolve_removed_cols == n_variables);

        if(!presolve_completely_solved && root_node_primal_bound) {
            s_root_node_primal_bound = to_string(*root_node_primal_bound);
        } else {
            s_root_node_primal_bound = "none";
        }

        if(!presolve_completely_solved && root_node_dual_bound) {
            s_root_node_dual_bound = to_string(*root_node_dual_bound);
        } else {
            s_root_node_dual_bound = "none";
        }

        return CompactModelSolutionStats::to_csv() + "," +
               to_string(optimal_solution) + "," +
               to_string(proven_infeasible) + "," +
               to_string(n_primal_selected_items) + "," +
               s_primal_selected_items + "," +
               s_primal_profit + "," +
               s_primal_weight + "," +
               to_string(best_dual_bound) + "," +
               s_root_node_primal_bound + "," +
               s_root_node_dual_bound + "," +
               to_string(root_node_time_elapsed) + "," +
               to_string(presolve_removed_cols) + "," +
               to_string(presolve_removed_rows) + "," +
               to_string(presolve_completely_solved) + "," +
               to_string(n_bb_nodes_visited);
    }

    namespace {
        struct GurobiRootNodeCB : public GRBCallback {
            CompactModelIntegerSolutionStats& stats;

            GurobiRootNodeCB(CompactModelIntegerSolutionStats& stats) :
                stats{stats} {}

            void callback() override {
                if(where == GRB_CB_MIPNODE) {
                    if(getIntInfo(GRB_CB_MIPNODE_STATUS) != GRB_OPTIMAL) {
                        return;
                    }

                    if((std::size_t) getDoubleInfo(GRB_CB_MIPNODE_NODCNT) != 0u) {
                        return;
                    }

                    if(getIntInfo(GRB_CB_MIPNODE_SOLCNT) > 0) {
                        stats.root_node_primal_bound = getDoubleInfo(GRB_CB_MIPNODE_OBJBST);
                    }

                    stats.root_node_dual_bound = getDoubleInfo(GRB_CB_MIPNODE_OBJBND);
                    stats.root_node_time_elapsed = getDoubleInfo(GRB_CB_RUNTIME);
                } else if(where == GRB_CB_PRESOLVE) {
                    stats.presolve_removed_cols = (std::size_t) getIntInfo(GRB_CB_PRE_COLDEL);
                    stats.presolve_removed_rows = (std::size_t) getIntInfo(GRB_CB_PRE_ROWDEL);
                }
            }
        };
    }

    void CompactModel::load_initial_solution(const std::vector<std::size_t>& initial_solution) {
        for(auto j = 0u; j < p.n_items; ++j) {
            x[j].set(GRB_DoubleAttr_Start, 0.0);
        }

        double weight_check = 0.0, profit_check = 0.0;

        std::cout << "Info: using initial solution (" << initial_solution.size() << " items): ";
        for(const auto j : initial_solution) {
            x[j].set(GRB_DoubleAttr_Start, 1.0);
            weight_check += p.weights[j];
            profit_check += p.profits[j];
            std::cout << j << ", ";
        }
        std::cout << "\b\b \n";
        std::cout << "Info: initial solution weight = " << weight_check << ", profit = " << profit_check << "\n";
    }

    CompactModel::CompactModel(const Problem& p, CompactModelParams params) :
        p{p}, params{params}, env{}, model{env},
        x_type(p.n_items, GRB_CONTINUOUS),
        x_lb(p.n_items, 0.0), x_ub(p.n_items, 1.0)
    {
        using std::chrono::steady_clock, std::chrono::duration_cast, std::chrono::milliseconds;

        params.weights_rescaling_factor = compute_best_weights_rescaling_factor();
        std::cout << "Info: using a weight rescaling factor of " << params.weights_rescaling_factor << "\n";

        const auto model_build_start_time = steady_clock::now();

        for(auto i = 0u; i < p.n_items; ++i) {
            x_name.emplace_back("x_" + std::to_string(i));
        }

        x = model.addVars(x_lb.data(), x_ub.data(), p.profits.data(), x_type.data(), x_name.data(), (int)p.n_items);

        GRBLinExpr weight_lhs;
        for(auto i = 0u; i < p.n_items; ++i) {
            weight_lhs += p.weights[i] * params.weights_rescaling_factor * x[i];
        }

        model.addConstr(weight_lhs >= p.min_weight * params.weights_rescaling_factor, "min_weight");

        for(auto i = 0u; i < p.n_items; ++i) {
            for(auto j = i + p.max_distance + 1; j < p.n_items; ++j) {
                GRBLinExpr max_dist_rhs;

                for(auto k = i + 1u; k <= j - 1u; ++k) {
                    max_dist_rhs += x[k];
                }

                const std::string name = "max_dist_" + std::to_string(i) + "_" + std::to_string(j);

                double mult = 1.0;
                if(params.lift_cc) {
                    const auto dist = (double)(j - i - 1u);
                    mult = std::floor(dist / (double)p.max_distance);
                }

                assert(mult >= 1.0);
                
                model.addConstr(mult * (x[i] + x[j] - 1) <= max_dist_rhs, name);
            }
        }

        if(params.use_vi1) {
            for(auto i = 0u; i < p.n_items; ++i) {
                const auto start_j = (i >= p.max_distance) ?
                    i - p.max_distance : 0u;
                const auto end_j = (i + p.max_distance < p.n_items) ?
                    i + p.max_distance : p.n_items - 1u;

                GRBLinExpr vi1_rhs;

                for(auto j = start_j; j <= end_j; ++j) {
                    if(j != i) {
                        vi1_rhs += x[j];
                    }
                }

                const std::string name = "vi1_" + std::to_string(i);

                model.addConstr(x[i] <= vi1_rhs, name);
            }
        }

        const auto model_build_end_time = steady_clock::now();
        time_to_build_model = duration_cast<milliseconds>(model_build_end_time - model_build_start_time).count() / 1000.0;
    }

    CompactModelLinearRelaxationSolutionStats CompactModel::solve_continuous_relaxation() {
        model.set(GRB_IntParam_Threads, params.n_threads);
        model.set(GRB_IntParam_Presolve, GRB_PRESOLVE_OFF);
        model.set(GRB_DoubleParam_TimeLimit, params.time_limit);

        for(auto i = 0u; i < p.n_items; ++i) {
            x[i].set(GRB_CharAttr_VType, GRB_CONTINUOUS);
        }

        model.optimize();

        const auto status = model.get(GRB_IntAttr_Status);
        auto solution = CompactModelLinearRelaxationSolutionStats{
            /* .n_variables = */   (std::size_t) model.get(GRB_IntAttr_NumVars),
            /* .n_constraints = */ (std::size_t) model.get(GRB_IntAttr_NumConstrs),
            /* .n_non_zero = */    (std::size_t) model.get(GRB_IntAttr_NumNZs),
            /* .time_to_build_model = */ time_to_build_model,
            /* .time_to_solve_model = */  model.get(GRB_DoubleAttr_Runtime),
            /* .feasible_integer_solution = */ false,
            /* .optimal_linear_relaxation_solution = */ false,
            /* .linear_relaxation_proven_infeasible = */ false,
            /* .linear_relaxation_selected_items = */ std::nullopt,
            /* .linear_relaxation_profit = */ std::nullopt,
            /* .linear_relaxation_weight = */ std::nullopt
        };

        if(status == GRB_INFEASIBLE) {
            solution.linear_relaxation_proven_infeasible = true;
            return solution;
        }

        if(status == GRB_SUBOPTIMAL || status == GRB_TIME_LIMIT) {
            return solution;
        }

        if(status == GRB_OPTIMAL) {
            const auto x_vals_raw = model.get(GRB_DoubleAttr_X, x, p.n_items);
            const auto x_vals = std::vector<double>(x_vals_raw, x_vals_raw + p.n_items);
            const bool is_integer = std::all_of(x_vals.begin(), x_vals.end(),
                [] (const double& value) -> bool {
                    return std::fabs(value) < 1e-9 || std::fabs(value) > 1.0 - 1e-9;
                }
            );

            solution.feasible_integer_solution = is_integer;
            solution.optimal_linear_relaxation_solution = true;
            solution.linear_relaxation_selected_items = x_vals;
            solution.linear_relaxation_profit = model.get(GRB_DoubleAttr_ObjVal);
            
            solution.linear_relaxation_weight = 0.0;
            for(auto i = 0u; i < p.n_items; ++i) {
                *solution.linear_relaxation_weight += x_vals[i] * p.weights[i];
            }

            delete[] x_vals_raw;
            return solution;
        }

        throw std::runtime_error("Unhandled Gurobi status: " + std::to_string(status));
    }

    CompactModelIntegerSolutionStats CompactModel::solve_integer() {
        if(!params.use_presolve) {
            std::cout << "Warning: presolve is disabled!\n";
        }

        model.set(GRB_IntParam_Threads, params.n_threads);
        model.set(GRB_IntParam_Presolve, (params.use_presolve ? GRB_PRESOLVE_AUTO : GRB_PRESOLVE_OFF));
        model.set(GRB_DoubleParam_TimeLimit, params.time_limit);

        for(auto i = 0u; i < p.n_items; ++i) {
            x[i].set(GRB_CharAttr_VType, GRB_BINARY);
        }

        auto solution = CompactModelIntegerSolutionStats{
            /* .n_variables = */ 0u,
            /* .n_constraints = */ 0u,
            /* .n_non_zero = */ 0u,
            /* .time_to_build_model = */ time_to_build_model,
            /* .time_to_solve_model = */ 0.0,
            /* .feasible_integer_solution = */ false,
            /* .optimal_solution = */ false,
            /* .proven_infeasible = */ false,
            /* .primal_selected_items = */ std::nullopt,
            /* .primal_profit = */ std::nullopt,
            /* .primal_weight = */ std::nullopt,
            /* .best_dual_bound = */ 0.0,
            /* .root_node_primal_bound = */ std::nullopt,
            /* .root_node_dual_bound = */ 0.0,
            /* .root_node_time_elapsed = */ 0.0,
            /* .presolve_removed_cols = */ 0u,
            /* .presolve_removed_rows = */ 0u,
            /* .n_bb_nodes_visited = */ 0u
        };

        auto root_node_cb = GurobiRootNodeCB{solution};
        model.setCallback(&root_node_cb);
        model.optimize();

        solution.n_variables = (std::size_t) model.get(GRB_IntAttr_NumVars);
        solution.n_constraints = (std::size_t) model.get(GRB_IntAttr_NumConstrs);
        solution.n_non_zero = (std::size_t) model.get(GRB_IntAttr_NumNZs);
        solution.time_to_solve_model = model.get(GRB_DoubleAttr_Runtime);
        solution.n_bb_nodes_visited = (std::size_t) model.get(GRB_DoubleAttr_NodeCount);
        const auto status = model.get(GRB_IntAttr_Status);

        if(status == GRB_INFEASIBLE) {
            solution.proven_infeasible = true;
            return solution;
        }

        if(status == GRB_SUBOPTIMAL || status == GRB_OPTIMAL || status == GRB_TIME_LIMIT) {
            solution.feasible_integer_solution = (model.get(GRB_IntAttr_SolCount) > 0);
            solution.optimal_solution = (status == GRB_OPTIMAL);

            // If Gurobi finds the optimum via a presolve heuristic and can prove it is
            // the optimum without branching, it never calls GRB_CB_MIPNODE and we cannot
            // set the root node stats there.
            if( solution.optimal_solution &&
                solution.n_bb_nodes_visited <= 1u
            ) {
                solution.root_node_primal_bound = model.get(GRB_DoubleAttr_ObjVal);
                solution.root_node_dual_bound = model.get(GRB_DoubleAttr_ObjVal);
            }
            
            if(solution.feasible_integer_solution) {
                const auto x_vals_raw = model.get(GRB_DoubleAttr_X, x, p.n_items);
                solution.primal_selected_items = std::vector<std::size_t>();
                solution.primal_profit = model.get(GRB_DoubleAttr_ObjVal);
                solution.primal_weight = 0.0;
                for(auto i = 0u; i < p.n_items; ++i) {
                    if(x_vals_raw[i] > 0.5) {
                        solution.primal_selected_items->push_back(i);
                        *solution.primal_weight += p.weights[i];
                    }
                }
                delete[] x_vals_raw;
            }

            solution.best_dual_bound = model.get(GRB_DoubleAttr_ObjBound);
            return solution;
        }

        throw std::runtime_error("Unhandled Gurobi status: " + std::to_string(status));
    }

    double CompactModel::compute_best_weights_rescaling_factor() const {
        const auto [min_it, max_it] = std::minmax_element(p.weights.begin(), p.weights.end());
        
        if(*min_it > 1e-3) {
            // No rescaling necessary.
            return 1.0;
        }

        // Rescaling factor to bring the smallest number up to 1e-3.
        double weights_rescaling_factor = 1e-3 / *min_it;

        if(*max_it * weights_rescaling_factor > 1e4) {
            // Rescaling factor would make the biggest number too big.
            weights_rescaling_factor = 1e4 / *max_it;
        }

        return weights_rescaling_factor;
    }
}