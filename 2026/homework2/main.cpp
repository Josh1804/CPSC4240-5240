#include <iostream>
#include <vector>
#include <atomic>
#include <algorithm>
#include <omp.h>
#include <random>
#include <chrono>
#include <cstring>
#include <cassert>

// Threshold to switch to sequential execution
const int SERIAL_THRESHOLD = 4096;

// ============================================================
// TASK 1: LOCK-FREE BUFFER POOL FOR MERGES
// ============================================================

struct PoolNode
{
  std::vector<int> *data;
  PoolNode *next;
};

class AtomicBufferPool
{
  std::atomic<PoolNode *> head;

public:
  AtomicBufferPool() : head(nullptr) {}

  ~AtomicBufferPool()
  {
    PoolNode *curr = head.load();
    while (curr)
    {
      PoolNode *next = curr->next;
      delete curr->data;
      delete curr;
      curr = next;
    }
  }

  // TODO: Implement Lock-Free Pop using CAS loop
  std::vector<int> *acquire_buffer(size_t capacity)
  {
    std::vector<int> *vec_ptr = nullptr;

    // --- TODO: YOUR CODE HERE ---
    // 1. Load head. 2. CAS loop. 3. Handle empty case.
    PoolNode* oldHead;
    PoolNode* newHead; 
    do
    {
      oldHead = head.load();
      if (oldHead == nullptr){
        break;
      }
      newHead = oldHead->next;
      vec_ptr = oldHead->data;
    } while (!head.compare_exchange_weak(oldHead, newHead));
    // ----------------------

    // Fallback: If pool empty, allocate fresh
    if (vec_ptr == nullptr)
    {
      vec_ptr = new std::vector<int>();
    }

    if (vec_ptr->size() < capacity)
    {
      vec_ptr->resize(capacity);
    }
    std::cout << "Buffer acquired by: " << omp_get_thread_num() << std::endl;
    return vec_ptr;
  }

  // TODO: Implement Lock-Free Push using CAS loop
  void release_buffer(std::vector<int> *buf)
  {
    // --- TODO: YOUR CODE HERE ---
    // 1. Create node. 2. CAS loop to push to head.

    PoolNode* newHead = new PoolNode;
    newHead->data = buf;
    PoolNode* oldHead;
    do
    {
      oldHead = head.load();
      newHead->next = oldHead;
    } while (!head.compare_exchange_weak(oldHead, newHead));
    std::cout << "Buffer released by: " << omp_get_thread_num() << std::endl;
    // ----------------------
  }
};

AtomicBufferPool pool;

// ============================================================
// TASK 2: PARALLEL MERGE OF TWO VECTORS
// ============================================================

void seq_merge(int *A, int nA, int *B, int nB, int *C)
{
  // std::cout << "Merging by: " << omp_get_thread_num() << " nA " << nA << " A " << A << " nB " << nB << std::endl;
  std::cout << "Merging by: " << omp_get_thread_num() << " A[nA-1] " << A[nA-1] << " B[nB-1] " << B[nB-1] << " A[0] " << A[0] << std::endl;
  std::merge(A, A + nA, B, B + nB, C);
  // if (nA + nB == 2048){
  //   for (int i = 0; i <= nA + nB - 1; i++){
  //     std::cout << "T[" << i << "] = " << T[i] << std::endl;
  //   }
  // }
}

