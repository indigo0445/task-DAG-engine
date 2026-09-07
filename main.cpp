#include <iostream>
#include <syncstream>
#include <vector>
#include <numeric>
#include "TaskNode.h"
#include "ThreadPoolExe.h"

int main() {
    std::vector<long long> data;
    long long median;
    long long sum;

    TaskNode n1([&data]() {
        const int len = 100'000'000;
        // fibonacci
        data.reserve(len);
        data.push_back(0);
        data.push_back(1);
        for (int i = 0; i < len - 2; i++) {
            data.push_back(data.back() + data[data.size() - 2]);
        }
        std::osyncstream(std::cout) << "built sequence\n";
    });

    TaskNode n2([&data, &median]() {
        std::vector<long long> data_copy = data;

        auto median_it = data_copy.begin() + data_copy.size() / 2;
        std::nth_element(data_copy.begin(), median_it, data_copy.end());
        median = *median_it;

        std::osyncstream(std::cout) << "median: " << median << '\n';
    }, n1);
    
    TaskNode n3([&data, &sum]() {
        sum = std::accumulate(data.begin(), data.end(), 0LL);
        std::osyncstream(std::cout) << "sum: " << sum << '\n';
    }, n1);

    TaskNode n4([median, sum]() {
        std::osyncstream(std::cout) << "sum * median: " << sum * median << '\n';
    }, {n2, n3});


    ThreadPoolExe exe;
    exe.compute_DAG(n1);
    exe.wait();

    exe.stop_workers();
}
