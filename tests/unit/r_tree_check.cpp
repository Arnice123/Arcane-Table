// Tests for rstar::RStarTree and rstar::Rect.
//
// Each test is a plain function and every check uses ARCANE_CHECK, so the first
// failure prints file:line and the condition, then exits with code 1.
// The process returns 0 when every test passes.
//
// Random data comes from a small fixed-seed generator (below) rather than
// <random> distributions, so every compiler/stdlib produces the same inputs
// and a failure always reproduces.

#include "arcane\core\util\r_tree.hpp"
#include "check.hpp"  

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <utility>
#include <vector>

namespace {

    using rstar::Rect;

    // Small trees split and reinsert after only a handful of inserts, so they
    // reach several levels quickly. M=4, m=2, p=1 satisfies every static_assert.
    using SmallTree = rstar::RStarTree<int, 4, 2, 1>;
    using MidTree = rstar::RStarTree<int, 8, 3, 2>;
    using DefaultTree = rstar::RStarTree<int>;  // M=16, m=6, p=4

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    // xorshift32: deterministic on every platform.
    struct Rng {
        std::uint32_t s;
        explicit Rng(std::uint32_t seed) : s(seed ? seed : 1u) {}
        std::uint32_t next() {
            s ^= s << 13;
            s ^= s >> 17;
            s ^= s << 5;
            return s;
        }
        // Uniform float in [lo, hi).
        float uniform(float lo, float hi) {
            return lo + (hi - lo) * (static_cast<float>(next() >> 8) / 16777216.0f);
        }
        // Uniform index in [0, n).
        std::size_t index(std::size_t n) { return static_cast<std::size_t>(next() % n); }
    };

    Rect makeRect(float x0, float y0, float x1, float y1) {
        return Rect{ Vec2{x0, y0}, Vec2{x1, y1} };
    }

    Rect randomPointRect(Rng& r, float extent) {
        return Rect::fromPoint(Vec2{ r.uniform(0, extent), r.uniform(0, extent) });
    }

    Rect randomBox(Rng& r, float extent, float maxSize) {
        float x = r.uniform(0, extent);
        float y = r.uniform(0, extent);
        return makeRect(x, y, x + r.uniform(0, maxSize), y + r.uniform(0, maxSize));
    }

    struct Item {
        Rect box;
        int value;
    };

    // Values of every item whose box intersects q, sorted.
    std::vector<int> bruteQuery(const std::vector<Item>& items, const Rect& q) {
        std::vector<int> out;
        for (const auto& it : items)
            if (it.box.intersects(q)) out.push_back(it.value);
        std::sort(out.begin(), out.end());
        return out;
    }

    // The k smallest squared distances from p, ascending.
    std::vector<float> bruteNearestDists(const std::vector<Item>& items, const Vec2& p, std::size_t k) {
        std::vector<float> d;
        for (const auto& it : items) d.push_back(it.box.minDist2(p));
        std::sort(d.begin(), d.end());
        if (d.size() > k) d.resize(k);
        return d;
    }

    template <typename Tree>
    std::vector<int> treeQuery(const Tree& t, const Rect& q) {
        std::vector<int> out = t.query(q);
        std::sort(out.begin(), out.end());
        return out;
    }

    // Checks query() against brute force for a batch of random query rects,
    // plus a query covering everything.
    template <typename Tree>
    void checkQueriesMatch(const Tree& t, const std::vector<Item>& items, Rng& r, float extent) {
        Rect all = makeRect(-1e6f, -1e6f, 1e6f, 1e6f);
        ARCANE_CHECK(treeQuery(t, all) == bruteQuery(items, all));
        for (int i = 0; i < 25; ++i) {
            Rect q = randomBox(r, extent, extent * 0.3f);
            ARCANE_CHECK(treeQuery(t, q) == bruteQuery(items, q));
        }
    }

    // Checks nearest() against brute force: distances must match exactly
    // (ties make the values ambiguous), each returned value must really be at
    // its reported distance, and results must be sorted nearest first.
    template <typename Tree>
    void checkNearestMatches(const Tree& t, const std::vector<Item>& items, const Vec2& p, std::size_t k) {
        auto res = t.nearest(p, k);
        auto expect = bruteNearestDists(items, p, k);
        ARCANE_CHECK(res.size() == expect.size());
        for (std::size_t i = 0; i < res.size(); ++i) {
            ARCANE_CHECK(res[i].first == expect[i]);
            if (i > 0) ARCANE_CHECK(res[i - 1].first <= res[i].first);
            bool found = false;
            for (const auto& it : items)
                if (it.value == res[i].second && it.box.minDist2(p) == res[i].first) { found = true; break; }
            ARCANE_CHECK(found);
        }
    }

    // -------------------------------------------------------------------------
    // Rect
    // -------------------------------------------------------------------------

