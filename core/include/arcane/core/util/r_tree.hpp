// rstar_tree.hpp — skeleton of an in-memory 2D R*-tree (Beckmann et al., SIGMOD 1990).
//
// Every function below is a stub with a TODO. The stubs compile (they return
// dummy values), so you can implement one piece at a time and run the tests.
//
// Suggested order:
//   1. Rect helpers (area, margin, expand, intersects, ...)
//   2. Node::mbr, query / queryRec            -> a tree with one leaf works
//   3. insertTop, insertAtLevel, insertRec, a simple chooseSubtree,
//      overflowTreatment that ALWAYS splits, split  -> a working R-tree
//   4. validate / validateRec                  -> catch bugs early
//   5. Proper R* chooseSubtree, then forcedReinsert
//   6. remove / removeRec
//   7. nearest (kNN)
//
// Terminology:
//   MBR   = minimum bounding rectangle (the smallest Rect containing something)
//   M     = MaxEntries, the most entries a node may hold
//   m     = MinEntries, the fewest entries a non-root node may hold
//   level = height above the leaves: leaf nodes have level 0, their parents 1, ...
//           Counting from the leaves (not from the root) means a stored level
//           stays valid even if the root splits and the tree grows taller.
//
// Requirements on T (the stored value, e.g. an int id or a pointer):
//   default-constructible, copyable, and has operator== (used by remove).

#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <deque>
#include <limits>
#include <memory>
#include <queue>
#include <tuple>
#include <utility>
#include <vector>

#include "core\arcane\geometry\math.hpp"

namespace rstar {

    // Returns v.x for axis 0 and v.y for axis 1.
    inline float axisValue(const Vec2& v, int axis) {
        return axis == 0 ? v.x : v.y;
    }

    // ---------------------------------------------------------------------------
    // Rect — axis-aligned bounding box, stored as its min and max corners.
    // Invariant for a non-empty Rect: min.x <= max.x and min.y <= max.y.
    // A point is stored as a Rect with min == max (zero width and height).
    // ---------------------------------------------------------------------------
    struct Rect {
        Vec2 min;  // bottom-left corner (smallest x and y)
        Vec2 max;  // top-right corner (largest x and y)

        // Returns an "inverted" Rect that contains nothing: min = +FLT_MAX and
        // max = -FLT_MAX (use std::numeric_limits<float>::max() and ::lowest()).
        // Calling expand() on it with any Rect r gives exactly r, so it is the
        // starting value when computing the MBR of a list of rects.
        static Rect empty() 
            return Rect{ Vec2{std::numeric_limits<float>::lowest(),std::numeric_limits<float>::lowest()}, 
                         Vec2{std::numeric_limits<float>::max(),std::numeric_limits<float>::max()} };
        }

        // Returns a zero-size Rect at point p (min = max = p).
        static Rect fromPoint(const Vec2& p) {
            return Rect{p, p};
        }

        // Returns width * height. A point has area 0.
        float area() const {
            return (max.x - min.x) * (max.y - min.y);
        }

        // Returns the "margin" (width + height). 
        float margin() const {
            return (max.x - min.x) + (max.y - min.y);
        }

        // Returns the centre coordinate on one axis: (min + max) / 2
        float center(int axis) const {
            return (axisValue(max, axis) + axisValue(min, axis)) / 2;
        }

        // Grows *this in place so it also covers o:
        void expand(const Rect& o) {
            min.x = std::min(min.x, o.min.x);
            min.y = std::min(min.y, o.min.y);
            max.x = std::max(max.x, o.max.x);
            max.x = std::max(max.y, o.max.y);
        }

