#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <limits>

// Include ParlayLib (adjust the path if needed)
#include "parlaylib/include/parlay/primitives.h"
#include "parlaylib/include/parlay/parallel.h"
#include "parlaylib/include/parlay/sequence.h"
#include "parlaylib/include/parlay/utilities.h"

// A simple 2D point structure
struct Point2D {
  double x, y;
  Point2D(double xx=0.0, double yy=0.0) : x(xx), y(yy) {}

  bool operator==(const Point2D& other) const {
    if (other.x == x && other.y == y)    
      return true;
    else {
      return false;
    }
  }
};

// A helper to compute squared distance
inline double squared_distance(const Point2D& a, const Point2D& b) {
  double dx = a.x - b.x;
  double dy = a.y - b.y;
  return dx*dx + dy*dy;
}

// KD-Tree node
struct KDNode {
  int axis;          // 0 for x, 1 for y
  double splitValue; // coordinate pivot
  int pointIndex;    // index in the original array

  KDNode* left;
  KDNode* right;

  KDNode() : axis(0), splitValue(0.0), pointIndex(-1),
             left(nullptr), right(nullptr) {}
};

// DistIndex for storing (distance^2, index)
struct DistIndex {
  double dist;
  int index;
  DistIndex(double d=0, int i=0) : dist(d), index(i) {}
};

// TODO: Implement a function to build the kd-tree
// NEED TO MAKE DESTROYER FOR TREE (jk maybe not???) 
// Takes sequence of just the side, the indices slice keeps track of index in main sequence, will transfer entire sequence every time
KDNode* build_kd_tree(parlay::sequence<int> indices, const parlay::sequence<Point2D>& points, int depth = 0) {
  // 1) Base cases: if 0 or 1 points, create a leaf node or return
  // 2) Determine axis = (depth % 2)
  int axis = depth % 2;
  if (indices.size() == 0) {
    return nullptr;
  }
  else if (indices.size() == 1) {
    KDNode* leaf = (KDNode*)malloc(sizeof(KDNode));
    leaf->axis = axis;
    if (axis == 0){
      leaf->splitValue = points[indices[0]].x;
    } 
    else {
      leaf->splitValue = points[indices[0]].y;
    }
    leaf->pointIndex = indices[0];
    leaf->left = nullptr;
    leaf->right = nullptr;
    return leaf;
  }
  // after this size of indices is 2 or more
  KDNode* root = (KDNode*)malloc(sizeof(KDNode));
  root->axis = axis;
  // 4) Find median index
  auto medInd = parlay::kth_smallest(indices, indices.size()/2, [&](auto &a, auto &b){
    if (axis == 0){
      return points[a].x < points[b].x;
    }
    else {
      return points[a].y < points[b].y;
    }});
  root->pointIndex = *medInd;
  if (axis == 0){
    root->splitValue = points[*medInd].x;
  }
  else {
    root->splitValue = points[*medInd].y;
  }
  
  // 3) Sort indices by that axis (x or y)
  // Instead of sorting the indices I'll just go through each element and add them to an array 
  // depending on if they're greater than or less than the median element 
  parlay::sequence<int> left_ind;
  parlay::sequence<int> right_ind;
  // indices must be in same order of values so hard to parallelize 
  for (int i = 0; i < indices.size(); i++){
    // Skip the median element itself; it is stored as root->pointIndex
    if (indices[i] == *medInd) {
      continue;
    }
    if (axis == 0){
      if (points[indices[i]].x < root->splitValue){
        left_ind.push_back(indices[i]);
      } 
      else {
        right_ind.push_back(indices[i]);
      }
    }
    else {
      if (points[indices[i]].y < root->splitValue){
        left_ind.push_back(indices[i]);
      } 
      else {
        right_ind.push_back(indices[i]);
      }
    }
  }

  // 5) Create a node with that pivot
  // 6) Recurse left and right in parallel
  parlay::par_do(
    [&]() {root->left = build_kd_tree(left_ind, points, depth + 1);}, 
    [&]() {root->right = build_kd_tree(right_ind, points, depth + 1);}, 
    false
  );
  return root; 
}

// KNN Helper: holds a local max-heap of size k
class KNNHelper {
public:
  KNNHelper(const parlay::sequence<Point2D>& pts, int kk)
    : points(pts), k(kk) {
    best.reserve(k);
  }