// TODO: Implement the Divide-and-Conquer Parallel Merge
// Follow the algorithm described in the assignment PDF.
void parallel_binary_merge(int *A, int sA, int tA, int *B, int sB, int tB, int *C, int sC, int tC)
{
  int nA = tA - sA + 1; 
  int nB = tB - sB + 1; 
  // 1. Base Case (use seq_merge)
  if (nA + nB < SERIAL_THRESHOLD){
    // std::cout << "Merging by: " << omp_get_thread_num() << " nA " << nA << " sA " << sA << " nB " << nB << " sB " << sB << std::endl;
    seq_merge(A + sA, nA, B + sB, nB, C);
    return;
  }

  // 2. Ensure A is larger (Swap if needed)
  if (nA < nB){
    // swaps pointers
    int* temp = B;
    B = A; 
    A = temp;

    // swaps sizes, start, and end 
    int nTemp = nB;
    nB = nA;
    nA = nTemp;

    nTemp = sB;
    sB = sA; 
    sA = nTemp;

    nTemp = tB;
    tB = tA; 
    tA = nTemp;
  }

  if (nA <= 0 || nB <= 0){
    return;
  }

  // 3. Find Median of A
  // floors answer // MAYBE MINUS ONE 
  int midA = sA + nA/2 - 1; // start plus half of the size 
  int medianA = A[midA];

  // 4. Binary Search Median in B
  int j = -1;
  int top = tB; 
  int bottom = sB;
  // could be parallelized 
  while(bottom < top){
    int mid = bottom + (top - bottom)/2;
    // std::cout << "Merging by: " << omp_get_thread_num() << " mid " << mid << " Bottom " << bottom << " top " << top << std::endl;
    if (B[mid] == medianA){
      j = mid;
      // std::cout << "Merging by: " << omp_get_thread_num() << " mid " << mid << " Bottom " << bottom << " top " << top << std::endl;
      // std::cout << "Merging by: " << omp_get_thread_num() << " mid should end now "<< i << std::endl;
      break;
    }
    if (B[mid] < medianA){
      bottom = mid + 1;
      j = bottom;
      // std::cout << "Merging by: " << omp_get_thread_num() << " mid " << mid << " Bottom " << bottom << " top " << top << std::endl;
      // std::cout << "Merging by: " << omp_get_thread_num() << " B[mid] " << B[mid] << " median " << medianA << std::endl;
    }
    if (B[mid] > medianA) {
      j = top;
      top = mid - 1;
      // std::cout << "Merging by: " << omp_get_thread_num() << " mid " << mid << " Bottom " << bottom << " top " << top << std::endl;
      // std::cout << "Merging by: " << omp_get_thread_num() << " mid should end now "<< i << std::endl;
    }
    if (bottom == top){
      j = top;
      // std::cout << "Merging by: " << omp_get_thread_num() << " mid should end now "<< i << std::endl;
      break;
    }
    // std::cout << "Merging by: " << omp_get_thread_num() << " mid " << mid << " Bottom " << bottom << " top " << top << std::endl;
    // std::cout << "Merging by: " << omp_get_thread_num() << " sB " << sB << " tB " << tB << std::endl;
    // std::cout << "Merging by: " << omp_get_thread_num() << " j " << j << std::endl;
  }
  // if (j == -1){
  //   if (bottom == top){
  //     j = bottom;
  //   }
    // std::cout << "Merging by: " << omp_get_thread_num() << " uhm... idk how this happened " << std::endl;
  //   // std::cout << "Merging by: " << omp_get_thread_num() << " broken: Bottom " << bottom << " top " << top << std::endl;
  //   // std::cout << "Merging by: " << omp_get_thread_num() << " sB " << sB << " tB " << tB  << " j " << j << std::endl;
  //   // std::cout << "Merging by: " << omp_get_thread_num() << " B[mid] " << B[mid] << " median " << medianA << std::endl;
  // }

  // 5. Place Median in C
  int midC = midA + j;
  // std::cout << "Merging by: " << omp_get_thread_num() << " sB " << sB << " tB " << tB << std::endl;
  // std::cout << "Merging by: " << omp_get_thread_num() << " sA " << sA << " tA " << tA << std::endl;
  // std::cout << "Merging by: " << omp_get_thread_num() << " midA " << midA << " j " << j << std::endl;
  // std::cout << "Merging by: " << omp_get_thread_num() << " midC " << midC << " C[midC] " << C[midC] << std::endl;
  C[midC] = medianA;
  // 6. Spawn 2 Recursive Tasks (Left and Right)
  #pragma omp task 
  {
    if (sB < j){
      parallel_binary_merge(A, sA, midA - 1, B, sB, j - 1, C, sC, midC);
    }
    // else {
    //   std::cout << "Merging by: " << omp_get_thread_num() << " sB " << sB << " tB " << tB << std::endl;
    // }
  }
  #pragma omp task 
  {
    if (tB > j + 1){
      // uses pointer arithmetic to move array up to midA and j 
      parallel_binary_merge(A, midA, tA, B, j, tB, C, midC + 1, tC);
    }
    // else {
    //   std::cout << "Merging by: " << omp_get_thread_num() << " sB " << sB << " tB " << tB << std::endl;
    // }
  }

  // 7. Wait
  #pragma omp taskwait
}

// ============================================================
// TASK 3: 4-WAY MERGESORT
// ============================================================