        // Returns a new Rect and leaves *this unchanged.
        Rect united(const Rect& o) const {
            return {Rect{Vec2{std::min(min.x, o.min.x), std::min(min.y, o.min.y), Vec2{std::max(max.x, o.max.x), std::max(max.y, o.max.y)};
        }

        // Returns true if *this and o overlap. Touching edges count as intersecting.
        bool intersects(const Rect& o) const {
            return (min.x <= o.max.x && max.x >= o.min.x &&
            min.y <= o.max.y && max.y >= o.min.y);
        }

        // Returns true if o lies completely inside *this (edges may touch).
        // remove() uses it to decide which subtrees could hold the entry.
        bool contains(const Rect& o) const {
            // TODO
            (void)o;
            return false;
        }

        // Returns the AREA of the intersection of *this and o, or 0 if they do not
        // overlap (or only touch). The overlap width on x is
        // min(max.x, o.max.x) - max(min.x, o.min.x); if it is <= 0, return 0.
        // Do the same for y and return width * height.
        float overlap(const Rect& o) const {
            // TODO
            (void)o;
            return 0.0f;
        }

        // Returns the SQUARED distance from p to the closest point of the Rect,
        // or 0 if p is inside. For each axis, the gap is (min - p) if p is below
        // min, (p - max) if p is above max, otherwise 0. Return gapX^2 + gapY^2.
        // Squared distances avoid sqrt and sort the same way as real distances.
        float minDist2(const Vec2& p) const {
            // TODO
            (void)p;
            return 0.0f;
        }

        // Exact equality of both corners. validate() uses it to compare a stored
        // MBR with a freshly computed one, and remove() uses it to find the entry.
        bool operator==(const Rect& o) const {
            // TODO
            (void)o;
            return false;
        }
        bool operator!=(const Rect& o) const { return !(*this == o); }
    };

    // ---------------------------------------------------------------------------
    // R*-tree
    // ---------------------------------------------------------------------------
    template <typename T,
        int MaxEntries = 16,                       // M
        int MinEntries = MaxEntries * 2 / 5,       // m: the paper suggests 40% of M
        int ReinsertCount = MaxEntries * 3 / 10>   // p: the paper suggests 30% of M
    class RStarTree {
        // These guarantee that a split of M+1 entries into two groups of >= m
        // is always possible, and that a node still has >= m entries after
        // forced reinsertion removes p of its M+1 entries.
        static_assert(MinEntries >= 2, "MinEntries must be >= 2");
        static_assert(2 * MinEntries <= MaxEntries + 1, "split needs 2*m <= M+1");
        static_assert(ReinsertCount >= 1, "ReinsertCount must be >= 1");
        static_assert(MaxEntries + 1 - ReinsertCount >= MinEntries,
            "node must keep >= m entries after forced reinsertion");

    public:
        // Creates an empty tree: root_ is an empty leaf (level 0), size_ is 0.
        RStarTree() : root_(std::make_unique<Node>(0)) {}

        // Nodes are owned by unique_ptr, so the tree can be moved but not copied.
        RStarTree(const RStarTree&) = delete;
        RStarTree& operator=(const RStarTree&) = delete;
        RStarTree(RStarTree&&) noexcept = default;
        RStarTree& operator=(RStarTree&&) noexcept = default;

        // Number of stored values (leaf entries), not the number of nodes.
        std::size_t size() const { return size_; }
        bool empty() const { return size_ == 0; }

        // Number of levels: 1 for a tree that is just one leaf.
        int height() const { return root_->level + 1; }

        // Deletes everything: replace root_ with a new empty leaf, set size_ = 0.
        void clear() {
            // TODO
        }

        // =======================================================================
        // insert
        // -----------------------------------------------------------------------
        // Input:  the box (use Rect::fromPoint for a point) and the value to store.
        // Result: the tree holds one more entry and size_ is one larger.
        // Steps:
        //   1. Build a leaf Entry {box, child = nullptr, value}.
        //   2. ++size_.
        //   3. insertTop(entry, 0); level 0 because data entries live in leaves.
        // =======================================================================
        void insert(const Rect& box, const T& value) {
            // TODO
            (void)box; (void)value;
        }

        // Convenience overload for points.
        void insert(const Vec2& point, const T& value) { insert(Rect::fromPoint(point), value); }

        // =======================================================================
        // remove
        // -----------------------------------------------------------------------
        // Input:  the box and value of an entry that was inserted before.
        // Output: true if one matching entry (box == box AND value == value) was
        //         found and removed, false if there was no such entry.
        // Steps:
        //   1. Make an empty list of orphans: (Entry, level) pairs.
        //   2. Call removeRec(root_, box, value, orphans). If it returns false,
        //      return false.
        //   3. --size_.
        //   4. For each orphan, call insertTop(entry, level). These are the
        //      entries of nodes that dropped below m and were dissolved; each one
        //      goes back in at the level it came from. Subtree entries stay
        //      whole subtrees; you don't re-insert their individual leaves.
        //   5. While the root is NOT a leaf and has exactly 1 entry, make that
        //      entry's child the new root (the tree gets one level shorter).
        //   6. Return true.
        // =======================================================================
        bool remove(const Rect& box, const T& value) {
            // TODO
            (void)box; (void)value;
            return false;
        }

        bool remove(const Vec2& point, const T& value) { return remove(Rect::fromPoint(point), value); }

        // =======================================================================
        // query (range search), callback version
        // -----------------------------------------------------------------------
        // Input:  query rectangle q, and a callable f(const Rect& box, const T& value).
        // Result: f is called once for every stored entry whose box intersects q,
        //         in no particular order. The tree is not modified.
        // Steps:  if the tree is not empty, call queryRec(root_, q, f).
        // =======================================================================
        template <typename F>
        void query(const Rect& q, F&& f) const {
            // TODO
            (void)q; (void)f;
        }

        // Same as above, but collects the matching values into a vector and
        // returns it. Easiest way: call the callback version with a lambda
        // that does out.push_back(value).
        std::vector<T> query(const Rect& q) const {
            // TODO
            (void)q;
            return {};
        }

        // =======================================================================
        // nearest (k nearest neighbours, best-first search)
        // -----------------------------------------------------------------------
        // Input:  query point p, and k = how many results you want.
        // Output: up to k (squaredDistance, value) pairs sorted nearest first.
        //         The distance is box.minDist2(p) of the stored entry. If the tree
        //         holds fewer than k entries, all of them are returned.
        // Steps:
        //   1. If k == 0 or the tree is empty, return an empty vector.
        //   2. Use a min-priority-queue of Items ordered by d (smallest d on top;
        //      std::priority_queue with std::greater<Item>). An Item is EITHER a
        //      node to open (node != nullptr) OR a data entry (entry != nullptr).
        //   3. Push {0, root_, nullptr}.
        //   4. While the queue is not empty and you have fewer than k results:
        //        pop the top Item.
        //        - If it is a data entry: append (d, entry->value) to the results.
        //          It is guaranteed to be the next nearest, because everything
        //          still in the queue has distance >= d.
        //        - If it is a node: for each of its entries, compute
        //          e.box.minDist2(p). If the node is a leaf, push the entry as a
        //          data Item; otherwise push e.child as a node Item.
        //   5. Return the results.
        // =======================================================================
        std::vector<std::pair<float, T>> nearest(const Vec2& p, std::size_t k) const {
            struct Item {
                float d;             // minDist2 from p to this item's box
                const Node* node;    // non-null: a subtree still to open
                const Entry* entry;  // non-null: a stored value (leaf entry)
                bool operator>(const Item& o) const { return d > o.d; }
            };
            // TODO
            (void)p; (void)k;
            return {};
        }

        // =======================================================================
        // validate (for debugging)
        // -----------------------------------------------------------------------
        // Output: true if the whole tree satisfies every invariant (see
        //         validateRec) AND the number of leaf entries found equals size_.
        // Call it in tests after inserts and removes; most bugs show up here first.
        // =======================================================================
        bool validate() const {
            // TODO: count = 0; return validateRec(root_, true, count) && count == size_;
            return true;
        }

    private:
        struct Node;

        // One slot in a node.
        //   In a leaf node:     box = the stored item's box, value = stored value,
        //                       child = nullptr.
        //   In an internal node: box = MBR of the child node (must always equal
        //                       child->mbr() exactly), child = owned child node,
        //                       value is unused.
        struct Entry {
            Rect box;
            std::unique_ptr<Node> child;
            T value{};
        };

        struct Node {
            int level;                   // 0 = leaf; a child is always one lower than its parent
            std::vector<Entry> entries;  // between m and M entries (root: 0..M if leaf, 2..M otherwise)

            // Reserve M+1: a node briefly holds one too many before overflow handling.
            explicit Node(int lvl) : level(lvl) { entries.reserve(MaxEntries + 1); }

            bool isLeaf() const { return level == 0; }

            // Returns the MBR of all entries' boxes: start from Rect::empty() and
            // expand() with every entry's box.
            Rect mbr() const {
                // TODO
                return {};
            }
        };

        std::unique_ptr<Node> root_;  // never null; an empty tree is an empty leaf
        std::size_t size_ = 0;        // number of stored values

        // reinserted_[L] is true once forced reinsertion has run at level L during
        // the current insertTop call. The R* rule: reinsert at most once per level
        // per insertion, and split if the same level overflows again.
        std::vector<bool> reinserted_;

        // Entries taken out by forcedReinsert that still need to go back in, with
        // the level each must be inserted at. Processed in FIFO order by insertTop.
        std::deque<std::pair<Entry, int>> pending_;

        // =======================================================================
        // insertTop: one complete logical insertion
        // -----------------------------------------------------------------------
        // Input:  an entry e and the level of the node it must end up in
        //         (0 for data entries, higher for subtrees from remove()).
        // Result: e and every entry moved by forced reinsertion are in the tree.
        // Steps:
        //   1. Reset reinserted_ to (root_->level + 1) values, all false.
        //   2. insertAtLevel(e, level).
        //   3. While pending_ is not empty: take the front (entry, lvl), pop it,
        //      insertAtLevel(entry, lvl). Those calls may add more to pending_,
        //      which is fine. Do NOT reset reinserted_ here; it covers the whole
        //      logical insertion.
        // =======================================================================
        void insertTop(Entry e, int level) {
            // TODO
            (void)e; (void)level;
        }

        // =======================================================================
        // insertAtLevel
        // -----------------------------------------------------------------------
        // Input:  entry e and target level (must be <= root_->level).
        // Result: e is placed. If the root split, the tree is one level taller.
        // Steps:
        //   1. sibling = insertRec(root_, e, level).
        //   2. If sibling is not null, the root was split:
        //        - create newRoot with level = root_->level + 1
        //        - give it two entries: {root_->mbr(), old root} and
        //          {sibling->mbr(), sibling}
        //        - root_ = newRoot
        //        - grow reinserted_ to root_->level + 1 (new values false).
        // =======================================================================
        void insertAtLevel(Entry e, int level) {
            // TODO
            (void)e; (void)level;
        }

        // =======================================================================
        // insertRec: recursive descent
        // -----------------------------------------------------------------------
        // Input:  current node, entry e, target level.
        // Output: a NEW sibling node if `node` had to be split (the caller must
        //         add it to its own entries), otherwise nullptr.
        // Steps:
        //   1. If node->level == level: push e into node->entries.
        //      Otherwise:
        //        a. i = chooseSubtree(node, e.box)
        //        b. child = node->entries[i].child
        //        c. sibling = insertRec(child, e, level)
        //        d. node->entries[i].box = child->mbr()  // child may have grown,
        //           or shrunk if it split or gave away entries to reinsertion
        //        e. if sibling: push a new Entry {sibling->mbr(), sibling}.
        //   2. If node now has more than M entries, return overflowTreatment(node).
        //   3. Return nullptr.
        // Note: get the child pointer BEFORE pushing into node->entries; pushing
        // can reallocate the vector, so don't hold references to entries across it.
        // =======================================================================
        std::unique_ptr<Node> insertRec(Node* node, Entry e, int level) {
            // TODO
            (void)node; (void)e; (void)level;
            return nullptr;
        }

        // =======================================================================
        // overflowTreatment
        // -----------------------------------------------------------------------
        // Input:  a node holding M+1 entries.
        // Output: a new sibling if we split, nullptr if we reinserted instead.
        // Steps:
        //   If node is NOT the root and reinserted_[node->level] is false:
        //     set it to true, call forcedReinsert(node), return nullptr.
        //   Otherwise: return split(node).
        // (Tip: first make this always split, so you have a plain R-tree; add
        //  reinsertion once everything else works.)
        // =======================================================================
        std::unique_ptr<Node> overflowTreatment(Node* node) {
            // TODO
            (void)node;
            return nullptr;
        }

        // =======================================================================
        // chooseSubtree: which child should receive a new box b?
        // -----------------------------------------------------------------------
        // Input:  an internal node (level >= 1) and the box b to insert.
        // Output: index into node->entries of the chosen child.
        // Rules:
        //   If node->level == 1 (its children are leaves), pick the entry with the
        //   least OVERLAP ENLARGEMENT, tie-break by least area enlargement, then
        //   by least area:
        //     for each entry i:
        //       u           = entries[i].box.united(b)
        //       overlapEnl  = sum over j != i of
        //                     u.overlap(entries[j].box) - entries[i].box.overlap(entries[j].box)
        //       areaEnl     = u.area() - entries[i].box.area()
        //   If node->level > 1, pick the entry with the least AREA ENLARGEMENT,
        //   tie-break by least area.
        // Tip: std::tie(a, b, c) < std::tie(bestA, bestB, bestC) compares the
        // criteria in order.
        // Note: with only points, every overlap and area is 0, so all of these
        // tie. A common fix is a final tie-break on the distance from b to the
        // centre of entries[i].box.
        // =======================================================================
        std::size_t chooseSubtree(const Node* node, const Rect& b) const {
            // TODO
            (void)node; (void)b;
            return 0;
        }

        // =======================================================================
        // forcedReinsert
        // -----------------------------------------------------------------------
        // Input:  a non-root node with M+1 entries.
        // Result: the node keeps M+1-p entries; the p entries FARTHEST from the
        //         node's centre are moved into pending_ with level = node->level.
        // Steps:
        //   1. c = centre of node->mbr() (use center(0), center(1)).
        //   2. Sort entries by squared distance from their box centre to c,
        //      farthest first.
        //   3. Move the first p entries into pending_, starting with the CLOSEST of
        //      them (index p-1 down to 0); the paper found "close reinsert" works
        //      better.
        //   4. Erase those p entries from the node.
        // The caller then recomputes this node's MBR (insertRec step 1d), which is
        // now smaller.
        // =======================================================================
        void forcedReinsert(Node* node) {
            // TODO
            (void)node;
        }

        // =======================================================================
        // split: the R* split
        // -----------------------------------------------------------------------
        // Input:  a node with n = M+1 entries.
        // Output: a new sibling Node at the same level. Both nodes end up with
        //         >= m entries; `node` keeps group 1, the sibling gets group 2.
        //
        // A "distribution" is: sort the entries, then group 1 = first k entries,
        // group 2 = the rest, for every k from m to n-m.
        // Entries can be sorted along an axis (0 = x, 1 = y) in two ways:
        //   by (min, then max) of that axis, or by (max, then min).
        //
        // Step 1: choose the split axis.
        //   For each axis: for both sort orders, sort the entries and add up
        //   margin(group 1 MBR) + margin(group 2 MBR) over all distributions.
        //   Pick the axis with the SMALLEST total.
        //
        // Step 2: choose the split index on that axis.
        //   For both sort orders and all distributions, pick the one with the
        //   least overlap(group1 MBR, group2 MBR), tie-break by least
        //   area(group1) + area(group2).
        //
        // Step 3: re-sort with the winning sort order, move entries [k, n) into
        //   a new sibling node, erase them from `node`, return the sibling.
        //
        // Efficiency tip: after each sort, build prefix MBRs (pre[i] = MBR of
        // entries 0..i) and suffix MBRs (suf[i] = MBR of entries i..n-1). Then
        // group 1 = pre[k-1] and group 2 = suf[k] for every k without rescanning.
        // =======================================================================
        std::unique_ptr<Node> split(Node* node) {
            // TODO
            (void)node;
            return nullptr;
        }

        // =======================================================================
        // removeRec
        // -----------------------------------------------------------------------
        // Input:  current node, the box and value to remove, and the orphans list.
        // Output: true if the entry was found and removed somewhere in this subtree.
        // Steps:
        //   Leaf: find an entry with box == b && value == v; erase it, return true.
        //         If none, return false.
        //   Internal: for each entry i whose box CONTAINS b (only those can hold it):
        //     if removeRec(child, b, v, orphans) is true:
        //       - if child now has fewer than m entries: move each of the child's
        //         entries into orphans as (entry, child->level), then erase
        //         entry i (this deletes the child node)
        //       - else: entries[i].box = child->mbr()   // it may have shrunk
        //       return true
        //   Return false if no child had it.
        // =======================================================================
        bool removeRec(Node* node, const Rect& b, const T& v,
            std::vector<std::pair<Entry, int>>& orphans) {
            // TODO
            (void)node; (void)b; (void)v; (void)orphans;
            return false;
        }

        // =======================================================================
        // queryRec
        // -----------------------------------------------------------------------
        // Input:  current node, query rect q, callback f.
        // Result: f(box, value) has been called for each matching leaf entry
        //         in this subtree.
        // Steps:  for each entry whose box intersects q:
        //           leaf node     -> f(entry.box, entry.value)
        //           internal node -> queryRec(entry.child, q, f)
        // =======================================================================
        template <typename F>
        void queryRec(const Node* node, const Rect& q, F& f) const {
            // TODO
            (void)node; (void)q; (void)f;
        }

        // =======================================================================
        // validateRec
        // -----------------------------------------------------------------------
        // Input:  current node, whether it is the root, and a running count.
        // Output: false if any invariant fails in this subtree. Adds the number of
        //         leaf entries found to `count`.
        // Check:
        //   - entries.size() <= M
        //   - non-root: entries.size() >= m
        //   - root that is not a leaf: entries.size() >= 2
        //   - leaf: no entry has a child          (then count += size, return true)
        //   - internal: every entry has a child, child->level == node->level - 1
        //     (so all leaves are at the same depth), entry.box == child->mbr()
        //     exactly, and validateRec(child, false, count) is true.
        // =======================================================================
        bool validateRec(const Node* node, bool isRoot, std::size_t& count) const {
            // TODO
            (void)node; (void)isRoot; (void)count;
            return true;
        }
    };

}  // namespace rstar