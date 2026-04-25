#ifndef GRAPH_RENDER_H
#define GRAPH_RENDER_H

// returns this node's id so parent can draw edge to it
#include "../include/dataframelib/Types.h"
#include <iostream>
#include "../include/dataframelib/lazy/nodes.h"

namespace dataframelib
{
    // Forward declaration — needed because lambda calls writeDot recursively
    int writeDot(std::ostream &out, const planNode &node, int &nodeId);

    int writeDot(std::ostream &out, const planNode &node, int &nodeId)
    {
        int myId = nodeId++;

        std::visit([&out, &nodeId, myId](const auto &n) -> void // capture all needed vars
                   {
        using NodeType = std::decay_t<decltype(n)>;

        if constexpr (std::is_same_v<NodeType, ScanNode>)
        {
            out << "  node" << myId
                << " [label=\"Scan\\n"
                << n.file_path << "\", fillcolor=lightgreen];\n";
        }
        else if constexpr (std::is_same_v<NodeType, FilterNode>)
        {
            out << "  node" << myId
                << " [label=\"Filter\\n"
                << n.predicate.toString() << "\"];\n";
            int childId = writeDot(out, *n.child, nodeId);
            out << "  node" << myId << " -> node" << childId << ";\n";
        }
        else if constexpr (std::is_same_v<NodeType, SelectNode>)
        {
            std::string cols;
            for (const auto& c : n.columns) cols += c + " ";
            out << "  node" << myId
                << " [label=\"Select\\n" << cols << "\"];\n";
            int childId = writeDot(out, *n.child, nodeId);
            out << "  node" << myId << " -> node" << childId << ";\n";
        }
        else if constexpr (std::is_same_v<NodeType, GroupByNode>)
        {
            std::string keys;
            for (const auto& k : n.group_columns) keys += k + " ";
            out << "  node" << myId
                << " [label=\"GroupBy\\n" << keys << "\"];\n";
            int childId = writeDot(out, *n.child, nodeId);
            out << "  node" << myId << " -> node" << childId << ";\n";
        }
        else if constexpr (std::is_same_v<NodeType, AggNode>)
        {
            std::string aggs;
            for (const auto& [k, v] : n.agg_map) aggs += k + ":" + v + " ";
            out << "  node" << myId
                << " [label=\"Aggregate\\n" << aggs << "\"];\n";
            int childId = writeDot(out, *n.child, nodeId);
            out << "  node" << myId << " -> node" << childId << ";\n";
        }
        else if constexpr (std::is_same_v<NodeType, JoinNode>)
        {
            out << "  node" << myId
                << " [label=\"Join\\n" << n.how
                << "\", fillcolor=lightyellow];\n";
            int leftId  = writeDot(out, *n.left,  nodeId);
            int rightId = writeDot(out, *n.right, nodeId);
            out << "  node" << myId << " -> node" << leftId  << ";\n";
            out << "  node" << myId << " -> node" << rightId << ";\n";
        }
        else if constexpr (std::is_same_v<NodeType, SortNode>)
        {
            std::string cols;
            for (const auto& c : n.sort_columns) cols += c + " ";
            out << "  node" << myId
                << " [label=\"Sort\\n" << cols
                << (n.ascending ? " ASC" : " DESC") << "\"];\n";
            int childId = writeDot(out, *n.child, nodeId);
            out << "  node" << myId << " -> node" << childId << ";\n";
        }
        else if constexpr (std::is_same_v<NodeType, HeadNode>)
        {
            out << "  node" << myId
                << " [label=\"Head\\n" << n.n << "\"];\n";
            int childId = writeDot(out, *n.child, nodeId);
            out << "  node" << myId << " -> node" << childId << ";\n";
        }
        else
        {
            out << "  node" << myId << " [label=\"Unknown\"];\n";
        } }, node);

        return myId;
    }
}
#endif