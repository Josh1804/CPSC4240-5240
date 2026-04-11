#include "kcore_pipeline.cuh"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

// Helper to load edges from a simple edge list file where each line has "u v"
parlay::sequence<Edge> load_edges(const std::string& filename, int& num_vertices) {
    std::ifstream infile(filename);
    if (!infile.is_open()) {
        std::cerr << "Failed to open " << filename << std::endl;
        exit(1);
    }
    
    int max_vertex = -1;
    std::vector<Edge> edges_vec;
    int u, v;
    while (infile >> u >> v) {
        edges_vec.push_back({u, v});
        if (u > max_vertex) max_vertex = u;
        if (v > max_vertex) max_vertex = v;
    }
    
    num_vertices = max_vertex + 1;
    return parlay::sequence<Edge>(edges_vec.begin(), edges_vec.end());
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <edge_list_file>\n";
        return 1;
    }

    std::string filename = argv[1];
    int num_vertices = 0;
    
    std::cout << "Loading edges from " << filename << "...\n";
    auto edges = load_edges(filename, num_vertices);
    std::cout << "Loaded " << edges.size() << " edges and " << num_vertices << " vertices.\n";

    // Build the CSR Graph using CPU (Task A)
    std::cout << "Building CSR on CPU...\n";
    CSRGraph graph = build_csr_cpu(edges, num_vertices);

    // Compute K-Core on GPU (Task B)
    std::vector<int> coreness(num_vertices, 0);
    std::cout << "Computing K-Core on GPU...\n";
    compute_kcore_gpu(graph, coreness.data());

    // Print some results or validate
    std::cout << "Sample coreness mapping:\n";
    for(int i = 0; i < std::min(10, num_vertices); i++) {
        std::cout << "Vertex " << i << " : Coreness " << coreness[i] << "\n";
    }

    // Cleanup resources
    // TODO: Free graph.row_offsets and graph.column_indices

    return 0;
}
