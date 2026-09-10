#pragma once
// custom threadpool implementation for this task
#include <thread>
#include <memory>
#include "TaskNode.h"
#include "DAGUtil.h"
#include <iostream>
#include <queue>
#include <mutex>
#include <functional>
#include <stdexcept>
#include <vector>
#include <semaphore>
#include <chrono>
#include <format>
#include <string>
#include <random>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <unordered_set>
#include <cstdint>

struct CompareNodes {
    bool operator()(const TaskNode* n1, const TaskNode* n2) {
        // compare cached sizes
        return n1->subgraph_size < n2->subgraph_size;
    }
};

class ThreadPoolExe {
private:
    int num_threads;
    std::atomic<int> num_nodes_left;
    std::unique_ptr<std::jthread[]> pool;
    std::counting_semaphore<10'000'000> new_task{0}; // releases whenever new item in q
    std::priority_queue<TaskNode*, std::vector<TaskNode*>, CompareNodes> node_q;
    std::mutex q_mtx;
    std::binary_semaphore all_completed{0};
    std::atomic<int> busy_workers{0};
    std::atomic<std::uint64_t> epoch{0};
    bool stopping;
    bool verbose;

    void worker_loop() {
        while (true) {
            if (stopping) {
                return;
            }
            std::unique_lock<std::mutex> lock(q_mtx);
            if (!node_q.empty()) {
                auto node = node_q.top();
                node_q.pop();
                // count as busy before dropping the lock so wait() can't
                // miss a popped node that hasn't started yet
                auto my_epoch = epoch.load();
                busy_workers++;
                lock.unlock();

                try {
                    node->task();
                } catch (const std::runtime_error& e) {
                    std::cout << "A task failed\n";
                }

                if (epoch.load() == my_epoch) {
                    // fetch_sub==1 is the thread that took the count from 1 to 0.
                    // `--` then `== 0` can let two finishers both see 0 (the other
                    // already decremented) and double-release the binary_semaphore
                    if (num_nodes_left.fetch_sub(1) == 1) {
                        all_completed.release();
                    }
                }

                // update successors and see if any are ready
                for (auto succ : node->successors) {
                    if (epoch.load() != my_epoch) {
                        break; // run ended; don't touch a graph that's being reset/freed
                    }
                    // pending_deps can be negative
                    if (succ->pending_deps.fetch_sub(1) == 1) {
                        if (epoch.load() != my_epoch) {
                            break;
                        }
                        lock.lock();
                        node_q.push(succ);
                        lock.unlock();
                        new_task.release();
                    }
                }
                busy_workers--;
            } else {
                lock.unlock();
                new_task.acquire();
            }
        }
    }

public:
    ThreadPoolExe() {
        num_threads = std::thread::hardware_concurrency();
        stopping = false;
        verbose = true;
        pool = std::make_unique<std::jthread[]>(num_threads); // fills w/ default constructor
        for (int i = 0; i < num_threads; i++) {
            pool[i] = std::jthread(&ThreadPoolExe::worker_loop, this);
        }
        std::cout << "Will run on " << num_threads << " hardware threads\n";
    }

    void set_verbose(bool v) {
        verbose = v;
    }

    void compute_DAG(TaskNode& source) {
        compute_DAG(std::vector<std::reference_wrapper<TaskNode>>{source});
    }

    void compute_DAG(std::vector<std::reference_wrapper<TaskNode>> sources) {
        // takes in references for convenience, to avoid typing &
        auto sources_ptrs = sources | std::views::transform([](TaskNode& source){ return &source; })
                                    | std::ranges::to<std::vector<TaskNode*>>();

        epoch.fetch_add(1); // new run; in-flight workers from the last dag must not mutate this one
        while (all_completed.try_acquire()) {
            // leftover completion from a previous run
        }
        DAGUtil::reset_graph_state(sources_ptrs);
        DAGUtil::check_validity(sources_ptrs);
        num_nodes_left = DAGUtil::get_size(sources_ptrs);
        {
            std::lock_guard<std::mutex> lock(q_mtx);
            while (!node_q.empty()) {
                node_q.pop();
            }
            node_q.push_range(sources_ptrs);
        }
        
        if (verbose) {
            std::cout << "Running task DAG with " << num_nodes_left << " nodes\n";
        }
        
        new_task.release(sources.size());
    }

