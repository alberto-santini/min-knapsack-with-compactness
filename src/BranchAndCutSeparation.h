#ifndef _BRANCH_AND_CUT_SEPARATION_H
#define _BRANCH_AND_CUT_SEPARATION_H

#include "Problem.h"
#include "BranchAndCut.h"
#include <gurobi_c++.h>

namespace kplink {
    struct BranchAndCutSeparationCB : GRBCallback {
        /** Problem instance. */
        const Problem& p;

        /** Solver parameters. */
        const BranchAndCutParams& params;

        /** Solution statistics to update. */
        BranchAndCutSolutionStats& stats;

        /** Pointer to x variables of the model. */
        GRBVar* x;

        /** Epsilon to check integrality and violations. */
        const double eps;

        BranchAndCutSeparationCB(
            const Problem& p, BranchAndCutParams& params,
            BranchAndCutSolutionStats& stats, GRBVar* x
        ) :
            p{p}, params{params}, stats{stats}, x{x}, eps{1e-6} {}

        /** Callback for Gurobi to call. */
        void callback() override;

    private:
        /** Separate inequalities violated by an integer solution. */
        void integer_separation(const double *const x_vals_raw);

        /** Separate inequalities violated by a fractional solution. */
        void fractional_separation(const double *const x_vals_raw);

        /** Finds the first object selected after object i, in an integer solution. */
        std::optional<std::size_t> first_integer_selected_after(std::size_t i, const double *const x_vals_raw) const;

        /** Adds a cut for item i, using integer separation, if a violated constraint is identified. */
        bool add_integer_cut_for(std::size_t i, const double *const x_vals_raw);

        /** Double-checks that items i and j violated a compactness constraint. (For debugging purposes.) */
        void double_check_violation_for(std::size_t i, std::size_t j, const double *const x_vals_raw, const char *const sep_type) const;

        /** Adds a cut for item i, using fractional separation, if a violated constraint is identified. */
        bool add_fractional_cut_for(std::size_t i, const double *const x_vals_raw);

        /** Checks whether a solution is integer. */
        bool is_integer(const double *const x_vals_raw) const;

        /** Adds a violated compactness constraint for items i and j. */
        void add_lazy_for(std::size_t i, std::size_t j);
    };
}

#endif