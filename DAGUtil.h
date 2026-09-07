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
    inline std::unordered_set<TaskNode*> get_descendants(std::vector<TaskNode*> sources) {
        std::unordered_set<TaskNode*> descs(sources.begin(), sources.end());
        for (auto source : sources) {
            get_descendants(source, descs);
        }
        return descs;
    }

    inline int get_size(TaskNode* n) {
        return get_descendants(n).size();
    }

    inline int get_size(std::vector<TaskNode*> sources) {
        return get_descendants(sources).size();
    }

    inline bool check_validity(std::vector<TaskNode*> sources) {
        // verify all sources have no deps
        if (std::ranges::any_of(sources, [](TaskNode* source){ return source->pending_deps != 0; })) {
            throw std::invalid_argument("DAG has an invalid source: in-degree not 0");
            return false;
        }
        // add more later
        return true;
    }
}
