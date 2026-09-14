#pragma once

#include "AtlasNet/Core/Network/Topology/TransitionPlan.hpp"
#include "AtlasNet/Core/Network/Topology/TopologyCommons.hpp"
namespace AtlasNet::Network::Topology
{
class TopologyTransitionPlanner
{
public:
  TransitionPlan Compute(const ConnectionGraph& current,
                       const ConnectionGraph& desired)
{
    TransitionPlan plan;

    ConnectionGraph working = current;


    using EdgePair = std::pair<AtlasNetNodeID, AtlasNetNodeID>;

    auto Normalize = [](AtlasNetNodeID a, AtlasNetNodeID b)
    {
        return std::minmax(a,b);
    };


    auto BuildEdgeSet = [](const ConnectionGraph& graph)
    {
        std::set<EdgePair> edges;

        for(auto [a,b] : graph.GetConnections())
            edges.insert({a,b});

        return edges;
    };


    auto currentEdges = BuildEdgeSet(current);
    auto desiredEdges = BuildEdgeSet(desired);


    std::set<EdgePair> toAdd;
    std::set<EdgePair> toRemove;


    std::set_difference(
        desiredEdges.begin(),
        desiredEdges.end(),
        currentEdges.begin(),
        currentEdges.end(),
        std::inserter(toAdd,toAdd.end()));


    std::set_difference(
        currentEdges.begin(),
        currentEdges.end(),
        desiredEdges.begin(),
        desiredEdges.end(),
        std::inserter(toRemove,toRemove.end()));



    while(!toAdd.empty())
    {
        auto edge = *toAdd.begin();
        toAdd.erase(toAdd.begin());


        working.AddEdge(edge.first, edge.second);


        TransitionStep step;

        step.connect.push_back({edge.first, edge.second});



        //
        // BFS through current graph
        //

        std::unordered_map<
            AtlasNetNodeID,
            AtlasNetNodeID> parent;


        std::queue<AtlasNetNodeID> q;


        parent[edge.first] = edge.first;
        q.push(edge.first);


        bool found = false;


        while(!q.empty() && !found)
        {
            auto node = q.front();
            q.pop();


           /*  for(auto neighbor : working.GetNeighbors(node))
            {
                auto ids = Normalize(node, neighbor);


                // ignore newly created edge
                if(ids == edge)
                    continue;


                if(parent.contains(neighbor))
                    continue;


                parent[neighbor] = node;


                if(neighbor == edge.second)
                {
                    found=true;
                    break;
                }


                q.push(neighbor);
            } */
        }



        //
        // remove obsolete edge on cycle
        //

        auto cur = edge.second;


        while(cur != edge.first)
        {
            auto p = parent[cur];

            auto ids = Normalize(p,cur);


            if(toRemove.contains(ids))
            {
                working.RemoveEdge(
                    ids.first,
                    ids.second);


                step.disconnect.push_back({ids.first, ids.second});

                toRemove.erase(ids);

                break;
            }


            cur=p;
        }


        plan.PushStep(std::move(step));
    }



    for(auto edge : toRemove)
    {
        working.RemoveEdge(
            edge.first,
            edge.second);


        TransitionStep step;

        step.disconnect.push_back({edge.first, edge.second});

        plan.PushStep(std::move(step));
    }


    return plan;
}
};
} // namespace AtlasNet::Network::Topology