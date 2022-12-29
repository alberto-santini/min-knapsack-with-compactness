#include "BranchAndCut.h"
#include "BranchAndCutSeparation.h"

#include <string>
#include <iostream>
#include <iterator>
#include <chrono>
#include <optional>
#include <cmath>
#include <algorithm>
#include <gurobi_c++.h>

namespace kplink {
    const std::string BranchAndCutParams::csv_header =
        "algo_name,n_threads,time_limit,weights_rescaling_factor,use_vi1,lift_cc";
    const std::string BranchAndCutSolutionStats::csv_header =
        "n_cuts_added_on_integer,n_cuts_added_on_fractional,"
        "feasible_integer_solution,optimal_solution,proven_infeasible,"
        "n_primal_selected_items,primal_selected_items,primal_profit,primal_weight,"
        "best_dual_bound,time_elapsed,separation_cb_time_elapsed,"
        "root_node_primal_bound,root_node_dual_bound,root_node_time_elapsed,"
        "n_bb_nodes_visited";

    std::string BranchAndCutParams::to_csv() const {
        return  algo_name + "," +
                std::to_string(n_threads) + "," +
                std::to_string(time_limit) + "," +
                std::to_string(weights_rescaling_factor) + "," +
                std::to_string(use_vi1) + "," +
                std::to_string(lift_cc);
    }

    std::string BranchAndCutSolutionStats::to_csv() const {
        using std::to_string;

        std::string s_primal_selected_items, s_primal_profit, s_primal_weight, s_root_node_primal_bound;
        std::string s_n_primal_selected_items = "none";

        if(primal_selected_items) {
            std::ostringstream oss;
            std::copy(primal_selected_items->begin(), primal_selected_items->end(),
                std::ostream_iterator<std::size_t>(oss, ","));
            s_primal_selected_items = "\"[" + oss.str() + "]\"";
            s_n_primal_selected_items = to_string(primal_selected_items->size());
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

        if(root_node_primal_bound) {
            s_root_node_primal_bound = to_string(*root_node_primal_bound);
        } else {
            s_root_node_primal_bound = "none";
        }

        return to_string(n_cuts_added_on_integer) + "," +
               to_string(n_cuts_added_on_fractional) + "," +
               to_string(feasible_integer_solution) + "," +
               to_string(optimal_solution) + "," +
               to_string(proven_infeasible) + "," +
               s_n_primal_selected_items + "," +
               s_primal_selected_items + "," +
               s_primal_profit + "," +
               s_primal_weight + "," +
               to_string(best_dual_bound) + "," +
               to_string(time_elapsed) + "," +
               to_string(separation_cb_time_elapsed) + "," +
               s_root_node_primal_bound + "," +
               to_string(root_node_dual_bound) + "," +
               to_string(root_node_time_elapsed) + "," +
               to_string(n_bb_nodes_visited);
    }

    namespace {
        struct GurobiRootNodeCB : public GRBCallback {
            BranchAndCutSolutionStats& stats;

            GurobiRootNodeCB(BranchAndCutSolutionStats& stats) :
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
                }
            }
        };
    }

    double BranchAndCut::compute_best_weights_rescaling_factor() const {
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

    void BranchAndCut::load_initial_solution(const std::vector<std::size_t>& initial_solution) {
        std::cout << "Info: using initial solution (" << initial_solution.size() << " items): ";
        for(const auto j : initial_solution) {
            x[j].set(GRB_DoubleAttr_Start, 1.0);
            std::cout << j << ", ";
        }
        std::cout << "\b\b \n";
    }

    BranchAndCut::BranchAndCut(const Problem& p, BranchAndCutParams params) :
        p{p}, params{params}, env{}, model{env},
        x_type(p.n_items, GRB_BINARY),
        x_lb(p.n_items, 0.0), x_ub(p.n_items, 1.0)
    {
        params.weights_rescaling_factor = compute_best_weights_rescaling_factor();
        std::cout << "Info: using a weight rescaling factor of " << params.weights_rescaling_factor << "\n";

        for(auto i = 0u; i < p.n_items; ++i) {
            x_name.emplace_back("x_" + std::to_string(i));
        }

        x = model.addVars(x_lb.data(), x_ub.data(), p.profits.data(), x_type.data(), x_name.data(), (int)p.n_items);

        GRBLinExpr weight_lhs;
        for(auto i = 0u; i < p.n_items; ++i) {
            weight_lhs += p.weights[i] * params.weights_rescaling_factor * x[i];
        }

        model.addConstr(weight_lhs >= p.min_weight * params.weights_rescaling_factor, "min_weight");

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
    }

    BranchAndCutSolutionStats BranchAndCut::solve() {
        model.set(GRB_IntParam_Threads, params.n_threads);
        model.set(GRB_DoubleParam_TimeLimit, params.time_limit);
        model.set(GRB_IntParam_LazyConstraints, 1);

        auto solution = BranchAndCutSolutionStats {
            /* .n_cuts_added_on_integer = */ 0u,
            /* .n_cuts_added_on_fractional = */ 0u,
            /* .feasible_integer_solution = */ false,
            /* .optimal_solution = */ false,
            /* .proven_infeasible = */ false,
            /* .primal_selected_items = */ std::nullopt,
            /* .primal_profit = */ std::nullopt,
            /* .primal_weight = */ std::nullopt,
            /* .best_dual_bound = */ 0.0,
            /* .time_elapsed = */ 0.0,
            /* .separation_cb_time_elapsed = */ 0.0,
            /* .root_node_primal_bound = */ std::nullopt,
            /* .root_node_dual_bound = */ 0.0,
            /* .root_node_time_elapsed = */ 0.0,
            /* .n_bb_nodes_visited = */ 0u
        };

        auto root_node_cb = GurobiRootNodeCB{solution};
        auto separation_cv = BranchAndCutSeparationCB{p, params, solution, x};
        model.setCallback(&root_node_cb);
        model.setCallback(&separation_cv);
        model.optimize();

        solution.time_elapsed = model.get(GRB_DoubleAttr_Runtime);
        solution.n_bb_nodes_visited = (std::size_t) model.get(GRB_DoubleAttr_NodeCount);
        const auto status = model.get(GRB_IntAttr_Status);

        if(status == GRB_INFEASIBLE) {
            solution.proven_infeasible = true;
            return solution;
        }

        if(status == GRB_SUBOPTIMAL || status == GRB_OPTIMAL || status == GRB_TIME_LIMIT) {
            solution.feasible_integer_solution = (model.get(GRB_IntAttr_SolCount) > 0);
            solution.optimal_solution = (status == GRB_OPTIMAL);

            // If Gurobi finds the optimum via a presolve heuristic, and confirms it is
            // feasible via the callback, it never calls GRB_CB_MIPNODE and we can never
            // update the root node bounds.
            if(solution.optimal_solution && solution.n_bb_nodes_visited <= 1u) {
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
}