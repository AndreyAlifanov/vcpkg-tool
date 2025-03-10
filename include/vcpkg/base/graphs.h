#pragma once

#include <vcpkg/base/fwd/graphs.h>

#include <vcpkg/base/checks.h>
#include <vcpkg/base/lineinfo.h>
#include <vcpkg/base/messages.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace vcpkg
{
    template<class V, class U>
    struct AdjacencyProvider
    {
        virtual std::vector<V> adjacency_list(const U& vertex) const = 0;
        virtual U load_vertex_data(const V& vertex) const = 0;
    };

    struct GraphRandomizer
    {
        virtual int random(int max_exclusive) = 0;

    protected:
        ~GraphRandomizer() = default;
    };

    namespace details
    {
        template<class Container>
        void shuffle(Container& c, GraphRandomizer* r)
        {
            if (!r) return;
            for (auto i = c.size(); i > 1; --i)
            {
                std::size_t j = r->random(static_cast<int>(i));
                if (j != i - 1)
                {
                    std::swap(c[i - 1], c[j]);
                }
            }
        }

        template<class V, class U>
        void topological_sort_internal(const V& vertex,
                                       const AdjacencyProvider<V, U>& f,
                                       std::unordered_map<V, ExplorationStatus>& exploration_status,
                                       std::vector<U>& sorted,
                                       GraphRandomizer* randomizer)
        {
            ExplorationStatus& status = exploration_status[vertex];
            switch (status)
            {
                case ExplorationStatus::FULLY_EXPLORED: return;
                case ExplorationStatus::PARTIALLY_EXPLORED:
                {
                    msg::println(msgGraphCycleDetected, msg::package_name = vertex);
                    for (auto&& node : exploration_status)
                    {
                        if (node.second == ExplorationStatus::PARTIALLY_EXPLORED)
                        {
                            msg::println(LocalizedString().append_indent().append_raw(node.first.to_string()));
                        }
                    }
                    Checks::exit_fail(VCPKG_LINE_INFO);
                }
                case ExplorationStatus::NOT_EXPLORED:
                {
                    status = ExplorationStatus::PARTIALLY_EXPLORED;
                    U vertex_data = f.load_vertex_data(vertex);
                    auto neighbours = f.adjacency_list(vertex_data);
                    details::shuffle(neighbours, randomizer);
                    for (const V& neighbour : neighbours)
                        topological_sort_internal(neighbour, f, exploration_status, sorted, randomizer);

                    sorted.push_back(std::move(vertex_data));
                    status = ExplorationStatus::FULLY_EXPLORED;
                    return;
                }
                default: Checks::unreachable(VCPKG_LINE_INFO);
            }
        }
    }

    template<class Range, class V, class U>
    std::vector<U> topological_sort(Range starting_vertices,
                                    const AdjacencyProvider<V, U>& f,
                                    GraphRandomizer* randomizer)
    {
        std::vector<U> sorted;
        std::unordered_map<V, ExplorationStatus> exploration_status;

        details::shuffle(starting_vertices, randomizer);

        for (auto&& vertex : starting_vertices)
        {
            details::topological_sort_internal(vertex, f, exploration_status, sorted, randomizer);
        }

        return sorted;
    }
}