    void testRectFromPoint() {
        Rect r = Rect::fromPoint(Vec2{ 3, -2 });
        ARCANE_CHECK(r.min.x == 3 && r.min.y == -2);
        ARCANE_CHECK(r.max.x == 3 && r.max.y == -2);
        ARCANE_CHECK(r.area() == 0);
        ARCANE_CHECK(r.margin() == 0);
    }

    void testRectAreaMarginCentre() {
        Rect r = makeRect(1, 2, 4, 7);  // 3 wide, 5 tall
        ARCANE_CHECK(r.area() == 15);
        ARCANE_CHECK(r.margin() == 8);
        ARCANE_CHECK(r.centre(0) == 2.5f);
        ARCANE_CHECK(r.centre(1) == 4.5f);
        ARCANE_CHECK(rstar::axisValue(Vec2{ 5, 6 }, 0) == 5);
        ARCANE_CHECK(rstar::axisValue(Vec2{ 5, 6 }, 1) == 6);
    }

    void testRectEmptyIsExpandIdentity() {
        Rect r = makeRect(-3, 1, 5, 9);
        Rect e = Rect::empty();
        e.expand(r);
        ARCANE_CHECK(e == r);

        // The empty rect must not intersect or contain anything.
        ARCANE_CHECK(!Rect::empty().intersects(r));
        ARCANE_CHECK(!Rect::empty().contains(r));

        // Expanding the empty rect with a point gives exactly that point.
        Rect p = Rect::fromPoint(Vec2{ 2, 2 });
        Rect e2 = Rect::empty();
        e2.expand(p);
        ARCANE_CHECK(e2 == p);
    }

    void testRectExpand() {
        Rect a = makeRect(0, 0, 1, 1);
        a.expand(makeRect(2, 3, 4, 5));
        ARCANE_CHECK(a == makeRect(0, 0, 4, 5));

        // Each axis must grow independently.
        Rect b = makeRect(0, 0, 10, 1);
        b.expand(makeRect(0, 0, 1, 10));
        ARCANE_CHECK(b == makeRect(0, 0, 10, 10));

        Rect c = makeRect(0, 0, 1, 1);
        c.expand(makeRect(-5, -6, 0.5f, 0.5f));
        ARCANE_CHECK(c == makeRect(-5, -6, 1, 1));
    }

    void testRectUnited() {
        const Rect a = makeRect(0, 0, 1, 1);
        Rect u = a.united(makeRect(2, -1, 3, 4));
        ARCANE_CHECK(u == makeRect(0, -1, 3, 4));
        ARCANE_CHECK(a == makeRect(0, 0, 1, 1));  // unchanged

        // united and expand agree.
        Rect b = a;
        b.expand(makeRect(2, -1, 3, 4));
        ARCANE_CHECK(b == u);
    }

    void testRectIntersects() {
        Rect a = makeRect(0, 0, 2, 2);
        ARCANE_CHECK(a.intersects(makeRect(1, 1, 3, 3)));    // partial overlap
        ARCANE_CHECK(a.intersects(makeRect(2, 0, 3, 2)));    // touching edge
        ARCANE_CHECK(a.intersects(makeRect(2, 2, 3, 3)));    // touching corner
        ARCANE_CHECK(a.intersects(makeRect(0.5f, 0.5f, 1, 1)));  // inside
        ARCANE_CHECK(a.intersects(makeRect(-1, -1, 3, 3)));  // surrounds
        ARCANE_CHECK(!a.intersects(makeRect(2.1f, 0, 3, 2)));  // gap on x
        ARCANE_CHECK(!a.intersects(makeRect(0, -3, 2, -0.1f)));  // gap on y
        ARCANE_CHECK(a.intersects(Rect::fromPoint(Vec2{ 2, 1 })));  // point on edge
        ARCANE_CHECK(!a.intersects(Rect::fromPoint(Vec2{ 1, 5 })));
        // Symmetric.
        ARCANE_CHECK(makeRect(1, 1, 3, 3).intersects(a));
    }

    void testRectContains() {
        Rect a = makeRect(0, 0, 4, 4);
        ARCANE_CHECK(a.contains(makeRect(1, 1, 2, 2)));
        ARCANE_CHECK(a.contains(a));                       // edges may touch
        ARCANE_CHECK(a.contains(Rect::fromPoint(Vec2{ 0, 4 })));
        ARCANE_CHECK(!a.contains(makeRect(1, 1, 5, 2)));
        ARCANE_CHECK(!a.contains(makeRect(-1, 1, 2, 2)));
        ARCANE_CHECK(!makeRect(1, 1, 2, 2).contains(a));
    }

