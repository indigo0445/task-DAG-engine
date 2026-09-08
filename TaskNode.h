#pragma once
#include <functional>
#include <unordered_set>
#include <ranges>
#include <iostream>
#include <memory>
#include <atomic>
#include <vector>
#include <optional>

struct TaskNode {
    // operate entirely on internal obj states
    std::function<void()> task;
    std::atomic<int> total_deps{0}; // for reusability
    std::atomic<int> pending_deps{0};
    int subgraph_size; // cache the size for pqueue comparator
    std::unordered_set<TaskNode*> successors; // immediate children
    
    template <typename Callable>
    TaskNode (Callable&& c): task(std::forward<Callable>(c)) {}

    template <typename Callable>
    TaskNode (Callable&& c, TaskNode& dep)
        : task(std::forward<Callable>(c)) {
            dep.directs_to(*this);
        }

    template <typename Callable>
    TaskNode (Callable&& c, std::vector<std::reference_wrapper<TaskNode>> deps)
        : task(std::forward<Callable>(c)) {
            add_deps(deps); // is this hard copy?
        }

    void directs_to(TaskNode& succ) {
        // if already directed, do nothing (could consider throwing an error?)
        if (successors.contains(&succ)) {
            return;
        }
        successors.insert(&succ);
        succ.total_deps++;
        succ.pending_deps++;
    }

    void directs_to(std::vector<std::reference_wrapper<TaskNode>> succs) {
        std::for_each(succs.begin(), succs.end(), [this](auto& succ){ return directs_to(succ); });
    }

    void add_deps(TaskNode& dep) {
        dep.directs_to(*this);
    }

    void add_deps(std::vector<std::reference_wrapper<TaskNode>> deps) {
        std::for_each(deps.begin(), deps.end(), [this](auto& dep){ return add_deps(dep); });
    }

    void del_successors(TaskNode& succ) {
        // if not directed, do nothing (could consider throwing an error?)
        if (!successors.contains(&succ)) {
            return;
        }
        successors.erase(&succ);
        succ.total_deps--;
        succ.pending_deps--;
    }

    void del_successors(std::vector<std::reference_wrapper<TaskNode>> succs) {
        std::for_each(succs.begin(), succs.end(), [this](auto& succ){ return del_successors(succ); });
    }

    void del_deps(TaskNode& deps) {
        deps.del_successors(*this);
    }

    void del_deps(std::vector<std::reference_wrapper<TaskNode>> deps) {
        std::for_each(deps.begin(), deps.end(), [this](auto& dep){ return del_deps(dep); });
    }

    void reset_pending_deps() {
        pending_deps = total_deps.load();
    }
};
