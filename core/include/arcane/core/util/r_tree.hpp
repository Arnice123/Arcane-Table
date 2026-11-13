// Requirements on T (the stored value, e.g. an int id or a pointer):
//   default-constructible, copyable, and has operator== (used by remove).

#pragma once

#include <type_traits>
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <utility>
#include <vector>
#include <ranges>
#include <limits>
#include <memory>
#include <deque>
#include <cmath>
#include <span>
#include <queue>
#include <tuple>

#include "arcane\core\geometry\math.hpp"
#include "arcane\core\geometry\segment.hpp"

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

        // Returns an "inverted" Rect that contains nothing: min = +FLT_MAX and max = -FLT_MAX
        // Calling expand() on it with any Rect r gives exactly r, so it is the starting value when computing the MBR of a list of rects.
        static Rect empty() {
            return Rect{ Vec2{std::numeric_limits<float>::max(),std::numeric_limits<float>::max()},
                         Vec2{std::numeric_limits<float>::lowest(),std::numeric_limits<float>::lowest()} };
        }

        // Returns a zero-size Rect at point p (min = max = p).
        static Rect fromPoint(const Vec2& p) {
            return Rect{ p, p };
        }

        static Rect fromSegment(const Segment& s) {
            return Rect{ Vec2{std::min(s.a.x, s.b.x), std::min(s.a.y, s.b.y)}, Vec2{std::max(s.a.x, s.b.x), std::max(s.a.y, s.b.y)} };
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
        float centre(int axis) const {
            return (axisValue(max, axis) + axisValue(min, axis)) / 2;
        }

        // Grows *this in place so it also covers o:
        void expand(const Rect& o) {
            min.x = std::min(min.x, o.min.x);
            min.y = std::min(min.y, o.min.y);
            max.x = std::max(max.x, o.max.x);
            max.y = std::max(max.y, o.max.y);
        }

        // Returns a new Rect and leaves *this unchanged.
        Rect united(const Rect& o) const {
            return Rect{ Vec2{std::min(min.x, o.min.x), std::min(min.y, o.min.y)}, Vec2{std::max(max.x, o.max.x), std::max(max.y, o.max.y)} };
        }

        // Returns true if *this and o overlap. Touching edges count as intersecting.
        bool intersects(const Rect& o) const {
            return (min.x <= o.max.x && max.x >= o.min.x &&
                min.y <= o.max.y && max.y >= o.min.y);
        }

        // Returns true if o lies completely inside *this (edges may touch).
        bool contains(const Rect& o) const {
            return (min.x <= o.min.x && min.y <= o.min.y && max.x >= o.max.x && max.y >= o.max.y);
        }

        // Returns the AREA of the intersection of *this and o, or 0 if they do not overlap (or only touch). 
        float overlap(const Rect& o) const {
            float wOverlap = std::min(max.x, o.max.x) - std::max(min.x, o.min.x);
            //if (wOverlap < 0) return 0;
            float hOverlap = std::min(max.y, o.max.y) - std::max(min.y, o.min.y);
            return (wOverlap < 0 || hOverlap < 0) ? 0 : wOverlap * hOverlap;
        }

        // Returns the SQUARED distance from p to the closest point of the Rect, or 0 if p is inside.
        float minDist2(const Vec2& p) const {
            float gapX = (p.x < min.x) ? min.x - p.x : (p.x > max.x) ? p.x - max.x : 0.0f;
            float gapY = (p.y < min.y) ? min.y - p.y : (p.y > max.y) ? p.y - max.y : 0.0f;
            return gapX * gapX + gapY * gapY;
        }

        // Exact equality of both corners. 
        bool operator==(const Rect& o) const {
            return min.x == o.min.x && min.y == o.min.y &&
                max.x == o.max.x && max.y == o.max.y;
        }
        bool operator!=(const Rect& o) const { return !(*this == o); }
    };

    template <typename T,
        int MaxEntries = 16,                       // M
        int MinEntries = MaxEntries * 2 / 5,       // m: the paper suggests 40% of M
        int ReinsertCount = MaxEntries * 3 / 10>   // p: the paper suggests 30% of M
    class RStarTree {
        // These guarantee that a split of M+1 entries into two groups of >= m is always possible, 
        // and that a node still has >= m entries after forced reinsertion removes p of its M+1 entries.
        static_assert(MinEntries >= 2, "MinEntries must be >= 2");
        static_assert(2 * MinEntries <= MaxEntries + 1, "split needs 2*m <= M+1");
        static_assert(ReinsertCount >= 1, "ReinsertCount must be >= 1");
        static_assert(MaxEntries + 1 - ReinsertCount >= MinEntries, "node must keep >= m entries after forced reinsertion");

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
            root_ = std::make_unique<Node>(0);
            size_ = 0;
        }

        // In:  the box (use Rect::fromPoint for a point) and the value to store.
        // Out: the tree holds one more entry and size_ is one larger.
        void insert(const Rect& box, const T& value) {
            Entry e{ box, nullptr, value };
            ++size_;
            insertTop(std::move(e), 0);
        }

        // Convenience overload for points.
        void insert(const Vec2& point, const T& value) { insert(Rect::fromPoint(point), value); }

        // In:  a span of (box, value) pairs, one per item.
        // Out: the tree holds exactly these items, built bottom-up by STR packing. Anything already in the tree is discarded. size_ == items.size().
        void bulkLoad(std::span<const std::pair<Rect, T>> items) {
            // turn each pair into a leaf Entry {box, nullptr, value}
            std::vector<Entry> entries;
            entries.reserve(items.size());
            for (const auto& i : items) {
                entries.push_back({ i.first, nullptr, i.second });
            }
            buildFromLeaves(std::move(entries));
        }

        // In:  a span of segments, and a callable valueOf(std::size_t index, const Segment& s) that returns the T to store for segs[index].
        // Out: same as above, with each segment's box = Rect::fromSegment(segs[i]).
        template <typename F>
        void bulkLoad(std::span<const Segment> segs, F&& valueOf) {
            // for each entry in segs: build a leaf Entry {Rect::fromSegment(segs[i]), nullptr, valueOf(i, segs[i])}
            std::vector<Entry> entries;
            entries.reserve(segs.size());
            for (std::size_t i = 0; i < segs.size(); ++i)
                entries.push_back(Entry{ Rect::fromSegment(segs[i]), nullptr, valueOf(i, segs[i]) });
        
            buildFromLeaves(std::move(entries));
        }

        // In:  a span of segments.
        // Out: same as above, storing each segment's index in the span as its value.
        // Only exists when T can be built from a std::size_t (int ids, etc.), hence the requires clause.
        void bulkLoad(std::span<const Segment> segs) requires std::is_constructible_v<T, std::size_t> {
            // call the overload above with a lambda that returns static_cast<T>(i)
            bulkLoad(segs, [](std::size_t i, [[maybe_unused]] const Segment& s) {return static_cast<T>(i);});
        }

        // In:  the box and value of an entry that was inserted before.
        // Out: true if one matching entry (box == box AND value == value) was found and removed, false if there was no such entry.
        bool remove(const Rect& box, const T& value) {
            // empty list of orphans to get filled by removeRec
            std::vector<std::pair<Entry, int>> orphans;
            if (!removeRec(root_.get(), box, value, orphans))
            {
                // if nothing was deleted return false
                return false;
            }
            --size_;
            // reinsert all entries of nodes that dropped below m (at the same level) (first = entry, second = level)
            for (auto& orph : orphans)
            {
                insertTop(std::move(orph.first), orph.second);
            }
            // If the root isn't a leaf and has 1 entry then shorten the tree
            while (root_->level != 0 && root_->entries.size() == 1)
            {
                root_ = std::move(root_->entries[0].child);
            }
            return true;
        }

        bool remove(const Vec2& point, const T& value) { return remove(Rect::fromPoint(point), value); }

        // Input:  query rectangle q, and a callable f(const Rect& box, const T& value).
        // Result: f is called once for every stored entry whose box intersects q, in no particular order. The tree is not modified.
        template <typename F>
        void query(const Rect& q, F&& f) const {
            queryRec(root_.get(), q, f);
        }

        // Same as query(const Rect& q, F&& f), but collects the matching values into a vector and returns it.
        std::vector<T> query(const Rect& q) const {
            std::vector<T> out;
            query(q, [&out]([[maybe_unused]] const Rect& box, const T& value) {out.push_back(value);});
            return out;
        }

        // (k nearest neighbours, best-first search)
        // In:  query point p, and k = how many results you want.
        // Out: up to k (squaredDistance, value) pairs sorted nearest first. The distance is box.minDist2(p) of the stored entry. 
        //         If the tree holds fewer than k entries, all of them are returned.
        std::vector<std::pair<float, T>> nearest(const Vec2& p, std::size_t k) const {
            //      An Item is EITHER a node to open (node != nullptr) OR a data entry (entry != nullptr).
            struct Item {
                float d;             // minDist2 from p to this item's box
                const Node* node;    // non-null: a subtree still to open
                const Entry* entry;  // non-null: a stored value (leaf entry)
                bool operator>(const Item& o) const { return d > o.d; }
            };
            // If k == 0 or the tree is empty, return an empty vector.
            if (k == 0 || size_ == 0) return {};

            //A min-prio-queue of Items ordered by d(smallest d on top).
            std::priority_queue<Item, std::vector<Item>, std::greater<Item>> lowestK;
            std::vector<std::pair<float, T>> results;

            lowestK.push({ 0, root_.get(), nullptr });

            // While the queue is not empty and you have fewer than k results:
            while (!lowestK.empty() && results.size() < k)
            {
                Item itm = lowestK.top();
                lowestK.pop();
                if (itm.node) { // Node
                    for (const auto& e : itm.node->entries)
                    {
                        float d = e.box.minDist2(p);
                        if (itm.node->level == 0) { // leaf node
                            lowestK.push({ d, nullptr, &e }); // add to q as Data Entry
                        }
                        else {
                            lowestK.push({ d, e.child.get(), nullptr }); // add the child since e isn't a leaf
                        }
                    }
                }
                else if (itm.entry) { // Data Entry
                    results.push_back({ itm.d, itm.entry->value });
                }
            }
            return results;
        }

        // Output: true if the whole tree satisfies every invariant AND the number of leaf entries found equals size_.
        // To be called in tests after inserts and removes.
        bool validate() const {
            std::size_t count = 0;
            return validateRec(root_.get(), true, count) && count == size_;
        }
    private:
        struct Node;

        // One slot in a node.
        //   In a leaf node:     box = the stored item's box, value = stored value, child = nullptr.
        //   In an internal node: box = MBR of the child node (must always equal child->mbr() exactly), child = owned child node, value is unused.
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
                Rect m = Rect::empty();
                for (const auto& e : entries)
                {
                    m.expand(e.box);
                }
                return m;
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

        // In:  all leaf entries (child == nullptr), in any order.
        // Outcome: root_ is the top of a freshly packed tree containing every entry, size_ == entries.size(), pending_ is empty. An empty input gives an empty leaf root.
        void buildFromLeaves(std::vector<Entry> entries) {
            size_ = entries.size();
            pending_.clear();
            int level = 0;

            // while there are still entries left, process them
            while (entries.size() > static_cast<std::size_t>(MaxEntries))
            {
                entries = packLevel(std::move(entries), level);
                ++level;
            }

            // the remaining <= M entries become the root:
            auto root = std::make_unique<Node>(level);
            root->entries = std::move(entries);
            root_ = std::move(root);
        }

        // In:  n > M entries that all belong in nodes at level lvl.
        // Out: one parent entry {node->mbr(), node} per new node, ready to be packed at level lvl + 1.
        //      Every new node has between m and M entries.
        //
        // Node sizes: P = ceil(n / M) nodes. Give the first (n % P) nodes floor(n/P) + 1 entries and the rest floor(n/P).
        //   ceil(n/P) <= M because of how P was chosen, and floor(n/P) >= m follows from 2m <= M + 1:
        //   n >= (P-1)M + 1, so n - Pm >= (P-2)(M-1)/2 >= 0.
        //   (Filling nodes with M each and putting the rest in the last node can leave it below m.)
        std::vector<Entry> packLevel(std::vector<Entry> es, int lvl) {
            // compute P, the number of slices S = ceil(sqrt(P)), base = n / P and extra = n % P
            // start(j) = index of node j's first entry = j * base + min(j, extra), so start(P) == n
            // sort all entries by box centre on axis 0 (x)
            // for each slice s: it owns nodes j0 = s*P/S up to j1 = (s+1)*P/S (always whole nodes, so the sizes above still hold)
            //     sort entries [start(j0), start(j1)) by box centre on axis 1 (y)
            //     for each node j in [j0, j1): create Node(lvl), move entries [start(j), start(j+1)) into it,
            //                                  push a parent Entry{node->mbr(), std::move(node)}
            // return the parent entries
            const std::size_t n = es.size();
            const std::size_t P = (n + MaxEntries - 1) / MaxEntries;
            const std::size_t S = static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<double>(P))));
            const std::size_t base = n / P, extra = n % P;

            auto start = [&](std::size_t j) { return j * base + std::min(j, extra); };
            auto byCentre = [](int axis) {
                return [axis](const Entry& a, const Entry& b) { return a.box.centre(axis) < b.box.centre(axis); };
                };

            std::sort(es.begin(), es.end(), byCentre(0));
            std::vector<Entry> parents;
            for (std::size_t s = 0; s < S; ++s) {
                const std::size_t j0 = s * P / S, j1 = (s + 1) * P / S;
                if (j0 == j1) continue;
                std::sort(es.begin() + start(j0), es.begin() + start(j1), byCentre(1));
                for (std::size_t j = j0; j < j1; ++j) {
                    auto node = std::make_unique<Node>(lvl);
                    for (std::size_t i = start(j); i < start(j + 1); ++i)
                        node->entries.push_back(std::move(es[i]));
                    parents.push_back({ node->mbr(), std::move(node) });
                }
            }
            return parents;
        }

        // In:  an entry e and the level of the node it must end up in (0 for data entries, higher for subtrees from remove()).
        // Outcome: e and every entry moved by forced reinsertion are in the tree.
        void insertTop(Entry e, int level) {
            // reset reinserted so we can reinsert at all levels up to this
            reinserted_.assign(root_->level + 1, false);
            insertAtLevel(std::move(e), level);
            while (!pending_.empty())
            {
                // take the front and insert it at (entry, lvl)
                std::pair<Entry, int> top_ = std::move(pending_.front());
                pending_.pop_front();

                // this may cause more entries to be added to pending so keep looping
                insertAtLevel(std::move(top_.first), top_.second);
            }
        }

        // In:  entry e and target level (must be <= root_->level).
        // Outcome: e is placed. If the root split, the tree is one level taller.
        void insertAtLevel(Entry e, int level) {
            std::unique_ptr<Node> sibling = insertRec(root_.get(), std::move(e), level);
            if (sibling != nullptr) // root was split
            {
                //create newRoot (level = root_->level + 1) and give it two entries: {root_->mbr(), old root} and {sibling->mbr(), sibling}
                std::unique_ptr<Node> new_root = std::make_unique<Node>(root_->level + 1);
                new_root->entries.push_back(Entry{ root_->mbr(), std::move(root_) });
                new_root->entries.push_back(Entry{ sibling->mbr(), std::move(sibling) });

                // update the root
                root_ = std::move(new_root);

                // reset and grow reinserted
                reinserted_.assign(root_->level + 1, false);
            }
        }


        // In:  current node, entry e, target level.
        // Out: a NEW sibling node if node had to be split, otherwise nullptr.
        std::unique_ptr<Node> insertRec(Node* node, Entry e, int level) {
            // if we are at desired level add e to entries
            if (node->level == level)
            {
                node->entries.push_back(std::move(e));
            }
            else
            {
                // choose subtre
                std::size_t i = chooseSubtree(node, e.box);
                Node* child = node->entries[i].child.get();

                // assign sibling as the new node if we split
                std::unique_ptr<Node> sibling = insertRec(child, std::move(e), level);
                node->entries[i].box = child->mbr();

                // if the child split re add the entry
                if (sibling)
                {
                    node->entries.push_back(Entry{ sibling->mbr(), std::move(sibling) });
                }
            }

            // if there are too many entries either split or reinsert
            if (node->entries.size() > MaxEntries)
            {
                return overflowTreatment(node);
            }
            return nullptr;
        }

        // In:  a node holding M+1 entries.
        // Out: a new sibling if we split, nullptr if we reinserted instead.
        std::unique_ptr<Node> overflowTreatment(Node* node) {
            // reinsert if we haven't reinserted at this level and we aren't at the root
            if (node != root_.get() && !reinserted_[node->level])
            {
                reinserted_[node->level] = true;
                forcedReinsert(node);
                return nullptr;
            }
            return split(node);
        }

        // In:  an internal node (level >= 1) and the box b to insert.
        // Out: index into node->entries of the chosen child that should receive a new box b
        std::size_t chooseSubtree(const Node* node, const Rect& b) const {
            const auto& es = node->entries;
            const float inf = std::numeric_limits<float>::max();
            std::size_t best = 0;

            if (node->level == 1) // children are leaves
            {
                float bOv = inf, bEnl = inf, bArea = inf;
                for (std::size_t i = 0; i < es.size(); ++i) {
                    // rectangle that contains this entry and b
                    Rect u = es[i].box.united(b);
                    float ov = 0;

                    // calculate how much overlap is being added 
                    for (std::size_t j = 0; j < es.size(); ++j) {
                        if (i == j) continue;
                        ov += u.overlap(es[j].box) - es[i].box.overlap(es[j].box);
                    }
                    float area = es[i].box.area();
                    float enl = u.area() - area;

                    // Choose by overlap enlargement, tie break with area enlargement then area
                    if (std::tie(ov, enl, area) < std::tie(bOv, bEnl, bArea)) {
                        bOv = ov; bEnl = enl; bArea = area; best = i;
                    }
                }
            }
            else // children are not leaves
            {
                float bEnl = inf, bArea = inf;
                for (std::size_t i = 0; i < es.size(); ++i) {
                    float area = es[i].box.area();
                    float enl = es[i].box.united(b).area() - area;

                    // Choose by area enlargement, tie break with area
                    if (std::tie(enl, area) < std::tie(bEnl, bArea)) {
                        bEnl = enl; bArea = area; best = i;
                    }
                }
            }
            return best;
        }

        // In:  a non-root node with M+1 entries.
        // Outcome: the node keeps M+1-p entries and the p entries furthest from the node's centre are moved into pending_ with level = node->level.
        void forcedReinsert(Node* node) {
            // calculate centre pos
            const Rect nMBR = node->mbr();
            Vec2 ctr = { nMBR.centre(0), nMBR.centre(1) };

            auto& es = node->entries;

            // calculate distance to centre, order so furthest is first
            auto dist2 = [ctr](const Rect& r) {
                float c0 = r.centre(0) - ctr.x;
                float c1 = r.centre(1) - ctr.y;
                return c0 * c0 + c1 * c1;
                };
            std::sort(es.begin(), es.end(), [&](const Entry& a, const Entry& b) { return dist2(a.box) > dist2(b.box); });

            // loop from p-1 to 0, adding them to pending, guarenteed to exist since p = M * 0.3
            // paper says to go from closest to furthest (The R*-tree: an efficient and robust access method for points and rectangles)
            for (int i = ReinsertCount - 1; i >= 0; --i)
                pending_.emplace_back(std::move(es[i]), node->level);

            // remove the old nodes
            es.erase(es.begin(), es.begin() + ReinsertCount);
        }


        // In: a leaf or internal node with exactly n = M + 1 entries.
        // Out: A new sibling Node at the same level. Both nodes end up with >= m entries; `node` keeps group 1, the sibling gets group 2.
        std::unique_ptr<Node> split(Node* node) {
            auto& es = node->entries;
            const std::size_t n = es.size();
            const std::size_t m = MinEntries;
            const float inf = std::numeric_limits<float>::max();

            // sort by either taking smallest max or min depending on byHi
            auto sortAxis = [&](int axis, bool byHi) {
                std::sort(es.begin(), es.end(), [&](const Entry& a, const Entry& b) {
                    if (byHi) return std::make_pair(axisValue(a.box.max, axis), axisValue(a.box.min, axis)) < std::make_pair(axisValue(b.box.max, axis), axisValue(b.box.min, axis));
                    return std::make_pair(axisValue(a.box.min, axis), axisValue(a.box.max, axis)) < std::make_pair(axisValue(b.box.min, axis), axisValue(b.box.max, axis));
                    });
                };


            // make two groups where if you take pre[i-1] and suf[i] then they reprent the MBR of all entries up to 0 to i-1 and all the entries i to n-1
            std::vector<Rect> pre(n), suf(n);
            auto computeMBR = [&]() {
                pre[0] = es[0].box;
                for (std::size_t i = 1; i < n - 1; ++i) pre[i] = pre[i - 1].united(es[i].box);
                suf[n - 1] = es[n - 1].box;
                for (std::size_t i = n - 1; i-- > 0;) suf[i] = suf[i + 1].united(es[i].box);
                };

            float bestMargin = inf;
            std::size_t bestAxis = 0;
            for (std::size_t i = 0; i < 2; ++i) // Loop through each axis
            {
                float marginSum = 0;
                for (bool HiLo : {false, true})
                {
                    sortAxis(i, HiLo);
                    computeMBR();
                    for (std::size_t k = m; k <= n - m; ++k)
                        marginSum += pre[k - 1].margin() + suf[k].margin(); // calculate the perimiter of the two boxes
                }
                if (marginSum < bestMargin) // choose the axis with the smallest perimiter
                {
                    bestMargin = marginSum;
                    bestAxis = i;
                }
            }

            float bOv = inf, bArea = inf;
            bool bestByHi = false;
            std::size_t bestK = m;
            for (bool HiLo : {false, true})
            {
                sortAxis(bestAxis, HiLo);
                computeMBR();
                for (std::size_t k = m; k <= n - m; ++k)
                {
                    // calculate overlap and area, we want the smallest overlap while breaking ties with area
                    float ov = pre[k - 1].overlap(suf[k]);
                    float area = pre[k - 1].area() + suf[k].area();
                    if (std::tie(ov, area) < std::tie(bOv, bArea)) {
                        bOv = ov; bArea = area; bestByHi = HiLo; bestK = k;
                    }
                }
            }

            // sort by the optimal options and keep 0...bestK-1 in node and bestK...n-1 in silbing
            sortAxis(bestAxis, bestByHi);
            std::unique_ptr<Node> sibling = std::make_unique<Node>(node->level);
            sibling->entries.reserve(n - bestK);

            for (std::size_t k = bestK; k < n; ++k)
            {
                sibling->entries.push_back(std::move(es[k]));
            }
            es.erase(es.begin() + bestK, es.end());
            return sibling;
        }

        // In:  current node, the box and value to remove, and the orphans list.
        // Out: true if the entry was found and removed somewhere in this subtree.
        bool removeRec(Node* node, const Rect& b, const T& v, std::vector<std::pair<Entry, int>>& orphans) {
            auto& es = node->entries;
            if (node->level == 0) { // leaf
                for (std::size_t i = 0; i < es.size(); ++i) {
                    if (es[i].box == b && es[i].value == v) { // if entry matches delete it and return true
                        es.erase(es.begin() + i);
                        return true;
                    }
                }
                return false;
            }

            // Internal Node
            for (std::size_t i = 0; i < es.size(); ++i) {
                // Make sure it contains the rec
                if (!es[i].box.contains(b)) continue;

                // if we don't remove it in the child then that means it's not in here
                Node* child = es[i].child.get();
                if (!removeRec(child, b, v, orphans)) continue;

                // we did remove it but now we are below the min entries, add it to the orphans
                if (child->entries.size() < MinEntries) {
                    for (auto& entry : child->entries) {
                        orphans.emplace_back(std::move(entry), child->level);
                    }
                    es.erase(es.begin() + i);
                }
                else { // we did remove it but now the size of the mbr is probably smaller
                    es[i].box = es[i].child->mbr();
                }
                return true;
            }
            return false;
        }

        // In:  current node, query rect q, callback f.
        // Outcome: Runs f(entry.box, entry.value) on each leaf entry that intersects with q
        template <typename F>
        void queryRec(const Node* node, const Rect& q, F& f) const {
            const auto& es = node->entries;
            bool leaf = node->isLeaf();
            for (const auto& e : es)
            {
                if (e.box.intersects(q)) {
                    if (leaf) {
                        f(e.box, e.value);
                    }
                    else {
                        queryRec(e.child.get(), q, f);
                    }
                }
            }
        }

        // In:  current node, whether it is the root, and a running count.
        // Out: false if any invariant fails in this subtree. Adds the number of leaf entries found to `count`.
        // Checks the following:
        //   - entries.size() <= M
        //   - non-root: entries.size() >= m
        //   - root that is not a leaf: entries.size() >= 2
        //   - leaf: no entry has a child
        //   - internal: every entry has a child, 
        //               child->level == node->level - 1
        //               entry.box == child->mbr()
        //               validateRec(child, false, count) is true.
        // =======================================================================
        bool validateRec(const Node* node, bool isRoot, std::size_t& count) const {
            const auto& es = node->entries;
            if (es.size() > MaxEntries) return false;
            if (!isRoot && es.size() < MinEntries) return false;
            if (isRoot && !node->isLeaf() && es.size() < 2) return false;
            if (node->isLeaf()) {
                for (const auto& e : es) {
                    if (e.child != nullptr) return false;
                }
                count += es.size();
            }
            else {
                for (const auto& e : es) {
                    if (e.child == nullptr || node->level - 1 != e.child->level || !validateRec(e.child.get(), false, count) || e.box != e.child->mbr()) {
                        return false;
                    }
                }
            }
            return true;
        }
    };
}