    void testRectOverlap() {
        Rect a = makeRect(0, 0, 4, 4);
        ARCANE_CHECK(a.overlap(makeRect(2, 2, 6, 6)) == 4);
        ARCANE_CHECK(a.overlap(makeRect(1, 1, 2, 3)) == 2);  // fully inside
        ARCANE_CHECK(a.overlap(a) == 16);
        ARCANE_CHECK(a.overlap(makeRect(4, 0, 6, 4)) == 0);  // touching
        ARCANE_CHECK(a.overlap(makeRect(5, 5, 6, 6)) == 0);  // disjoint both axes
        // Disjoint on one axis only: both the width and height check matter.
        ARCANE_CHECK(a.overlap(makeRect(5, 1, 6, 2)) == 0);
        ARCANE_CHECK(a.overlap(makeRect(1, 5, 2, 6)) == 0);
        // Disjoint on both axes gives two negatives; product must not be positive.
        ARCANE_CHECK(a.overlap(makeRect(-3, -3, -1, -1)) == 0);
        // Symmetric.
        ARCANE_CHECK(makeRect(2, 2, 6, 6).overlap(a) == 4);
    }

    void testRectMinDist2() {
        Rect a = makeRect(0, 0, 2, 2);
        ARCANE_CHECK(a.minDist2(Vec2{ 1, 1 }) == 0);  // inside
        ARCANE_CHECK(a.minDist2(Vec2{ 2, 1 }) == 0);  // on edge
        ARCANE_CHECK(a.minDist2(Vec2{ 5, 1 }) == 9);  // right
        ARCANE_CHECK(a.minDist2(Vec2{ -3, 1 }) == 9); // left
        ARCANE_CHECK(a.minDist2(Vec2{ 1, 6 }) == 16); // above
        ARCANE_CHECK(a.minDist2(Vec2{ 1, -2 }) == 4); // below
        ARCANE_CHECK(a.minDist2(Vec2{ 5, 6 }) == 25); // corner: 3^2 + 4^2
        ARCANE_CHECK(Rect::fromPoint(Vec2{ 0, 0 }).minDist2(Vec2{ 3, 4 }) == 25);
    }

    void testRectEquality() {
        ARCANE_CHECK(makeRect(0, 1, 2, 3) == makeRect(0, 1, 2, 3));
        ARCANE_CHECK(makeRect(0, 1, 2, 3) != makeRect(0, 1, 2, 4));
        ARCANE_CHECK(makeRect(0, 1, 2, 3) != makeRect(9, 1, 2, 3));
        ARCANE_CHECK(!(makeRect(0, 1, 2, 3) != makeRect(0, 1, 2, 3)));
    }

    // -------------------------------------------------------------------------
    // Empty tree
    // -------------------------------------------------------------------------

    void testEmptyTree() {
        SmallTree t;
        ARCANE_CHECK(t.size() == 0);
        ARCANE_CHECK(t.empty());
        ARCANE_CHECK(t.height() == 1);
        ARCANE_CHECK(t.validate());
        ARCANE_CHECK(t.query(makeRect(-100, -100, 100, 100)).empty());
        ARCANE_CHECK(t.nearest(Vec2{ 0, 0 }, 5).empty());
        ARCANE_CHECK(!t.remove(Vec2{ 0, 0 }, 1));
        ARCANE_CHECK(t.size() == 0);
        ARCANE_CHECK(t.validate());

        int calls = 0;
        t.query(makeRect(-100, -100, 100, 100), [&](const Rect&, const int&) { ++calls; });
        ARCANE_CHECK(calls == 0);
    }

    // -------------------------------------------------------------------------
    // Insert
    // -------------------------------------------------------------------------

    void testInsertSingle() {
        SmallTree t;
        t.insert(Vec2{ 1, 2 }, 42);
        ARCANE_CHECK(t.size() == 1);
        ARCANE_CHECK(!t.empty());
        ARCANE_CHECK(t.height() == 1);
        ARCANE_CHECK(t.validate());

        auto hit = t.query(Rect::fromPoint(Vec2{ 1, 2 }));
        ARCANE_CHECK(hit.size() == 1 && hit[0] == 42);
        ARCANE_CHECK(t.query(makeRect(5, 5, 6, 6)).empty());
    }

    void testInsertUpToCapacityStaysOneLeaf() {
        SmallTree t;
        for (int i = 0; i < 4; ++i) {
            t.insert(Vec2{ static_cast<float>(i), 0 }, i);
            ARCANE_CHECK(t.validate());
        }
        ARCANE_CHECK(t.size() == 4);
        ARCANE_CHECK(t.height() == 1);
    }

    void testFirstOverflowSplitsRoot() {
        // The root never does forced reinsertion, so the 5th insert into an
        // M=4 leaf root must split it and grow the tree to height 2.
        SmallTree t;
        for (int i = 0; i < 5; ++i) t.insert(Vec2{ static_cast<float>(i), 0 }, i);
        ARCANE_CHECK(t.size() == 5);
        ARCANE_CHECK(t.height() == 2);
        ARCANE_CHECK(t.validate());
        ARCANE_CHECK(treeQuery(t, makeRect(-1, -1, 10, 1)) == (std::vector<int>{0, 1, 2, 3, 4}));
    }

