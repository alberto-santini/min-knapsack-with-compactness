#include "BranchAndCutSeparation.h"
#include "BranchAndCut.h"

#include <chrono>
#include <iostream>
#include <optional>
#include <cmath>
#include <cassert>
#include <gurobi_c++.h>

namespace kplink {
    void BranchAndCutSeparationCB::callback() {
        using std::chrono::steady_clock, std::chrono::milliseconds, std::chrono::duration_cast;

        const auto start_time = steady_clock::now();

        if(where == GRB_CB_MIPSOL) {
            double *x_vals_raw = getSolution(x, p.n_items);
            integer_separation(x_vals_raw);
            delete[] x_vals_raw;
        }

        if(where == GRB_CB_MIPNODE) {
            if(getIntInfo(GRB_CB_MIPNODE_STATUS) != GRB_OPTIMAL) {
                return;
            }

            double *x_vals_raw = getNodeRel(x, p.n_items);

            // Sometimes Gurobi gives extremely small (but non-zero)
            // values to variables, even when the solution is integer.
            // Same for variables which should be 1, but are 0.99999...
            for(auto i = 0u; i < p.n_items; ++i) {
                if(x_vals_raw[i] < eps) {
                    x_vals_raw[i] = 0.0;
                } else if(x_vals_raw[i] > 1.0 - eps) {
                    x_vals_raw[i] = 1.0;
                }
            }
            
            if(is_integer(x_vals_raw)) {
                integer_separation(x_vals_raw);
            } else {
                fractional_separation(x_vals_raw);
            }

            delete[] x_vals_raw;
        }

        const auto end_time = steady_clock::now();
        stats.separation_cb_time_elapsed +=
            duration_cast<milliseconds>(end_time - start_time).count() / 1000.0;
    }

    void BranchAndCutSeparationCB::integer_separation(const double *const x_vals_raw) {
        for(auto i = 0u; i < p.n_items; ++i) {
            if(x_vals_raw[i] > eps && add_integer_cut_for(i, x_vals_raw)) {
                stats.n_cuts_added_on_integer += 1;
            }
        }
    }

    void BranchAndCutSeparationCB::fractional_separation(const double *const x_vals_raw) {
        for(auto i = 0u; i < p.n_items; ++i) {
            if(x_vals_raw[i] > eps && add_fractional_cut_for(i, x_vals_raw)) {
                stats.n_cuts_added_on_fractional += 1;
            }
        }
    }

    std::optional<std::size_t> BranchAndCutSeparationCB::first_integer_selected_after(std::size_t i, const double *const x_vals_raw) const {
        for(auto j = i + 1u; j < p.n_items; ++j) {
            if(x_vals_raw[j] > 0.5) {
                return j;
            }
        }
        return std::nullopt;
    }

    bool BranchAndCutSeparationCB::add_integer_cut_for(std::size_t i, const double *const x_vals_raw) {
        const auto j = first_integer_selected_after(i, x_vals_raw);

        if(j && *j > i + p.max_distance) {
            #ifdef BC_DEBUG
                double_check_violation_for(i, *j, x_vals_raw, "integer");
            #endif

            add_lazy_for(i, *j);
            return true;
        }

        return false;
    }

    void BranchAndCutSeparationCB::double_check_violation_for(std::size_t i, std::size_t j, const double *const x_vals_raw, const char *const sep_type) const {
        double rhs = 0.0;

        for(auto k = i + 1; k <= j - 1u; ++k) {
            rhs += x_vals_raw[k];
        }

        if(x_vals_raw[i] + x_vals_raw[j] <= rhs + 1.0 - eps) {
            std::cerr << "Incorrect " << sep_type << " separation for i = " << i << ", j = " << j << "\n";
            std::cerr << "x[" << i << "] = " << x_vals_raw[i] << ", x[" << j << "] = " << x_vals_raw[j] << "\n";
            std::cerr << "sum(i < k < j) x[k] = " << rhs << "\n";
        }
    }

    bool BranchAndCutSeparationCB::add_fractional_cut_for(std::size_t i, const double *const x_vals_raw) {
        const auto k_end = std::min(p.n_items - 1u, i + p.max_distance);
        double cumulative_weight = 0.0;
        for(auto k = i + 1u; k <= k_end; ++k) {
            cumulative_weight += x_vals_raw[k];

            if(cumulative_weight >= 1.0 - eps) {
                return false;
            }
        }

        const auto j_start = std::min(p.n_items - 1u, i + p.max_distance + 1u);

        if(j_start <= i) {
            return false;
        }

        for(auto j = j_start; j < p.n_items; ++j) {
            if(x_vals_raw[j] < eps) {
                continue;
            }

            if(x_vals_raw[i] + x_vals_raw[j] > cumulative_weight + 1.0 + eps) {
                #ifdef BC_DEBUG
                    double_check_violation_for(i, j, x_vals_raw, "fractional");
                #endif

                add_lazy_for(i, j);
                return true;
            } else {
                cumulative_weight += x_vals_raw[j];
            }
        }

        return false;
    }

    bool BranchAndCutSeparationCB::is_integer(const double *const x_vals_raw) const {
        for(auto i = 0u; i < p.n_items; ++i) {
            if(x_vals_raw[i] > eps || x_vals_raw[i] < 1 - eps) {
                return false;
            }
        }

        return true;
    }

    void BranchAndCutSeparationCB::add_lazy_for(std::size_t i, std::size_t j) {
        assert(j > i + p.max_distance);

        GRBLinExpr rhs;
        for(auto k = i + 1u; k <= j - 1u; ++k) {
            rhs += x[k];
        }

        double mult = 1.0;
        if(params.lift_cc) {
            const auto dist = (double)(j - i - 1u);
            mult = std::floor(dist / (double)p.max_distance);
        }

        assert(mult >= 1.0);

        addLazy(mult * (x[i] + x[j] - 1) <= rhs);
    }
}