#include "kcore_pipeline.cuh"
#include <iostream>
#include <algorithm> // For std::lower_bound

CSRGraph build_csr_cpu(const parlay::sequence<Edge>& edges, int num_vertices) {
    CSRGraph graph;
    graph.num_vertices = num_vertices;
    graph.num_edges = (int)edges.size();
    
    // Allocate the underlying CSR arrays
    graph.row_offsets = new int[num_vertices + 1];
    graph.column_indices = new int[graph.num_edges];

    // -------------------------------------------------------------------------
    // 1. Edge Sorting
    // -------------------------------------------------------------------------
    auto compare_edges = [](const Edge& a, const Edge& b) {
        if (a.u != b.u) return a.u < b.u;
        return a.v < b.v;
    };
    auto sorted_edges = parlay::sort(edges, compare_edges);

    // -------------------------------------------------------------------------
    // 2. Degree Counting
    // -------------------------------------------------------------------------
    parlay::sequence<int> degrees(num_vertices, 0);
    
    parlay::parallel_for(0, num_vertices, [&](int i) {
        Edge dummy_start = {i, -1};
        Edge dummy_end = {i+1, -1};
        
        auto start_it = std::lower_bound(sorted_edges.begin(), sorted_edges.end(), dummy_start, compare_edges);
        auto end_it = std::lower_bound(sorted_edges.begin(), sorted_edges.end(), dummy_end, compare_edges);
        
        // TODO: Calculate the difference between end_it and start_it to find the degree
        // degrees[i] = ...;
    });
    
    // -------------------------------------------------------------------------
    // 3. Prefix Sum Generation
    // -------------------------------------------------------------------------
    auto scan_res = parlay::scan(degrees);
    auto& offsets = scan_res.first;
    int total_edges = scan_res.second;
    
    parlay::parallel_for(0, num_vertices, [&](int i) {
        graph.row_offsets[i] = offsets[i];
    });
    
    // TODO: Set the very last element of row_offsets to the total number of edges
    // graph.row_offsets[num_vertices] = ...;
    
    // -------------------------------------------------------------------------
    // 4. Column Indices Population
    // -------------------------------------------------------------------------
    parlay::parallel_for(0, graph.num_edges, [&](int i) {
        // TODO: Map destination vertices `v` directly from sorted_edges 
        // into graph.column_indices  
        // graph.column_indices[i] = ...;
    });
    
    return graph;
}