    void testInsertManyPointsValidatesEveryStep() {
        Rng r(12345);
        SmallTree t;
        std::vector<Item> items;
        for (int i = 0; i < 500; ++i) {
            Rect b = randomPointRect(r, 100);
            t.insert(b, i);
            items.push_back({ b, i });
            ARCANE_CHECK(t.size() == items.size());
            ARCANE_CHECK(t.validate());
        }
        ARCANE_CHECK(t.height() >= 4);  // 500 entries at <=4 per node
        checkQueriesMatch(t, items, r, 100);
    }

    void testInsertManyBoxes() {
        Rng r(777);
        MidTree t;
        std::vector<Item> items;
        for (int i = 0; i < 1000; ++i) {
            Rect b = randomBox(r, 200, 15);
            t.insert(b, i);
            items.push_back({ b, i });
        }
        ARCANE_CHECK(t.size() == 1000);
        ARCANE_CHECK(t.validate());
        checkQueriesMatch(t, items, r, 200);
    }

    void testInsertGrid() {
        // Regular grid: lots of equal coordinates, which stresses the sort
        // tie-breaking in split and the distance ties in forced reinsertion.
        SmallTree t;
        std::vector<Item> items;
        int v = 0;
        for (int x = 0; x < 20; ++x)
            for (int y = 0; y < 20; ++y) {
                Rect b = Rect::fromPoint(Vec2{ static_cast<float>(x), static_cast<float>(y) });
                t.insert(b, v);
                items.push_back({ b, v });
                ++v;
            }
        ARCANE_CHECK(t.size() == 400);
        ARCANE_CHECK(t.validate());

        // Inclusive query on grid lines: 3x3 block.
        auto hit = treeQuery(t, makeRect(5, 5, 7, 7));
        ARCANE_CHECK(hit.size() == 9);
        ARCANE_CHECK(hit == bruteQuery(items, makeRect(5, 5, 7, 7)));

        // A query between grid lines finds nothing.
        ARCANE_CHECK(t.query(makeRect(5.2f, 5.2f, 5.8f, 5.8f)).empty());
    }

    void testInsertAllSamePoint() {
        // Every entry identical: every split candidate has the same cost.
        SmallTree t;
        for (int i = 0; i < 100; ++i) {
            t.insert(Vec2{ 3, 3 }, i);
            ARCANE_CHECK(t.validate());
        }
        ARCANE_CHECK(t.size() == 100);
        ARCANE_CHECK(t.query(Rect::fromPoint(Vec2{ 3, 3 })).size() == 100);
        ARCANE_CHECK(t.query(Rect::fromPoint(Vec2{ 3, 3.5f })).empty());
    }

    void testInsertSortedInput() {
        // Monotone input is a classic worst case for naive R-tree inserts.
        SmallTree t;
        std::vector<Item> items;
        for (int i = 0; i < 300; ++i) {
            Rect b = Rect::fromPoint(Vec2{ static_cast<float>(i), static_cast<float>(i) * 0.5f });
            t.insert(b, i);
            items.push_back({ b, i });
        }
        ARCANE_CHECK(t.validate());
        Rng r(5);
        checkQueriesMatch(t, items, r, 300);
    }

    void testInsertNegativeCoordinates() {
        Rng r(99);
        SmallTree t;
        std::vector<Item> items;
        for (int i = 0; i < 200; ++i) {
            Rect b = Rect::fromPoint(Vec2{ r.uniform(-50, 50), r.uniform(-50, 50) });
            t.insert(b, i);
            items.push_back({ b, i });
        }
        ARCANE_CHECK(t.validate());
        ARCANE_CHECK(treeQuery(t, makeRect(-50, -50, 0, 0)) == bruteQuery(items, makeRect(-50, -50, 0, 0)));
    }

    void testDefaultParameters() {
        Rng r(2024);
        DefaultTree t;
        std::vector<Item> items;
        for (int i = 0; i < 3000; ++i) {
            Rect b = (i % 2) ? randomBox(r, 1000, 20) : randomPointRect(r, 1000);
            t.insert(b, i);
            items.push_back({ b, i });
        }
        ARCANE_CHECK(t.size() == 3000);
        ARCANE_CHECK(t.validate());
        ARCANE_CHECK(t.height() >= 3);
        checkQueriesMatch(t, items, r, 1000);
        for (int i = 0; i < 10; ++i)
            checkNearestMatches(t, items, Vec2{ r.uniform(0, 1000), r.uniform(0, 1000) }, 10);
    }

    // -------------------------------------------------------------------------
    // Query
    // -------------------------------------------------------------------------

