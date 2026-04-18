#pragma once

#ifndef __CUDACC__
// ParlayLib is CPU-only; nvcc must not see these headers.
#include "parlaylib/include/parlay/parallel.h"
#include "parlaylib/include/parlay/sequence.h"
#include "parlaylib/include/parlay/primitives.h"
#endif

struct Edge { int u; int v; };

struct CSRGraph {
    int num_vertices;
    int num_edges;
    int* row_offsets;    // Size: num_vertices + 1
    int* column_indices; // Size: num_edges
};

#ifndef __CUDACC__
// Task A (Implemented in .cpp)
CSRGraph build_csr_cpu(const parlay::sequence<Edge>& edges, int num_vertices);
#endif

// Task B (Implemented in .cu)
void compute_kcore_gpu(const CSRGraph& h_G, int* h_coreness);
