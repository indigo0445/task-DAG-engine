#pragma once
#include "TaskNode.h"
#include <unordered_set>
#include <algorithm>
#include <stdexcept>

namespace ranges = std::ranges;

namespace DAGUtil {
    namespace {
        // descs holds all visited descendants of sources
        inline void get_descendants_DFS(const TaskNode* n, std::unordered_set<const TaskNode*>& descs) {
            for (auto succ : n->successors) {
                if (descs.contains(succ)) {
                    continue;
                }
                descs.insert(succ);
                get_descendants_DFS(succ, descs);
            }
        }
    }

    inline std::unordered_set<const TaskNode*> get_descendants(const TaskNode* n) {
        std::unordered_set<const TaskNode*> descs = {n};
        get_descendants_DFS(n, descs);
        return descs;
    }

    // are these move semantics efficient?
    inline std::unordered_set<const TaskNode*> get_descendants(std::vector<TaskNode*>& sources) {
        std::unordered_set<const TaskNode*> descs(sources.begin(), sources.end());
        ranges::for_each(sources, [&](const auto& source) {
            get_descendants_DFS(source, descs);
        });
        return descs;
    }

    namespace {
        // also updates all subgraph size caches in TaskNodes
        inline int get_size_DFS(std::unordered_map<TaskNode*, int>& visited_cache, TaskNode* n) {
            if (visited_cache.contains(n)) {
                return visited_cache[n];
            }
            // assume valid DAG, so won't loop back while DFS; won't update visited here
            auto sizes = n->successors | std::views::transform([&](TaskNode* succ) {
                return get_size_DFS(visited_cache, succ);
            });
            auto size = std::accumulate(sizes.begin(), sizes.end(), 0);
            visited_cache[n] = size;
            n->subgraph_size = size;
            return size;
        }
    }

    inline int get_size(TaskNode* n) {
        std::unordered_map<TaskNode*, int> visited_cache;
        return get_size_DFS(visited_cache, n);
    }

    inline int get_size(std::vector<TaskNode*>& sources) {
        std::unordered_map<TaskNode*, int> visited_cache;

        ranges::for_each(sources, [&visited_cache](auto& source) {
            // result unused since sum of these will overcount
            get_size_DFS(visited_cache, source);
        });
        return visited_cache.size();
    }

    namespace {
        inline void reset_graph_state_DFS(std::unordered_set<TaskNode*>& visited,
                                          TaskNode* n) {
            if (visited.contains(n)) {
                return;
            }
            
            visited.insert(n);
            ranges::for_each(n->successors, [&visited](const auto& succ) {
                reset_graph_state_DFS(visited, succ);
            });
        }
    }

    inline void reset_graph_state(std::vector<TaskNode*>& sources) {
        // resets pending_deps counter on all nodes
        std::unordered_set<TaskNode*> visited;
        ranges::for_each(sources, [&visited](const auto& source) {
            reset_graph_state_DFS(visited, source);
        });
    }

    namespace {
        inline bool graph_any_of_DFS(std::unordered_set<TaskNode*>& visited,
                                     std::function<bool(TaskNode*)> pred, TaskNode* n) {
            if (visited.contains(n)) {
                return false;
            }
            if (pred(n)) {
                return true;
            }

            visited.insert(n);
            return ranges::any_of(n->successors, [&](const auto& succ) {
                return graph_any_of_DFS(visited, pred, succ);
            });
        }
    }

    inline bool graph_any_of(std::vector<TaskNode*>& sources, std::function<bool(TaskNode*)> pred) {
        // checks if pred is true for any of DAG
        std::unordered_set<TaskNode*> visited;
        return ranges::any_of(sources, [&](const auto& source) {
            return graph_any_of_DFS(visited, pred, source);
        });
    }

    namespace {
        inline bool check_cycle_DFS(std::unordered_set<TaskNode*>& node_path,
                                    std::unordered_set<TaskNode*>& verified_no_cycle,
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
        // could consider switching to Kahn's alg
        std::unordered_set<TaskNode*> node_path;
        std::unordered_set<TaskNode*> verified_no_cycle; // starting at node (reached from root), no cycle
        
        return ranges::any_of(sources, [&](auto source) {
            return check_cycle_DFS(node_path, verified_no_cycle, source);
        });
    }

    inline bool check_validity(std::vector<TaskNode*>& sources) {
        // verify all sources have no deps
        if (ranges::any_of(sources, [](const TaskNode* source){ return source->pending_deps != 0; })) {
            throw std::invalid_argument("Invalid source: A provided source's in-degree is not 0; sources should not have dependencies");
            return false;
        }

        // check for cycles
        if (check_cycle(sources)) {
            throw std::invalid_argument("Not a DAG: Contains cycle");
            return false;
        }

        // check for pending_deps == total_deps?
        if (graph_any_of(sources, [](const TaskNode* n){ return n->pending_deps != n->total_deps; })) {
            throw std::invalid_argument("Graph is not reset; Consider calling reset_graph_state");
            return false;
        }


        // add more later
        return true;
    }


    inline void check_performance() {
        // creates large tests and prints timing benchmraks

    }
}