    void testQueryCallbackGetsStoredBox() {
        SmallTree t;
        std::vector<Item> items;
        for (int i = 0; i < 50; ++i) {
            Rect b = makeRect(static_cast<float>(i), 0, static_cast<float>(i) + 0.5f, 1);
            t.insert(b, i);
            items.push_back({ b, i });
        }
        int calls = 0;
        t.query(makeRect(-1, -1, 100, 100), [&](const Rect& box, const int& value) {
            ++calls;
            ARCANE_CHECK(value >= 0 && value < 50);
            ARCANE_CHECK(box == items[static_cast<std::size_t>(value)].box);
            });
        ARCANE_CHECK(calls == 50);
    }

    void testQueryTouchingEdges() {
        SmallTree t;
        t.insert(makeRect(0, 0, 1, 1), 1);
        t.insert(makeRect(2, 0, 3, 1), 2);
        t.insert(makeRect(0, 2, 1, 3), 3);
        // Query whose right edge touches box 2's left edge, top touches box 3.
        auto hit = treeQuery(t, makeRect(0.5f, 0.5f, 2, 2));
        ARCANE_CHECK(hit == (std::vector<int>{1, 2, 3}));
        // A point query exactly on a corner.
        auto corner = treeQuery(t, Rect::fromPoint(Vec2{ 1, 1 }));
        ARCANE_CHECK(corner == (std::vector<int>{1}));
    }

    void testQueryLargeBoxContainingPoint() {
        SmallTree t;
        Rng r(4);
        for (int i = 0; i < 100; ++i) t.insert(randomPointRect(r, 10), i);
        t.insert(makeRect(-100, -100, 100, 100), 999);
        auto hit = t.query(Rect::fromPoint(Vec2{ 50, 50 }));
        ARCANE_CHECK(hit.size() == 1 && hit[0] == 999);
        ARCANE_CHECK(t.validate());
    }

    void testQueryDoesNotModifyTree() {
        Rng r(31);
        SmallTree t;
        for (int i = 0; i < 100; ++i) t.insert(randomPointRect(r, 50), i);
        int h = t.height();
        for (int i = 0; i < 20; ++i) (void)t.query(randomBox(r, 50, 20));
        ARCANE_CHECK(t.size() == 100);
        ARCANE_CHECK(t.height() == h);
        ARCANE_CHECK(t.validate());
    }

    // -------------------------------------------------------------------------
    // Nearest
    // -------------------------------------------------------------------------

    void testNearestKZero() {
        SmallTree t;
        t.insert(Vec2{ 0, 0 }, 1);
        ARCANE_CHECK(t.nearest(Vec2{ 0, 0 }, 0).empty());
    }

    void testNearestSimple() {
        SmallTree t;
        t.insert(Vec2{ 0, 0 }, 0);
        t.insert(Vec2{ 10, 0 }, 1);
        t.insert(Vec2{ 3, 4 }, 2);
        t.insert(Vec2{ -1, 0 }, 3);
        t.insert(Vec2{ 100, 100 }, 4);

        auto res = t.nearest(Vec2{ 0, 0 }, 3);
        ARCANE_CHECK(res.size() == 3);
        ARCANE_CHECK(res[0].first == 0 && res[0].second == 0);
        ARCANE_CHECK(res[1].first == 1 && res[1].second == 3);
        ARCANE_CHECK(res[2].first == 25 && res[2].second == 2);

        auto one = t.nearest(Vec2{ 99, 99 }, 1);
        ARCANE_CHECK(one.size() == 1 && one[0].second == 4 && one[0].first == 2);
    }

    void testNearestMoreThanSize() {
        SmallTree t;
        std::vector<Item> items;
        for (int i = 0; i < 7; ++i) {
            Rect b = Rect::fromPoint(Vec2{ static_cast<float>(i * i), 0 });
            t.insert(b, i);
            items.push_back({ b, i });
        }
        auto res = t.nearest(Vec2{ 0, 0 }, 100);
        ARCANE_CHECK(res.size() == 7);
        for (std::size_t i = 0; i < res.size(); ++i) ARCANE_CHECK(res[i].second == static_cast<int>(i));
        checkNearestMatches(t, items, Vec2{ 0, 0 }, 100);
    }

    void testNearestUsesBoxDistance() {
        // A large box near p beats a point that is closer to the box's centre.
        SmallTree t;
        t.insert(makeRect(1, -10, 2, 10), 1);  // edge at distance 1
        t.insert(Vec2{ 0, 5 }, 2);             // distance 5
        auto res = t.nearest(Vec2{ 0, 0 }, 2);
        ARCANE_CHECK(res.size() == 2);
        ARCANE_CHECK(res[0].second == 1 && res[0].first == 1);
        ARCANE_CHECK(res[1].second == 2 && res[1].first == 25);

        // Query point inside a box gives distance 0.
        auto inside = t.nearest(Vec2{ 1.5f, 3 }, 1);
        ARCANE_CHECK(inside.size() == 1 && inside[0].second == 1 && inside[0].first == 0);
    }

