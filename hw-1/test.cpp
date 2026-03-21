#include "dfs.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {

bool ContainsAllVerticesExactlyOnce(const std::vector<int>& order, int vertex_count) {
    if (static_cast<int>(order.size()) != vertex_count) {
        return false;
    }

    std::vector<int> sorted = order;
    std::sort(sorted.begin(), sorted.end());

    for (int i = 0; i < vertex_count; ++i) {
        if (sorted[i] != i) {
            return false;
        }
    }
    return true;
}

}

TEST(CooperativeDFSTest, EmptyGraphReturnsEmptyOrder) {
    Graph graph;

    const std::vector<int> order = CooperativeDFS(graph);

    EXPECT_TRUE(order.empty());
}

TEST(CooperativeDFSTest, SingleVertexGraph) {
    Graph graph;
    graph.adj = {{}};

    const std::vector<int> order = CooperativeDFS(graph);

    ASSERT_EQ(order.size(), 1u);
    EXPECT_EQ(order[0], 0);
}

TEST(CooperativeDFSTest, LineGraphVisitsEveryVertexExactlyOnce) {
    Graph graph;
    graph.adj = {
        {1},
        {0, 2},
        {1, 3},
        {2}
    };

    const std::vector<int> order = CooperativeDFS(graph);

    EXPECT_TRUE(ContainsAllVerticesExactlyOnce(order, 4));
}

TEST(CooperativeDFSTest, DisconnectedGraphStillVisitsAllVertices) {
    Graph graph;
    graph.adj = {
        {1},
        {0},
        {3},
        {2},
        {}
    };

    const std::vector<int> order = CooperativeDFS(graph);

    EXPECT_TRUE(ContainsAllVerticesExactlyOnce(order, 5));
}

TEST(CooperativeDFSTest, CycleGraphVisitsEachVertexOnce) {
    Graph graph;
    graph.adj = {
        {1, 3},
        {0, 2},
        {1, 3},
        {2, 0}
    };

    const std::vector<int> order = CooperativeDFS(graph);

    EXPECT_TRUE(ContainsAllVerticesExactlyOnce(order, 4));
}

TEST(CooperativeDFSTest, LoggingProducesTraceMessages) {
    Graph graph;
    graph.adj = {
        {1},
        {0}
    };

    std::ostringstream out;
    const std::vector<int> order = CooperativeDFS(graph, &out);

    EXPECT_TRUE(ContainsAllVerticesExactlyOnce(order, 2));
    EXPECT_NE(out.str().find("enter vertex"), std::string::npos);
    EXPECT_NE(out.str().find("inspect edge"), std::string::npos);
}

TEST(CooperativeDFSTest, InvalidAdjacentVertexThrows) {
    Graph graph;
    graph.adj = {
        {1},
        {2}
    };

    EXPECT_THROW(
        {
            const auto order = CooperativeDFS(graph);
            (void)order;
        },
        std::out_of_range
    );
}