    void wait() {
        all_completed.acquire();
        // last worker may still be in the successor loop; don't reset the
        // same graph (or destroy it) until nobody is touching nodes
        while (busy_workers.load() != 0) {
            std::this_thread::yield();
        }
        epoch.fetch_add(1); // drop any straggler successor updates
        std::lock_guard<std::mutex> lock(q_mtx);
        while (!node_q.empty()) {
            node_q.pop();
        }
    }

    void stop_workers() {
        // does not interrupt if thread is working; I don't think C++ can interrupt a working thread?
        stopping = true;
        for (int i = 0; i < num_threads; i++) {
            new_task.release();
        }
    }

    static void check_performance();
};

namespace {
    using clock = std::chrono::steady_clock;

    // cheap busy-loop so wide graphs can actually use the pool; not a real workload
    inline void busy_work() {
        volatile unsigned x = 1;
        for (int i = 0; i < 2000; i++) {
            x = x * 1664525u + 1013904223u;
        }
    }

    struct BenchGraph {
        std::vector<std::unique_ptr<TaskNode>> storage;
        std::vector<TaskNode*> all;
        std::vector<TaskNode*> sources;
        std::string name;

        TaskNode* add() {
            storage.push_back(std::make_unique<TaskNode>(busy_work));
            all.push_back(storage.back().get());
            return all.back();
        }

        void pick_sources() {
            // in-degree 0
            sources.clear();
            std::ranges::for_each(all, [this](auto* n) {
                if (n->total_deps == 0) {
                    sources.push_back(n);
                }
            });
        }

        int n() const {
            return static_cast<int>(all.size());
        }

        int edge_count() const {
            int e = 0;
            std::ranges::for_each(all, [&](auto* n) {
                e += static_cast<int>(n->successors.size());
            });
            return e;
        }

        std::vector<std::reference_wrapper<TaskNode>> source_refs() {
            std::vector<std::reference_wrapper<TaskNode>> refs;
            refs.reserve(sources.size());
            for (auto* s : sources) {
                refs.emplace_back(*s);
            }
            return refs;
        }
    };

    struct Stats {
        double min_ms{};
        double median_ms{};
        double p95_ms{};
        double mean_ms{};
        double stdev_ms{};
        int batch{1};
        int samples{0};
    };

    inline void run_once(ThreadPoolExe& exe, BenchGraph& g) {
        exe.compute_DAG(g.source_refs());
        exe.wait();
    }

    inline Stats measure_exe(ThreadPoolExe& exe, BenchGraph& g) {
        // warmup; also proves the graph can be reset and rerun
        run_once(exe, g);
        run_once(exe, g);
        run_once(exe, g);

        auto t0 = clock::now();
        run_once(exe, g);
        auto probe_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now() - t0).count();

        int batch = 1;
        if (probe_ns > 0) {
            batch = static_cast<int>(std::max(8'000'000.0 / static_cast<double>(probe_ns), 1.0));
            batch = std::min(batch, 32); // don't rerun a huge dag 200k times
        }

        const int num_samples = 11;
        std::vector<double> samples;
        samples.reserve(num_samples);
        for (int s = 0; s < num_samples; s++) {
            auto a = clock::now();
            for (int i = 0; i < batch; i++) {
                run_once(exe, g);
            }
            auto b = clock::now();
            samples.push_back(std::chrono::duration<double, std::milli>(b - a).count() / batch);
        }
        std::ranges::sort(samples);

