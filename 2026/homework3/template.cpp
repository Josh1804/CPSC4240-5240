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

// For a max-heap, we want to put the largest distance on top
// inline bool operator<(const DistIndex &a, const DistIndex &b) {
//   return a.dist < b.dist;
// }

// TODO: Implement a function to build the kd-tree
// NEED TO MAKE DESTROYER FOR TREE
// Takes sequence of just the side, the indices slice keeps track of index in main sequence 
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
  // since points contains only the values I can sort that first
  // auto sorted_pts = parlay::sort(points, [&](auto &a, auto &b){
  //   if (axis == 0){
  //     return a.x < b.x;
  //   }
  //   else {
  //     return a.y < b.y;
  //   }
  // });
  auto medInd = parlay::kth_smallest(indices, indices.size()/2, [&](auto &a, auto &b){
    if (axis == 0){
      return points[a].x < points[b].x;
    }
    else {
      return points[a].y < points[b].y;
    }});
  // std::cout << "medInd: " << *medInd << std::endl;
  root->pointIndex = *medInd;
  if (axis == 0){
    root->splitValue = points[*medInd].x;
  }
  else {
    root->splitValue = points[*medInd].y;
  }
  // std::cout << "splitvalue: " << root->splitValue << std::endl;
  
  // 3) Sort indices by that axis (x or y)
  // Instead of sorting the indices I'll just go through each element and add them to an array 
  // depending on if they're greater than or less than the median element 
  // Should do this at the same time instead of sorting at the beginning 
  if (indices.size() == 2){
    root->right = nullptr;

    // parlay::sequence<Point2D> left;
    parlay::sequence<int> left_ind;
    left_ind.push_back(indices[0]);
    // left.push_back(points[0]);
    root->left = build_kd_tree(left_ind, points, depth + 1);
    return root;
  }
  // parlay::sequence<Point2D> left;
  parlay::sequence<int> left_ind;
  // parlay::sequence<Point2D> right;
  parlay::sequence<int> right_ind;
  // indices must be in same order of values so hard to parallelize 
  for (int i = 0; i < indices.size(); i++){
    if (axis == 0){
      if (points[indices[i]].x < root->splitValue){
        // left.push_back(points[indices[i]]);
        left_ind.push_back(indices[i]);
      } 
      else {
        // right.push_back(points[indices[i]]);
        right_ind.push_back(indices[i]);
      }
    }
    else {
      if (points[indices[i]].y < root->splitValue){
        // left.push_back(points[indices[i]]);
        left_ind.push_back(indices[i]);
      } 
      else {
        // right.push_back(points[indices[i]]);
        right_ind.push_back(indices[i]);
      }
    }
  }

  // 5) Create a node with that pivot
  // 6) Recurse left and right in parallel
  // std::cout << "Depth: " << depth << std::endl;
  // for (int i = 0; i < indices.size(); i++){
  //   std::cout << "points[" << i << "]: (" << points[i].x << ", " << points[i].y << ")" << std::endl;
  // }
  // std::cout << "size of left: " << left_ind.size() << std::endl;
  // std::cout << "size of right: " << right_ind.size() << std::endl;
  parlay::par_do(
    [&]() {root->left = build_kd_tree(left_ind, points, depth + 1);}, 
    [&]() {root->right = build_kd_tree(right_ind, points, depth + 1);}, 
    false
  );
  // std::cout << "Depth: " << depth << std::endl;
  // std::cout << "id: : " << root->pointIndex << std::endl;
  // if (root->left == nullptr){
  //   std::cout << "left: : empty" << std::endl;
  // }
  // else {
  //   std::cout << "left: : " << root->left->pointIndex << std::endl;
  // }
  // if (root->right == nullptr){
  //   std::cout << "right : empty" << std::endl;
  // }
  // else {
  //   std::cout << "right: : " << root->right->pointIndex << std::endl;
  // }
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
    // std::cout << "Search called" << std::endl;
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
        if (best.size() >= k && std::fabs(q.x - node->splitValue) < best[0].dist){
          search(node->right, q);
        }
      }
      else {
        search(node->right, q);
        if (best.size() >= k && std::fabs(node->splitValue - q.x) < best[0].dist){
          search(node->left, q);
        }
      }
    }
    else 
    {
      if (q.y < node->splitValue){
        search(node->left, q);
        if (best.size() >= k && std::fabs(q.y - node->splitValue) < best[0].dist){
          search(node->right, q);
        }
      }
      else {
        search(node->right, q);
        if (best.size() >= k && std::fabs(node->splitValue - q.y) < best[0].dist){
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
        int index = 0;
        int root = 0;
        // compare with parent and swap 
        while (index < best.size()) {
          // check left child 
          int new_ind;
          if (index*2 + 1 < best.size() && best[index*2 + 1].dist > best[index].dist){
            // change index to follow 
            index = index*2 + 1;
          }
          // check right child 
          else if (index*2 + 2 < best.size() && best[index*2 + 2].dist > best[index].dist){
            // change index to follow 
            index = index*2 + 2;
          }
          if (index != root){
            std::swap(best[root], best[index]);
            root = index;
          }
          else {
            break;
          }
        }
      }
    }
    // std::cout << "elem.dist: " << elem.dist << std::endl;
    // for (int i = 0; i < best.size(); i++){
    //   std::cout << "best[" << i << "]: " << best[i].dist << std::endl;
    // }
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
    // double x; 
    // double y;
    Point2D temp = Point2D();
    for (int i = 0; i < count; i++){
      file >> buffer; 
      temp.x = std::stod(buffer);
      file >> buffer; 
      temp.y = std::stod(buffer);
      points.push_back(temp);
      // std::cout << "temp[" << i << "]: (" << temp.x << ", " << temp.y << ")" << std::endl;
    }
    // for (int i = 0; i < count; i++){
    //   std::cout << "points[" << i << "]: (" << points[i].x << ", " << points[i].y << ")" << std::endl;
    // }
    return points;
  }
  else {
    std::cerr << "ERROR: Could not open file\n";
    parlay::sequence<Point2D> points(0);
    return points;
  }

}

int main(int argc, char** argv) {
  setenv("OMP_NUM_THREADS", "1", 1);
  setenv("PARLAY_NUM_THREADS", "1", 1);
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
    std::cout << "Query " << q << " : ("
              << query_points[q].x << ", "
              << query_points[q].y << ")\n";
    std::cout << "  kNN: ";
    for (auto &di : results[q]) {
      std::cout << "(dist2=" << di.dist
                << ", idx=" << di.index << ") ";
    }
    std::cout << "\n";
  }

  return 0;
}

