#pragma once

#include <iosfwd>
#include <vector>

struct Graph {
    std::vector<std::vector<int>> adj;
};

std::vector<int> CooperativeDFS(const Graph& graph, std::ostream* log = nullptr);
