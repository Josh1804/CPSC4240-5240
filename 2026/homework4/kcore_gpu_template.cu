#include "kcore_pipeline.cuh"
#include <iostream>
#include <cub/device/device_scan.cuh>

// ============================================================================
// CUDA KERNEL HEADERS
// ============================================================================

__global__ void level_init_kernel(int num_vertices, int k, const int* d_degrees, int* d_flags, int* d_coreness) {
    int id = blockIdx.x * blockDim.x + threadIdx.x;
    if (id < num_vertices) {
        // TODO: Check if vertex degree is below threshold `k` and is unpeeled
        if (d_degrees[id] <= k && d_coreness[id] == 0) {
            d_flags[id] = 1;
            // TODO: Update d_coreness to track exactly which level this vertex fell into
            // d_coreness[id] = ...;
        } else {
            d_flags[id] = 0;
        }
    }
}

__global__ void compaction_kernel(int num_vertices, const int* d_flags, const int* d_frontier_offsets, int* d_compacted_frontier) {
    int id = blockIdx.x * blockDim.x + threadIdx.x;
    if (id < num_vertices) {
        if (d_flags[id] == 1) {
            // TODO: Write vertex ID into proper offset of d_compacted_frontier
            // int offset = d_frontier_offsets[id];
            // d_compacted_frontier[...] = id;
        }
    }
}

__global__ void degree_updates_kernel(int num_active, const int* d_compacted_frontier, 
                                      const int* row_offsets, const int* column_indices, 
                                      int* d_degrees, const int* d_coreness) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < num_active) {
        int u = d_compacted_frontier[idx];
        
        // Loop through all neighbors of `u` using CSR
        int start = row_offsets[u];
        int end = row_offsets[u + 1];
        
        for (int i = start; i < end; i++) {
            int v = column_indices[i];
            
            // Only decrement degree of neighbors that are completely unpeeled (coreness == 0)
            if (d_coreness[v] == 0) {
                // TODO: atomicSub the degree of vertex `v` safely by 1
                // atomicSub(..., 1);
            }
        }
    }
}

// ============================================================================
// MAIN GPU PIPELINE
// ============================================================================

void compute_kcore_gpu(const CSRGraph& h_G, int* h_coreness) {
    // -------------------------------------------------------------------------
    // 1. Memory Transfers & Allocation
    // -------------------------------------------------------------------------
    int* d_row_offsets;
    int* d_column_indices;
    int* d_degrees;
    int* d_flags;
    int* d_coreness;
    int* d_frontier_offsets;
    int* d_compacted_frontier;

    cudaMalloc(&d_row_offsets, (h_G.num_vertices + 1) * sizeof(int));
    cudaMalloc(&d_column_indices, h_G.num_edges * sizeof(int));
    cudaMalloc(&d_degrees, h_G.num_vertices * sizeof(int));
    cudaMalloc(&d_flags, h_G.num_vertices * sizeof(int));
    cudaMalloc(&d_coreness, h_G.num_vertices * sizeof(int));
    cudaMalloc(&d_frontier_offsets, h_G.num_vertices * sizeof(int));
    cudaMalloc(&d_compacted_frontier, h_G.num_vertices * sizeof(int));

    cudaMemcpy(d_row_offsets, h_G.row_offsets, (h_G.num_vertices + 1) * sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpy(d_column_indices, h_G.column_indices, h_G.num_edges * sizeof(int), cudaMemcpyHostToDevice);
    cudaMemset(d_coreness, 0, h_G.num_vertices * sizeof(int));

    // Initialize degrees
    int* h_initial_degrees = new int[h_G.num_vertices];
    for (int i = 0; i < h_G.num_vertices; i++) {
        h_initial_degrees[i] = h_G.row_offsets[i+1] - h_G.row_offsets[i];
    }
    cudaMemcpy(d_degrees, h_initial_degrees, h_G.num_vertices * sizeof(int), cudaMemcpyHostToDevice);
    delete[] h_initial_degrees;

    // -------------------------------------------------------------------------
    // 2. CUB Temp Storage Pre-Allocation
    // -------------------------------------------------------------------------
    void *d_temp_storage = nullptr;
    size_t temp_storage_bytes = 0;
    cub::DeviceScan::ExclusiveSum(d_temp_storage, temp_storage_bytes, d_flags, d_frontier_offsets, h_G.num_vertices);
    cudaMalloc(&d_temp_storage, temp_storage_bytes);

    int current_k = 1;
    int total_peeled = 0;
    int block_size = 256;
    int grid_size_N = (h_G.num_vertices + block_size - 1) / block_size;

    // -------------------------------------------------------------------------
    // 3. The Control Loop
    // -------------------------------------------------------------------------
    while (total_peeled < h_G.num_vertices) {
        
        while (true) {
            // STEP A: Launch level_init_kernel
            level_init_kernel<<<grid_size_N, block_size>>>(h_G.num_vertices, current_k, d_degrees, d_flags, d_coreness);
            
            // STEP B: Run CUB ExclusiveSum
            cub::DeviceScan::ExclusiveSum(d_temp_storage, temp_storage_bytes, d_flags, d_frontier_offsets, h_G.num_vertices);
            
            // STEP C: Check completion
            int last_flag, last_offset;
            // TODO: Use cudaMemcpy to grab the VERY LAST element of `d_flags` and `d_frontier_offsets`
            // to find out exactly how many vertices were flagged this round.
            // cudaMemcpy(&last_flag, d_flags + (h_G.num_vertices - 1), ..., cudaMemcpyDeviceToHost);
            // cudaMemcpy(&last_offset, d_frontier_offsets + (h_G.num_vertices - 1), ..., cudaMemcpyDeviceToHost);
            
            int num_frontier = last_flag + last_offset;
            if (num_frontier == 0) break; 
            
            total_peeled += num_frontier;
            int frontier_grid = (num_frontier + block_size - 1) / block_size;

            // STEP D: Run compaction_kernel
            compaction_kernel<<<grid_size_N, block_size>>>(h_G.num_vertices, d_flags, d_frontier_offsets, d_compacted_frontier);
            
            // STEP E: Run degree_updates_kernel
            degree_updates_kernel<<<frontier_grid, block_size>>>(num_frontier, d_compacted_frontier, d_row_offsets, d_column_indices, d_degrees, d_coreness);
        }
        current_k++;
    }

    // -------------------------------------------------------------------------
    // 4. Write Back
    // -------------------------------------------------------------------------
    cudaMemcpy(h_coreness, d_coreness, h_G.num_vertices * sizeof(int), cudaMemcpyDeviceToHost);
    
    // Cleanup
    cudaFree(d_row_offsets);
    cudaFree(d_column_indices);
    cudaFree(d_degrees);
    cudaFree(d_flags);
    cudaFree(d_coreness);
    cudaFree(d_frontier_offsets);
    cudaFree(d_compacted_frontier);
    cudaFree(d_temp_storage);
}
