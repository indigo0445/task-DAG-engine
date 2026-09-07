#pragma once
#include "TaskNode.h"
#include <unordered_set>
#include <algorithm>
#include <stdexcept>

namespace DAGUtil {
    namespace {
        inline void get_descendants(TaskNode* n, std::unordered_set<TaskNode*>& descs) {
            for (auto succ : n->successors) {
                if (descs.contains(succ)) {
                    continue;
                }
                descs.insert(succ);
                get_descendants(succ, descs);
            }
        }
    }

    inline std::unordered_set<TaskNode*> get_descendants(TaskNode* n) {
        std::unordered_set<TaskNode*> descs = {n};
        get_descendants(n, descs);
        return descs;
    }

    // are these move semantics efficient?
    inline std::unordered_set<TaskNode*> get_descendants(std::vector<TaskNode*>& sources) {
        std::unordered_set<TaskNode*> descs(sources.begin(), sources.end());
        for (auto source : sources) {
            get_descendants(source, descs);
        }
        return descs;
    }

    inline int get_size(TaskNode* n) {
        return get_descendants(n).size();
    }

    inline int get_size(std::vector<TaskNode*>& sources) {
        return get_descendants(sources).size();
    }

    namespace {
        inline bool check_cycle_DFS(std::unordered_set<TaskNode*>& node_path,
                                    std::unordered_set<TaskNode*> verified_no_cycle,
                                    TaskNode* n) {
            if (verified_no_cycle.contains(n)) {
                return false;
            }
            if (node_path.contains(n)) {
                return true;
            }

            node_path.insert(n);
            for (auto succ : n->successors) {
                if (check_cycle_DFS(node_path, verified_no_cycle, succ)) {
                    return true; // no need to erase n; propagate trues
                }
            }
            node_path.erase(n);

            verified_no_cycle.insert(n);
            return false;
        }
    }

    inline bool check_cycle(std::vector<TaskNode*>& sources) {
        // returns true if HAS cycle
        // DFS to see if any paths loop back to itself; hard to monitor w/ BFS
        std::unordered_set<TaskNode*> node_path;
        std::unordered_set<TaskNode*> verified_no_cycle; // starting at node (reached from root), no cycle
        
        return std::ranges::any_of(sources, [&](auto source) {
            return check_cycle_DFS(node_path, verified_no_cycle, source);
        });
    }

    inline bool check_validity(std::vector<TaskNode*>& sources) {
        // verify all sources have no deps
        if (std::ranges::any_of(sources, [](TaskNode* source){ return source->pending_deps != 0; })) {
            throw std::invalid_argument("Invalid source: A provided source's in-degree is not 0; sources should not have dependencies");
            return false;
        }

        // check for cycles
        if (check_cycle(sources)) {
            throw std::invalid_argument("Not a DAG: Contains cycle");
            return false;
        }

        // add more later
        return true;
    }


    inline void check_performance() {
        // creates large tests and prints timing benchmraks

    }
}
