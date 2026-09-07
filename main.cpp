#include <bits/stdc++.h>
#include "TaskNode.h"
#include "ThreadPoolExe.h"

int main() {
    auto vec = std::make_shared<std::vector<int>>();
    auto sz = std::make_shared<int>(0);

    TaskNode n1([vec]() {
        vec->push_back(1);
        vec->push_back(2);
        vec->push_back(3);
    }); // no return type, no inputs
    
    TaskNode n2([vec, sz]() {
       *sz = vec->size(); 
    });

    n1.directs_to(&n2);

    std::cout << *sz << '\n';

    ThreadPoolExe exe;
    exe.compute_DAG(&n1);
}