    void testNearestRandomAgainstBruteForce() {
        Rng r(8080);
        SmallTree t;
        std::vector<Item> items;
        for (int i = 0; i < 600; ++i) {
            Rect b = (i % 3 == 0) ? randomBox(r, 100, 5) : randomPointRect(r, 100);
            t.insert(b, i);
            items.push_back({ b, i });
        }
        for (int i = 0; i < 40; ++i) {
            // Some query points outside the data's extent too.
            Vec2 p{ r.uniform(-20, 120), r.uniform(-20, 120) };
            std::size_t k = 1 + r.index(30);
            checkNearestMatches(t, items, p, k);
        }
        checkNearestMatches(t, items, Vec2{ 50, 50 }, items.size());
    }

    // -------------------------------------------------------------------------
    // Remove
    // -------------------------------------------------------------------------

    void testRemoveSingle() {
        SmallTree t;
        t.insert(Vec2{ 1, 1 }, 7);
        ARCANE_CHECK(t.remove(Vec2{ 1, 1 }, 7));
        ARCANE_CHECK(t.size() == 0);
        ARCANE_CHECK(t.empty());
        ARCANE_CHECK(t.height() == 1);
        ARCANE_CHECK(t.validate());
        ARCANE_CHECK(t.query(Rect::fromPoint(Vec2{ 1, 1 })).empty());
        // Removing again fails.
        ARCANE_CHECK(!t.remove(Vec2{ 1, 1 }, 7));
    }

    void testRemoveRequiresBoxAndValueMatch() {
        SmallTree t;
        Rng r(3);
        for (int i = 0; i < 60; ++i) t.insert(randomPointRect(r, 30), i);
        t.insert(makeRect(1, 1, 2, 2), 1000);
        std::size_t before = t.size();

        ARCANE_CHECK(!t.remove(makeRect(1, 1, 2, 2), 1001));   // right box, wrong value
        ARCANE_CHECK(!t.remove(makeRect(1, 1, 2, 3), 1000));   // wrong box, right value
        ARCANE_CHECK(!t.remove(makeRect(1.5f, 1.5f, 1.5f, 1.5f), 1000));  // inside, not equal
        ARCANE_CHECK(!t.remove(Vec2{ 500, 500 }, 1000));        // outside everything
        ARCANE_CHECK(t.size() == before);
        ARCANE_CHECK(t.validate());

        ARCANE_CHECK(t.remove(makeRect(1, 1, 2, 2), 1000));
        ARCANE_CHECK(t.size() == before - 1);
        ARCANE_CHECK(t.validate());
    }

    void testRemoveDuplicatesOneAtATime() {
        SmallTree t;
        for (int i = 0; i < 3; ++i) t.insert(Vec2{ 5, 5 }, 9);
        t.insert(Vec2{ 5, 5 }, 8);
        ARCANE_CHECK(t.size() == 4);

        ARCANE_CHECK(t.remove(Vec2{ 5, 5 }, 9));
        ARCANE_CHECK(t.size() == 3);
        ARCANE_CHECK(t.validate());
        auto hit = treeQuery(t, Rect::fromPoint(Vec2{ 5, 5 }));
        ARCANE_CHECK(hit == (std::vector<int>{8, 9, 9}));

        ARCANE_CHECK(t.remove(Vec2{ 5, 5 }, 9));
        ARCANE_CHECK(t.remove(Vec2{ 5, 5 }, 9));
        ARCANE_CHECK(!t.remove(Vec2{ 5, 5 }, 9));
        ARCANE_CHECK(t.size() == 1);
        ARCANE_CHECK(t.validate());
    }

    void testRemoveAllRandomOrderShrinksTree() {
        Rng r(4242);
        SmallTree t;
        std::vector<Item> items;
        for (int i = 0; i < 400; ++i) {
            Rect b = (i % 4 == 0) ? randomBox(r, 100, 8) : randomPointRect(r, 100);
            t.insert(b, i);
            items.push_back({ b, i });
        }
        ARCANE_CHECK(t.height() >= 4);

        while (!items.empty()) {
            std::size_t idx = r.index(items.size());
            Item it = items[idx];
            items[idx] = items.back();
            items.pop_back();

            ARCANE_CHECK(t.remove(it.box, it.value));
            ARCANE_CHECK(t.size() == items.size());
            ARCANE_CHECK(t.validate());
            ARCANE_CHECK(!t.remove(it.box, it.value));  // gone now

            if (items.size() % 50 == 0) checkQueriesMatch(t, items, r, 100);
        }
        ARCANE_CHECK(t.empty());
        ARCANE_CHECK(t.height() == 1);

        // The tree is still usable after being emptied.
        t.insert(Vec2{ 1, 1 }, 1);
        ARCANE_CHECK(t.size() == 1);
        ARCANE_CHECK(t.validate());
    }

