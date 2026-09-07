#include <iostream>
#include <syncstream>
#include <vector>
#include <numeric>
#include "TaskNode.h"
#include "ThreadPoolExe.h"

int main() {
    auto vec = std::make_shared<std::vector<int>>();
    auto sz = std::make_shared<int>(0);
    auto sum = std::make_shared<int>(0);

    TaskNode n1([vec]() {
        vec->push_back(1);
        vec->push_back(2);
        vec->push_back(3);
    });
    
    TaskNode n2([vec, sz]() {
       *sz = vec->size(); 
       std::osyncstream(std::cout) << "len: " << *sz << '\n';
    }, n1);

    TaskNode n3([vec, sum]() {
       *sum = std::accumulate(vec->begin(), vec->end(), 0); 
       std::osyncstream(std::cout) << "sum: " << *sum << '\n';
    }, n1);

    TaskNode n4([sum, sz]() {
        std::osyncstream(std::cout) << "sum * len: " << (*sum) * (*sz) << '\n';
    }, {n2, n3});

    ThreadPoolExe exe;
    exe.compute_DAG({&n1});

    exe.stop_workers();
}