  // Perform recursive search
  void search(const KDNode* node, const Point2D& q) {
    // TODO:
    // 1) compute dist2 from q to node->pointIndex
    if (node == nullptr){
      return;
    }
    double dist = squared_distance(q, points[node->pointIndex]);
    // 2) update_best if needed
    // std::cout << "Update called" << std::endl;
    update_best(dist, node->pointIndex);
    // 3) compare q's coordinate to splitValue
    if (node->axis == 0){
      if (q.x < node->splitValue){
        search(node->left, q);
        if (q.x - node->splitValue < points[best[0].index].x){
          search(node->right, q);
        }
      }
      else {
        search(node->right, q);
        if (node->splitValue - q.x < points[best[0].index].x){
          search(node->left, q);
        }
      }
    }
    else 
    {
      if (q.y < node->splitValue){
        search(node->left, q);
        if (q.y - node->splitValue < points[best[0].index].y){
          search(node->right, q);
        }
      }
      else {
        search(node->right, q);
        if (node->splitValue - q.y < points[best[0].index].y){
          search(node->left, q);
        }
      }
    }
    // 4) search near side, possibly search far side if needed
  }

  // Return final results sorted by ascending distance
  parlay::sequence<DistIndex> get_results() const {
    parlay::sequence<DistIndex> result(best.begin(), best.end());
    parlay::sort_inplace(result, [&](auto &a, auto &b){
      return a.dist < b.dist;
    });
    return result;
  }

private:
  const parlay::sequence<Point2D>& points;
  int k;
  std::vector<DistIndex> best; // will be a max-heap

  // If we have fewer than k, push. Otherwise compare with largest so far.
  void update_best(double dist2, int idx) {
    DistIndex elem = DistIndex(dist2, idx);
    if (best.size() < k){
      // TODO: use a max-heap for best and update best with dist2 and idx
      insert(elem);
    }
    else {
      if (dist2 < best[0].dist){
        best[0] = elem;
        int root = 0;
        // Sift down: compare with children and swap with larger child
        while (true) {
          int largest = root;
          // check left child 
          if (root*2 + 1 < best.size() && best[root*2 + 1].dist > best[largest].dist){
            largest = root*2 + 1;
          }
          // check right child 
          if (root*2 + 2 < best.size() && best[root*2 + 2].dist > best[largest].dist){
            largest = root*2 + 2;
          }
          if (largest != root){
            std::swap(best[root], best[largest]);
            root = largest;
          }
          else {
            break;
          }
        }
      }
    }
  }

  void insert(DistIndex elem){
    // Add the new element to the end of the heap
    best.push_back(elem);
    // index of new elem
    int index = best.size() - 1;
    // compare with parent and swap 
    while (index > 0 && best[(index - 1) / 2].dist < best[index].dist) {  
      std::swap(best[index], best[(index - 1) / 2]);
      // change index to parent 
      index = (index - 1) / 2;
    }
  }
};

// Parallel k-NN for all queries
parlay::sequence<parlay::sequence<DistIndex>>
knn_search_all(const KDNode* root,
               const parlay::sequence<Point2D>& data_points,
               const parlay::sequence<Point2D>& query_points,
               int k) {
  int Q = (int)query_points.size();
  parlay::sequence<parlay::sequence<DistIndex>> results(Q);

  parlay::parallel_for(0, Q, [&](int i){
    KNNHelper helper(data_points, k);
    helper.search(root, query_points[i]);
    results[i] = helper.get_results();
  }, 0, false);

  return results;
}

// A function to load points from a file
parlay::sequence<Point2D> load_points_from_file(const std::string &filename) {
  // TODO: open file, read N, read N lines of x y into a parlay::sequence
  std::ifstream file(filename);
  std::string buffer;
  if (file.is_open()){
    getline(file, buffer);
    int count = std::stoi(buffer);
    parlay::sequence<Point2D> points;
    Point2D temp = Point2D();
    for (int i = 0; i < count; i++){
      file >> buffer; 
      temp.x = std::stod(buffer);
      file >> buffer; 
      temp.y = std::stod(buffer);
      points.push_back(temp);
    }
    return points;
  }
  else {
    std::cerr << "ERROR: Could not open file\n";
    parlay::sequence<Point2D> points(0);
    return points;
  }

}

int main(int argc, char** argv) {
  if (argc < 4) {
    std::cerr << "Usage: " << argv[0]
              << " <data_file> <query_file> <k>\n";
    return 1;
  }

  std::string data_file  = argv[1];
  std::string query_file = argv[2];
  int k = std::stoi(argv[3]);

  auto data_points = load_points_from_file(data_file);
  int N = (int)data_points.size();

  parlay::sequence<int> indices(N);
  parlay::parallel_for(0, N, [&](int i){ indices[i] = i; }, 0, false);
  KDNode* root = build_kd_tree(indices, data_points, 0);

  auto query_points = load_points_from_file(query_file);
  int Q = (int)query_points.size();

  auto results = knn_search_all(root, data_points, query_points, k);

  for (int q = 0; q < Q; q++) {
    printf("Query %d: (%.2f, %.2f)\n", q, query_points[q].x, query_points[q].y);
    printf("  kNN: ");
    for (auto &di : results[q]) {
      printf("(dist2=%.2f, idx=%d) ", di.dist, di.index);
    }
    printf("\n");
  }

  return 0;
}