        Stats st;
        st.min_ms = samples.front();
        st.median_ms = samples[samples.size() / 2];
        st.p95_ms = samples[static_cast<std::size_t>(0.95 * (samples.size() - 1))];
        st.mean_ms = std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();
        double var = 0;
        std::ranges::for_each(samples, [&](double x) {
            var += (x - st.mean_ms) * (x - st.mean_ms);
        });
        st.stdev_ms = std::sqrt(var / samples.size());
        st.batch = batch;
        st.samples = num_samples;
        return st;
    }

    inline std::string fmt_time(double ms) {
        if (ms < 0.001) {
            return std::format("{:7.1f} ns", ms * 1'000'000.0);
        }
        if (ms < 1.0) {
            return std::format("{:7.2f} us", ms * 1'000.0);
        }
        return std::format("{:7.3f} ms", ms);
    }

    inline BenchGraph make_chain(int n) {
        BenchGraph g;
        g.name = std::format("chain n={}", n);
        for (int i = 0; i < n; i++) {
            g.add();
        }
        for (int i = 0; i < n - 1; i++) {
            g.all[i]->directs_to(*g.all[i + 1]);
        }
        g.pick_sources();
        return g;
    }

    inline BenchGraph make_star(int n) {
        BenchGraph g;
        g.name = std::format("star n={}", n);
        auto* root = g.add();
        for (int i = 1; i < n; i++) {
            root->directs_to(*g.add());
        }
        g.pick_sources();
        return g;
    }

    inline BenchGraph make_binary_tree(int n) {
        BenchGraph g;
        g.name = std::format("bin_tree n={}", n);
        for (int i = 0; i < n; i++) {
            g.add();
        }
        for (int i = 0; i < n; i++) {
            int l = 2 * i + 1;
            int r = 2 * i + 2;
            if (l < n) {
                g.all[i]->directs_to(*g.all[l]);
            }
            if (r < n) {
                g.all[i]->directs_to(*g.all[r]);
            }
        }
        g.pick_sources();
        return g;
    }

    inline BenchGraph make_funnel(int n) {
        // inverted tree; lots of sources can start at once, then join
        BenchGraph g;
        g.name = std::format("funnel n={}", n);
        for (int i = 0; i < n; i++) {
            g.add();
        }
        for (int i = 1; i < n; i++) {
            g.all[i]->directs_to(*g.all[(i - 1) / 2]);
        }
        g.pick_sources();
        return g;
    }

    inline BenchGraph make_grid(int side) {
        // (x,y) -> (x+1,y) and (x,y+1); lots of join points
        BenchGraph g;
        int n = side * side;
        g.name = std::format("grid {}x{}", side, side);
        for (int i = 0; i < n; i++) {
            g.add();
        }
        auto at = [&](int x, int y) -> TaskNode& { return *g.all[y * side + x]; };
        for (int y = 0; y < side; y++) {
            for (int x = 0; x < side; x++) {
                if (x + 1 < side) {
                    at(x, y).directs_to(at(x + 1, y));
                }
                if (y + 1 < side) {
                    at(x, y).directs_to(at(x, y + 1));
                }
            }
        }
        g.pick_sources();
        return g;
    }

    inline BenchGraph make_layered(int layers, int width, int out_deg, std::uint32_t seed) {
        BenchGraph g;
        g.name = std::format("layered {}x{} d={}", layers, width, out_deg);
        std::vector<std::vector<TaskNode*>> layer(layers);
        for (int L = 0; L < layers; L++) {
            layer[L].reserve(width);
            for (int i = 0; i < width; i++) {
                layer[L].push_back(g.add());
            }
        }
        std::mt19937 rng(seed);
        std::uniform_int_distribution<int> dist(0, width - 1);
        for (int L = 0; L < layers - 1; L++) {
            for (auto* u : layer[L]) {
                std::unordered_set<int> seen;
                int deg = std::min(out_deg, width);
                while (static_cast<int>(seen.size()) < deg) {
                    int j = dist(rng);
                    if (seen.insert(j).second) {
                        u->directs_to(*layer[L + 1][j]);
                    }
                }
            }
        }
        g.pick_sources();
        return g;
    }

    inline BenchGraph make_random_sparse(int n, int avg_out, std::uint32_t seed) {
        BenchGraph g;
        g.name = std::format("random n={} out={} seed={}", n, avg_out, seed);
        for (int i = 0; i < n; i++) {
            g.add();
        }
        std::mt19937 rng(seed);
        for (int i = 0; i < n - 1; i++) {
            int remaining = n - 1 - i;
            int deg = std::min(avg_out, remaining);
            std::uniform_int_distribution<int> dist(i + 1, n - 1);
            std::unordered_set<int> seen;
            while (static_cast<int>(seen.size()) < deg) {
                int j = dist(rng);
                if (seen.insert(j).second) {
                    g.all[i]->directs_to(*g.all[j]);
                }
            }
        }
        g.pick_sources();
        return g;
    }

    inline BenchGraph make_parallel_join(int branches, int length) {
        // one source, independent pipelines, then a join; like the example.cpp shape
        BenchGraph g;
        g.name = std::format("par_join {}x{}", branches, length);
        auto* source = g.add();
        std::vector<TaskNode*> tails;
        tails.reserve(branches);
        for (int b = 0; b < branches; b++) {
            TaskNode* prev = source;
            for (int i = 0; i < length; i++) {
                auto* cur = g.add();
                prev->directs_to(*cur);
                prev = cur;
            }
            tails.push_back(prev);
        }
        auto* join = g.add();
        std::ranges::for_each(tails, [&](auto* t) {
            t->directs_to(*join);
        });
        g.pick_sources();
        return g;
    }
}

