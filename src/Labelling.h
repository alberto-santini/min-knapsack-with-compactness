#ifndef _LABELLING_H
#define _LABELLING_H

#include <cstddef>
#include <limits>
#include <optional>
#include <memory>
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <cassert>

#include "Problem.h"

namespace kplink {
    struct Label {
        static constexpr std::size_t SOURCE = std::numeric_limits<std::size_t>::max() - 2u;
        static constexpr std::size_t SINK = std::numeric_limits<std::size_t>::max() - 1u;

        /** Current item. */
        std::size_t current_item;

        /** Profit collected in the current partial solution. */
        double profit;

        /** Weight collected in the current partial solution. */
        double weight;

        /** True if the label was already extended. */
        bool extended = false;

        /** Predecessor label. */
        const Label *const predecessor;

        /**
         * Whether this label dominates another one.
         * 
         * Dominance is not strict, i.e., L1 dominates L2 if
         *  L1::current_item == L2::current_item AND
         *  L1::profit <= L2::profit AND
         *  L1::weight >= L2::weight
         * and there is no need to enforce any or at least one
         * of the two inequalities to be strict.
         */
        bool dominates(const Label& other) const {
            return (current_item == other.current_item) &&
                   (profit <= other.profit) &&
                   (weight >= other.weight);
        }

        /**
         * Operator required to store Label in std::set.
         */
        bool operator<(const Label& other) const {
            return std::make_tuple(current_item, profit, weight) <
                   std::make_tuple(other.current_item, other.profit, other.weight);
        }
    };

    std::ostream& operator<<(std::ostream& out, const Label& label);

    struct LabellingParams {
        /** Algorithm name. */
        std::string algo_name;

        /** Time limit in seconds. */
        double time_limit = 3600.0;

        /** Header for csv files. */
        static const std::string csv_header;

        /** Export to comma-separated list. */
        [[nodiscard]] std::string to_csv() const;
    };

    struct LabellingSolution {
        /** Selected items. */
        std::vector<std::size_t> selected_items;

        /** Profit collected. */
        double profit;

        /** Weight collected. */
        double weight;

        /** Time elapsed in seconds. */
        double time_elapsed;

        /** Number of undominated labels at the sink. */
        std::size_t n_undominated_labels_at_sink;

        /** Header for csv files. */
        static const std::string csv_header;

        /** Export to comma-separated list. */
        [[nodiscard]] std::string to_csv() const;
    };

    std::ostream& operator<<(std::ostream& out, const LabellingSolution& sol);

    struct Labelling {
        /** Problem instance. */
        const Problem& p;

        /** Labelling algorithm parameters. */
        const LabellingParams params;

        /** Builds the algorithm object from the problem instance. */
        Labelling(const Problem& p, const LabellingParams params) : p{p}, params{params} {}

        /** Executes the labelling algorithm. */
        [[nodiscard]] LabellingSolution solve();

        /**
         * Data structure used to hold the labels.
         * 
         * It maps the resident item to a set of labels which
         * have that item as their ::current_item.
         */
        using Labels = std::map<std::size_t, std::set<Label>>;

        /** Collection of all labels. */
        Labels labels;

    private:
        /**
         * Extendes a label to a new item.
         * 
         * It assumes that the extension is feasible:
         * this should be checked before calling this
         * function.
         */
        Label get_extension(const Label& label, std::size_t destination) const {
            assert(destination == Label::SINK || destination < p.n_items);

            const double new_profit =
                (destination == Label::SINK) ? label.profit : label.profit + p.profits[destination];

            const double new_weight =
                (destination == Label::SINK) ? label.weight : label.weight + p.weights[destination];

            const bool extended =
                (destination == Label::SINK) ? true : false;

            return Label{
                /* .current_item = */ destination,
                /* .profit = */ new_profit,
                /* .weight = */ new_weight,
                /* .extended = */ extended,
                /* .predecessor = */ &label
            };
        }

        /**
         * Stores a label in the collection of all labels.
         * 
         * If this is the first label with the given current vertex,
         * it creates the corresponding entry in the map.
         */
        void store_label(Label label) {
            if(labels.find(label.current_item) != labels.end()) {
                labels[label.current_item].insert(std::move(label));
            } else {
                labels[label.current_item] = { std::move(label) };
            }
        }

        /**
         * Gets the set of labels residing at a node/item, if any.
         * 
         * If no labels are yet at the item, it returns nullopt.
         */
        std::optional<std::reference_wrapper<std::set<Label>>> get_labels_at(std::size_t item) {
            if(labels.find(item) != labels.end()) {
                return std::ref(labels[item]);
            } else {
                return std::nullopt;
            }
        }

        /**
         * Gets an unexteded label, if any.
         * 
         * If there are unextended labels, it returns a reference
         * to one of them.
         * Otherwise, it returns std::nullopt.
         * 
         * For the moment we search for labels in whatever
         * arbitrary order they are returned by the iterators
         * over the map and the sets which form its values.
         */
        std::optional<std::reference_wrapper<const Label>> get_unextended_label() const {
            for(const auto& [current_item, label_set] : labels) {
                for(const auto& label : label_set) {
                    if(!label.extended) {
                        return std::ref(label);
                    }
                }
            }
            return std::nullopt;
        }

        /**
         * Extends a label to a new destination item.
         * 
         * It assumes that the extension is feasible.
         * 
         * It creates the new extended label and performs the
         * following checks:
         *  - If there is any label at the destination which
         *    dominates the new extension, then it "kills" the
         *    extension immediately.
         *  - If the new extension dominates any label at the
         *    destination, it removes the dominated label.
         */
        void extend_label(const Label& label, std::size_t destination);

        /** Prints all the labels to stdout. */
        void print_labels() const;
    };
}

#endif