    void testRemoveInInsertionOrder() {
        SmallTree t;
        std::vector<Item> items;
        for (int i = 0; i < 200; ++i) {
            Rect b = Rect::fromPoint(Vec2{ static_cast<float>(i % 15), static_cast<float>(i / 15) });
            t.insert(b, i);
            items.push_back({ b, i });
        }
        for (std::size_t i = 0; i < items.size(); ++i) {
            ARCANE_CHECK(t.remove(items[i].box, items[i].value));
            ARCANE_CHECK(t.validate());
        }
        ARCANE_CHECK(t.empty());
        ARCANE_CHECK(t.height() == 1);
    }

    void testRemoveHalfKeepsOtherHalf() {
        Rng r(1);
        MidTree t;
        std::vector<Item> keep, drop;
        for (int i = 0; i < 800; ++i) {
            Rect b = randomPointRect(r, 300);
            t.insert(b, i);
            (i % 2 ? keep : drop).push_back({ b, i });
        }
        for (const auto& it : drop) ARCANE_CHECK(t.remove(it.box, it.value));
        ARCANE_CHECK(t.size() == keep.size());
        ARCANE_CHECK(t.validate());
        checkQueriesMatch(t, keep, r, 300);
        for (int i = 0; i < 10; ++i)
            checkNearestMatches(t, keep, Vec2{ r.uniform(0, 300), r.uniform(0, 300) }, 15);
    }

    // -------------------------------------------------------------------------
    // Mixed workload
    // -------------------------------------------------------------------------

    template <typename Tree>
    void runMixedWorkload(std::uint32_t seed, int ops, float extent) {
        Rng r(seed);
        Tree t;
        std::vector<Item> items;
        int nextValue = 0;
        for (int op = 0; op < ops; ++op) {
            // Bias toward inserts early so the tree grows, then balance out.
            bool doInsert = items.empty() || r.index(100) < (op < ops / 3 ? 75u : 45u);
            if (doInsert) {
                Rect b = (r.index(3) == 0) ? randomBox(r, extent, extent * 0.05f) : randomPointRect(r, extent);
                t.insert(b, nextValue);
                items.push_back({ b, nextValue });
                ++nextValue;
            }
            else {
                std::size_t idx = r.index(items.size());
                ARCANE_CHECK(t.remove(items[idx].box, items[idx].value));
                items[idx] = items.back();
                items.pop_back();
            }
            ARCANE_CHECK(t.size() == items.size());
            ARCANE_CHECK(t.validate());

            if (op % 200 == 0) {
                checkQueriesMatch(t, items, r, extent);
                if (!items.empty())
                    checkNearestMatches(t, items, Vec2{ r.uniform(0, extent), r.uniform(0, extent) }, 8);
            }
        }
        checkQueriesMatch(t, items, r, extent);
    }

    void testMixedSmall() { runMixedWorkload<SmallTree>(17, 3000, 100); }
    void testMixedMid() { runMixedWorkload<MidTree>(29, 3000, 500); }
    void testMixedDefault() { runMixedWorkload<DefaultTree>(53, 3000, 1000); }

    // -------------------------------------------------------------------------
    // clear / move
    // -------------------------------------------------------------------------

    void testClear() {
        Rng r(10);
        SmallTree t;
        for (int i = 0; i < 150; ++i) t.insert(randomPointRect(r, 50), i);
        ARCANE_CHECK(t.height() > 1);
        t.clear();
        ARCANE_CHECK(t.size() == 0);
        ARCANE_CHECK(t.empty());
        ARCANE_CHECK(t.height() == 1);
        ARCANE_CHECK(t.validate());
        ARCANE_CHECK(t.query(makeRect(-1000, -1000, 1000, 1000)).empty());

        // Reusable after clear.
        for (int i = 0; i < 150; ++i) t.insert(randomPointRect(r, 50), i);
        ARCANE_CHECK(t.size() == 150);
        ARCANE_CHECK(t.validate());
    }

    void testMoveConstructAndAssign() {
        Rng r(11);
        std::vector<Item> items;
        SmallTree a;
        for (int i = 0; i < 120; ++i) {
            Rect b = randomPointRect(r, 40);
            a.insert(b, i);
            items.push_back({ b, i });
        }

        SmallTree b(std::move(a));
        ARCANE_CHECK(b.size() == 120);
        ARCANE_CHECK(b.validate());
        checkQueriesMatch(b, items, r, 40);

        SmallTree c;
        c.insert(Vec2{ 0, 0 }, -1);
        c = std::move(b);
        ARCANE_CHECK(c.size() == 120);
        ARCANE_CHECK(c.validate());
        checkQueriesMatch(c, items, r, 40);

        // The moved-to tree keeps working normally.
        ARCANE_CHECK(c.remove(items[0].box, items[0].value));
        c.insert(Vec2{ 1, 1 }, 500);
        ARCANE_CHECK(c.size() == 120);
        ARCANE_CHECK(c.validate());
    }