inline void ThreadPoolExe::check_performance() {
    // run the pool on a bunch of shapes/sizes; time compute_DAG + wait, not the builders
    std::cout << "ThreadPoolExe::check_performance\n"
              << "  times compute_DAG + wait on the same graph (reset between runs)\n"
              << "  each node does a short busy-loop so width can actually use the workers\n"
              << "  clock: steady_clock | samples: 11 batched | prefer -O2\n"
              << std::flush;

    ThreadPoolExe exe;
    exe.set_verbose(false);

    const std::uint32_t seed_a = 42;
    const std::uint32_t seed_b = 1337;
    const std::uint32_t seed_c = 20260908; // date this line was written

    const std::function<BenchGraph()> specs[] = {
        // deep vs wide: chain is almost serial; star fans out immediately
        [] { return make_chain(200); },
        [] { return make_chain(2000); },
        [] { return make_chain(8000); },
        [] { return make_star(200); },
        [] { return make_star(2000); },
        [] { return make_star(15000); },
        [] { return make_binary_tree(511); },
        [] { return make_binary_tree(4095); },
        [] { return make_binary_tree(8191); },
        [] { return make_funnel(200); },
        [] { return make_funnel(2000); },
        [] { return make_funnel(8000); },
        [] { return make_grid(16); },
        [] { return make_grid(40); },
        [] { return make_grid(80); },
        [] { return make_layered(8, 40, 3, seed_a); },
        [] { return make_layered(20, 80, 3, seed_a); },
        [] { return make_layered(30, 100, 4, seed_a); },
        [] { return make_parallel_join(8, 40); },
        [] { return make_parallel_join(20, 80); },
        [] { return make_parallel_join(40, 150); },
        // a few seeds at the mid size so one unlucky draw isn't the result
        [] { return make_random_sparse(500, 3, seed_a); },
        [] { return make_random_sparse(4000, 3, seed_a); },
        [] { return make_random_sparse(4000, 3, seed_b); },
        [] { return make_random_sparse(4000, 3, seed_c); },
        [] { return make_random_sparse(10000, 3, seed_a); },
        [] { return make_random_sparse(10000, 8, seed_a); },
    };

    std::cout << std::format("\n  {:<34} {:>8} {:>8} {:>6} {:>12} {:>12} {:>12} {:>8} {:>10}\n",
                             "graph", "n", "edges", "srcs", "median", "min", "p95", "batch", "ns/node")
              << std::flush;

    // keep every dag alive until the pool stops; a late worker must not
    // fetch_sub into freed TaskNodes that the allocator reused
    std::vector<BenchGraph> keep_alive;
    keep_alive.reserve(std::size(specs));

    for (const auto& make : specs) {
        keep_alive.push_back(make());
        auto& g = keep_alive.back();
        if (g.sources.empty()) {
            std::cout << std::format("  skip {}: no sources\n", g.name);
            continue;
        }

        auto st = measure_exe(exe, g);
        double ns_per = st.median_ms * 1'000'000.0 / g.n();
        std::cout << std::format("  {:<34} {:>8} {:>8} {:>6} {:>12} {:>12} {:>12} {:>8} {:>10.1f}\n",
                                 g.name,
                                 g.n(),
                                 g.edge_count(),
                                 g.sources.size(),
                                 fmt_time(st.median_ms),
                                 fmt_time(st.min_ms),
                                 fmt_time(st.p95_ms),
                                 st.batch,
                                 ns_per)
                  << std::flush;
    }

    exe.stop_workers();
    std::cout << "\ndone.\n";
}
