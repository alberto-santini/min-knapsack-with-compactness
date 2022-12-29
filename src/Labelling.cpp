#include "Labelling.h"

#include <cstddef>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <chrono>
#include <iostream>
#include <iterator>
#include <cassert>
#include <algorithm>

namespace kplink {
    const std::string LabellingParams::csv_header =
        "algo_name,time_limit";
    const std::string LabellingSolution::csv_header =
        "n_selected_items,selected_items,profit,weight,time_elapsed,n_undominated_labels_at_sink";

    std::string LabellingParams::to_csv() const {
        return algo_name + "," + std::to_string(time_limit);
    }

    std::string LabellingSolution::to_csv() const {
        std::ostringstream oss;
        std::copy(selected_items.begin(), selected_items.end(),
            std::ostream_iterator<std::size_t>(oss, ","));

        return std::to_string(selected_items.size()) + "," +
               "\"[" + oss.str() + "]\"," +
               std::to_string(profit) + "," +
               std::to_string(weight) + "," +
               std::to_string(time_elapsed) + "," +
               std::to_string(n_undominated_labels_at_sink);
    }

    std::ostream& operator<<(std::ostream& out, const Label& label) {
        out << "Label[ item = " << label.current_item << ", "
            << "profit = " << label.profit << ", "
            << "weight = " << label.weight << ", "
            << "extended = " << std::boolalpha << label.extended << ", "
            << "address = " << &label << ", ";

        if(label.predecessor != nullptr) {
            out << "predecessor = " << label.predecessor;
        } else {
            out << "no predecessor";
        }

        out << " ]";
        return out;
    }

    std::ostream& operator<<(std::ostream& out, const LabellingSolution& sol) {
        out << "LabellingSolution[ value = " << sol.selected_items.size() << ", "
            << "profit = " << sol.profit << ", "
            << "weight = " << sol.weight << ", "
            << "time (s) = " << sol.time_elapsed << ", "
            << "final labels = " << sol.n_undominated_labels_at_sink << " ]\n";
        out << "\tSelected items: ";
        std::copy(sol.selected_items.begin(), sol.selected_items.end(),
            std::ostream_iterator<std::size_t>(out, ", "));
        out << "\n";
        return out;
    }

    LabellingSolution Labelling::solve() {
        using std::chrono::steady_clock;
        using std::chrono::duration_cast;
        using std::chrono::milliseconds;

        const auto start_time = steady_clock::now();

        store_label(Label{
            /* .current_item = */ Label::SOURCE,
            /* .profit = */ 0.0,
            /* .weight = */ 0.0,
            /* .extended = */ false,
            /* .predecessor = */ nullptr
        });

        while(true) {
            #ifdef DEBUG
                print_labels();
            #endif

            const auto current_time = steady_clock::now();
            const auto elapsed_time_s = duration_cast<milliseconds>(current_time - start_time).count() / 1000.0;

            if(elapsed_time_s > params.time_limit) {
                break;
            }

            const auto current_label = get_unextended_label();

            if(!current_label) {
                break;
            }

            #ifdef DEBUG
                std::cout << "Selected label for extension: " << *current_label << "\n";
            #endif

            if(current_label->get().weight >= p.min_weight) {
                extend_label(*current_label, Label::SINK);
            } else {
                const auto current_item = current_label->get().current_item;

                if(current_item == Label::SOURCE) {
                    for(std::size_t item = 0u; item < p.n_items; ++item) {
                        extend_label(*current_label, item);
                    }                
                } else {
                    const std::size_t limit = std::min(
                        current_item + p.max_distance,
                        p.n_items - 1u
                    );

                    for(std::size_t item = current_item + 1u; item <= limit; ++item) {
                        extend_label(*current_label, item);
                    }
                }
            }

            // Using const_cast. I know that changing extended is not going
            // to change the std::set containing the label, but std::set
            // doesn't know, so it only returns const references from its
            // iterators.
            const_cast<Label&>(current_label->get()).extended = true;
        }

        const auto end_time = steady_clock::now();
        const auto time_elapsed = duration_cast<milliseconds>(end_time - start_time).count() / 1000.0;

        if(labels.find(Label::SINK) == labels.end() || labels[Label::SINK].empty()) {
            throw std::runtime_error("No label extended up to the sink within the time limit!");
        }

        auto optimal_it = std::min_element(
            labels[Label::SINK].begin(),
            labels[Label::SINK].end(),
            [] (const Label& l1, const Label& l2) -> bool {
                return l1.profit < l2.profit;
            }
        );

        std::vector<std::size_t> selected_items;
        const Label* current_label = &(*optimal_it);
        double weight_check = 0.0;
        double profit_check = 0.0;

        while(current_label != nullptr) {
            const auto ci = current_label->current_item;

            if(ci != Label::SOURCE && ci != Label::SINK) {
                selected_items.push_back(ci);
                weight_check += p.weights[ci];
                profit_check += p.profits[ci];
            }

            current_label = current_label->predecessor;
        }

        return LabellingSolution{
            /* .selected_items = */ selected_items,
            /* .profit = */ profit_check,
            /* .weight = */ weight_check,
            /* .time_elapsed = */ time_elapsed,
            /* .n_undominated_labels_at_sink = */ labels[Label::SINK].size()
        };
    }

    void Labelling::extend_label(const Label& label, std::size_t destination) {
        auto new_label = get_extension(label, destination);
        auto existing_labels = get_labels_at(destination);

        #ifdef DEBUG
            std::cout << "Extended to new label: " << new_label << "\n";
        #endif

        if(!existing_labels) {
            #ifdef DEBUG
                std::cout << "No label at destination " << destination << ": storing the new label\n";
            #endif

            store_label(new_label);
            return;
        }

        for(auto it = existing_labels->get().begin(); it != existing_labels->get().end();) {
            auto& label = *it;

            if(label.dominates(new_label)) {
                #ifdef DEBUG
                    std::cout << "New label dominated by " << label << ": deleting new label " << new_label << "\n";
                #endif

                return;
            }

            if(new_label.dominates(label)) {
                #ifdef DEBUG
                    std::cout << "New label dominates " << label << ": label deleted by new label " << new_label << "\n";
                #endif

                existing_labels->get().erase(it++);
                continue;
            }

            ++it;
        }

        store_label(new_label);
    }

     void Labelling::print_labels() const {
         for(const auto& [current_item, label_set] : labels) {
             std::cout << "=== " << label_set.size()
                       << " labels at item " << current_item
                       << " ===\n";

            for(const auto& label : label_set) {
                std::cout << label << "\n";
            }

            std::cout << "\n";
         }
     }
}