void mergesort_4way(int *arr, int n)
{
  if (n < SERIAL_THRESHOLD)
  {
    std::sort(arr, arr + n);
    return;
  }

  // 1. Calculate Splits
  int q = n / 4;
  int r = n % 4;
  int s1 = q, s2 = q, s3 = q, s4 = q + r;

  int *p1 = arr;
  int *p2 = p1 + s1;
  int *p3 = p2 + s2;
  int *p4 = p3 + s3;

  // TODO: Spawn 4 Parallel Tasks to sort p1, p2, p3, p4
  // Use #pragma omp task

  // --- TODO: YOUR CODE HERE ---
  #pragma omp task 
  {
    mergesort_4way(p1, s1);
  }
  #pragma omp task 
  {
    mergesort_4way(p2, s2);
  }
  #pragma omp task 
  {
    mergesort_4way(p3, s3);
  }
  #pragma omp task 
  {
    mergesort_4way(p4, s4);
  }

  #pragma omp taskwait
  // ----------------------

  // 2. Acquire Buffer
  std::vector<int> *temp_vec = pool.acquire_buffer(n);
  int *T = temp_vec->data();
  int *T_mid = T + (s1 + s2);
  
  // 3. Parallel Merge Phase
  // Merge (Q1+Q2) -> Left Half of T
  // Merge (Q3+Q4) -> Right Half of T
  // TODO: Launch in parallel tasks calling parallel_binary_merge
  
  // --- TODO: YOUR CODE HERE ---
  #pragma omp task 
  {
    std::cout << "P1 and P2 merging by: " << omp_get_thread_num() << std::endl;
    parallel_binary_merge(p1, 0, s1 - 1, p2, 0, s2 - 1, T, 0, s1 + s2 - 1);
    std::cout << "P1 and P2 binary merged by: " << omp_get_thread_num() << std::endl;
    // for (int i = s2 + s1 - 1; i >= 0 ; i--){
    //   std::cout << "T[" << i << "] = " << T[i] << std::endl;
    // }
  }
  #pragma omp task 
  {
    std::cout << "P3 and P4 merging by: " << omp_get_thread_num() << std::endl;
    parallel_binary_merge(p3, 0, s3 - 1, p4, 0, s4 - 1, T_mid, 0, s3 + s4 - 1);
    std::cout << "P3 and P4 binary merged by: " << omp_get_thread_num() << std::endl;
    // for (int i = s3 + s4 - 1; i >= 0 ; i--){
    //   std::cout << "T_mid[" << i << "] = " << T_mid[i] << std::endl;
    // }
    // std::cout << "s3: " << s3 << " s4: " << s4 << std::endl;
  }
  
  #pragma omp taskwait
  // ----------------------

  // 4. Final Merge: Left+Right -> Original Array
  std::cout << "T and T_mid merging by: " << omp_get_thread_num() << std::endl;
  parallel_binary_merge(T, 0, s1 + s2 - 1, T_mid, 0, s3 + s4 - 1, arr, 0, n - 1);
  std::cout << "T and T_mid merging by: " << omp_get_thread_num() << std::endl;
  for (int i = s1 + s2 + s3 - 5; i <= s2 + s2 + s3 + 10 ; i++){
    std::cout << "arr[" << i << "] = " << arr[i] << std::endl;
  }
  // 5. Cleanup
  pool.release_buffer(temp_vec);
}

// ============================================================
// COMMAND-LINE AND GRADESCOPE TESTS (DO NOT MODIFY)
// ============================================================
int main(int argc, char *argv[])
{
  if (argc < 4)
  {
    std::cerr << "Usage: " << argv[0] << " <N> <num_threads> <seed>\n";
    return 1;
  }

  int N = std::stoi(argv[1]);
  int num_threads = std::stoi(argv[2]);
  unsigned int seed = std::stoul(argv[3]);

  std::vector<int> data(N);
  std::mt19937 rng(seed);
  for (int i = 0; i < N; ++i)
  {
    data[i] = rng();
  }

  std::vector<int> check = data;

  omp_set_num_threads(num_threads);
  omp_set_nested(1);
  omp_set_dynamic(0);

  double start_time = omp_get_wtime();

#pragma omp parallel
  {
#pragma omp single
    {
      mergesort_4way(data.data(), N);
    }
  }

  double end_time = omp_get_wtime();
  double elapsed = end_time - start_time;

  std::sort(check.begin(), check.end());
  bool passed = (data == check);

  // Output formatted string for Python tester
  if (passed)
  {
    std::cout << "RESULT:PASS," << elapsed << "\n";
    return 0;
  }
  else
  {
    std::cout << "RESULT:FAIL," << elapsed << "\n";
    return 1;
  }
}