    // -------------------------------------------------------------------------
    // Other value types
    // -------------------------------------------------------------------------

    struct Handle {
        int id = 0;
        bool operator==(const Handle& o) const { return id == o.id; }
    };

    void testPointerValues() {
        std::vector<int> storage(50);
        rstar::RStarTree<int*, 4, 2, 1> t;
        for (int i = 0; i < 50; ++i) t.insert(Vec2{ static_cast<float>(i), 0 }, &storage[static_cast<std::size_t>(i)]);
        ARCANE_CHECK(t.validate());
        auto hit = t.query(Rect::fromPoint(Vec2{ 10, 0 }));
        ARCANE_CHECK(hit.size() == 1 && hit[0] == &storage[10]);
        ARCANE_CHECK(t.remove(Vec2{ 10, 0 }, &storage[10]));
        ARCANE_CHECK(!t.remove(Vec2{ 11, 0 }, &storage[10]));  // pointer must match
        ARCANE_CHECK(t.size() == 49);
        ARCANE_CHECK(t.validate());
    }

    void testStructValues() {
        rstar::RStarTree<Handle, 4, 2, 1> t;
        for (int i = 0; i < 40; ++i) t.insert(Vec2{ 0, static_cast<float>(i) }, Handle{ i });
        auto nn = t.nearest(Vec2{ 0, 20.2f }, 1);
        ARCANE_CHECK(nn.size() == 1 && nn[0].second.id == 20);
        ARCANE_CHECK(t.remove(Vec2{ 0, 20 }, Handle{ 20 }));
        nn = t.nearest(Vec2{ 0, 20.2f }, 1);
        ARCANE_CHECK(nn.size() == 1 && nn[0].second.id == 21);
        ARCANE_CHECK(t.validate());
    }

    // -------------------------------------------------------------------------

    struct TestCase {
        const char* name;
        void (*fn)();
    };

    const TestCase kTests[] = {
        {"Rect fromPoint", testRectFromPoint},
        {"Rect area/margin/centre", testRectAreaMarginCentre},
        {"Rect empty is expand identity", testRectEmptyIsExpandIdentity},
        {"Rect expand", testRectExpand},
        {"Rect united", testRectUnited},
        {"Rect intersects", testRectIntersects},
        {"Rect contains", testRectContains},
        {"Rect overlap", testRectOverlap},
        {"Rect minDist2", testRectMinDist2},
        {"Rect equality", testRectEquality},

        {"empty tree", testEmptyTree},
        {"insert single", testInsertSingle},
        {"insert up to capacity stays one leaf", testInsertUpToCapacityStaysOneLeaf},
        {"first overflow splits root", testFirstOverflowSplitsRoot},
        {"insert many points, validate every step", testInsertManyPointsValidatesEveryStep},
        {"insert many boxes", testInsertManyBoxes},
        {"insert grid", testInsertGrid},
        {"insert all same point", testInsertAllSamePoint},
        {"insert sorted input", testInsertSortedInput},
        {"insert negative coordinates", testInsertNegativeCoordinates},
        {"default template parameters", testDefaultParameters},

        {"query callback gets stored box", testQueryCallbackGetsStoredBox},
        {"query touching edges", testQueryTouchingEdges},
        {"query large box containing point", testQueryLargeBoxContainingPoint},
        {"query does not modify tree", testQueryDoesNotModifyTree},

        {"nearest k = 0", testNearestKZero},
        {"nearest simple", testNearestSimple},
        {"nearest k > size", testNearestMoreThanSize},
        {"nearest uses box distance", testNearestUsesBoxDistance},
        {"nearest random vs brute force", testNearestRandomAgainstBruteForce},

        {"remove single", testRemoveSingle},
        {"remove requires box and value match", testRemoveRequiresBoxAndValueMatch},
        {"remove duplicates one at a time", testRemoveDuplicatesOneAtATime},
        {"remove all random order, tree shrinks", testRemoveAllRandomOrderShrinksTree},
        {"remove in insertion order", testRemoveInInsertionOrder},
        {"remove half keeps other half", testRemoveHalfKeepsOtherHalf},

        {"mixed workload M=4", testMixedSmall},
        {"mixed workload M=8", testMixedMid},
        {"mixed workload M=16", testMixedDefault},

        {"clear", testClear},
        {"move construct and assign", testMoveConstructAndAssign},
        {"pointer values", testPointerValues},
        {"struct values", testStructValues},
    };

}  // namespace

int main() {
    int n = 0;
    for (const auto& t : kTests) {
        std::printf("[ RUN  ] %s\n", t.name);
        std::fflush(stdout);
        t.fn();  // ARCANE_CHECK exits on failure
        std::printf("[  OK  ] %s\n", t.name);
        ++n;
    }
    std::printf("All %d tests passed.\n", n);
    return 0;
}