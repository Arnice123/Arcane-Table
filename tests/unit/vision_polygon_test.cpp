// Correctness tests for PolygonBackend (ARCH-04 spike, Piece 7).
//
// Cases 1-6 are hand-built scenes with known answers. Case 7 compares the
// backend with the brute-force oracle on 100,000 random (viewer, point) pairs
// over the canonical stress scene and prints the disagreement rates for the ADR.
//
// Every hand-built case runs twice: once as written (near the origin) and once
// shifted far away, because several classic bugs (e.g. passing a point where a
// direction is expected) only work when the viewer happens to sit at (0, 0).
//
// "Visible" means: within the viewer's radius AND no wall crosses the straight
// line from the viewer to the point. A visible point must pass both contains()
// and line_of_sight(); a hidden point inside the radius must fail both.
//
// Random data uses std::mt19937 (identical on every compiler) and converts its
// raw output to floats by hand, because std::uniform_real_distribution is not
// guaranteed to give the same numbers across standard libraries.

#include "arcane/core/vision/polygon_backend.hpp"

#include "check.hpp"        // ARCANE_CHECK; adjust to the real header name
#include "stress_scene.hpp"        // make_stress_scene

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <random>
#include <span>
#include <string_view>
#include <vector>

namespace {

    // -------------------------------------------------------------------------
    // Helpers (only use Vec2's x/y, so they don't depend on the Math API)
    // -------------------------------------------------------------------------

    constexpr float kPi = 3.14159265358979f;

    Vec2 add(Vec2 a, Vec2 b) { return Vec2{ a.x + b.x, a.y + b.y }; }

    float dist2(Vec2 a, Vec2 b) {
        const float dx = a.x - b.x, dy = a.y - b.y;
        return dx * dx + dy * dy;
    }

    bool isFinite(Vec2 p) { return std::isfinite(p.x) && std::isfinite(p.y); }

    // Distance from p to the closest point of segment s.
    float distToSegment(Vec2 p, const Segment& s) {
        const float dx = s.b.x - s.a.x, dy = s.b.y - s.a.y;
        const float len2 = dx * dx + dy * dy;
        float t = len2 > 0 ? ((p.x - s.a.x) * dx + (p.y - s.a.y) * dy) / len2 : 0.0f;
        t = std::fmax(0.0f, std::fmin(1.0f, t));
        return std::sqrt(dist2(p, Vec2{ s.a.x + t * dx, s.a.y + t * dy }));
    }

    // Points lying on a wall's face: the backend counts them visible (you can see a
    // wall), the oracle counts touching as blocked. Neither is wrong, so grid-based
    // comparisons skip points this close to a wall.
    bool onAWall(std::span<const Segment> walls, Vec2 p) {
        for (const auto& w : walls) if (distToSegment(p, w) < 1e-3f) return true;
        return false;
    }

    // Uniform float in [lo, hi) from a 32-bit mt19937 draw.
    float uniform(std::mt19937& g, float lo, float hi) {
        return lo + (hi - lo) * (static_cast<float>(g() >> 8) / 16777216.0f);
    }

    // The arc of the radius is approximated by straight edges, so points in a
    // very thin band just inside the radius may be reported hidden. Hand-built
    // cases keep their points out of that band; case 7 reports it separately.
    constexpr float kInnerSafe = 0.99f;  // points at <= 0.99 r must be visible (if unblocked)
    constexpr float kOuterSafe = 1.01f;  // points at >= 1.01 r must be hidden

    // -------------------------------------------------------------------------
    // A hand-built scene: walls, one viewer, and points with known answers.
    // -------------------------------------------------------------------------

    struct Case {
        const char* name;
        std::vector<Segment> walls;
        Viewer viewer;
        std::vector<Vec2> visible;  // must be seen
        std::vector<Vec2> hidden;   // must not be seen
    };

    Case shifted(const Case& c, Vec2 by) {
        Case s = c;
        for (auto& w : s.walls) { w.a = add(w.a, by); w.b = add(w.b, by); }
        s.viewer.origin = add(s.viewer.origin, by);
        for (auto& p : s.visible) p = add(p, by);
        for (auto& p : s.hidden) p = add(p, by);
        return s;
    }

    // Checks every listed point against contains() and line_of_sight(), and
    // basic sanity of the outline.
    void checkCase(IVisibilityBackend& b, const Case& c) {
        b.set_occluders(c.walls);
        auto field = b.compute(c.viewer);
        ARCANE_CHECK(field != nullptr);

        const Vec2 o = c.viewer.origin;
        const float r = c.viewer.radius;

        for (const Vec2& p : c.visible) {
            if (!field->contains(p) || !b.line_of_sight(o, p)) {
                std::fprintf(stderr, "  case '%s': (%g, %g) should be visible from (%g, %g)\n",
                    c.name, p.x, p.y, o.x, o.y);
            }
            ARCANE_CHECK(field->contains(p));
            ARCANE_CHECK(b.line_of_sight(o, p));
            ARCANE_CHECK(b.line_of_sight(p, o));  // symmetric
        }
        for (const Vec2& p : c.hidden) {
            if (field->contains(p)) {
                std::fprintf(stderr, "  case '%s': (%g, %g) should be hidden from (%g, %g)\n",
                    c.name, p.x, p.y, o.x, o.y);
            }
            ARCANE_CHECK(!field->contains(p));
            // Inside the radius, hidden can only mean "blocked by a wall".
            if (dist2(o, p) <= r * r) {
                ARCANE_CHECK(!b.line_of_sight(o, p));
                ARCANE_CHECK(!b.line_of_sight(p, o));
            }
        }

        // Outline: at least a triangle, every point finite and within the radius.
        const auto outline = field->outline();
        ARCANE_CHECK(outline.size() >= 3);
        for (const Vec2& p : outline) {
            ARCANE_CHECK(isFinite(p));
            ARCANE_CHECK(dist2(o, p) <= r * r * 1.0001f + 1e-6f);
        }
    }

    // Runs a case as written and shifted far from the origin.
    void runCase(const Case& c) {
        PolygonBackend b;
        checkCase(b, c);
        PolygonBackend b2;
        checkCase(b2, shifted(c, Vec2{ 731.25f, -412.5f }));
    }

    // -------------------------------------------------------------------------
    // 1. No walls
    // -------------------------------------------------------------------------

    void testNoWalls() {
        PolygonBackend b;
        b.set_occluders({});
        for (Vec2 o : { Vec2{ 0, 0 }, Vec2{ 37.5f, -12.25f } }) {
            const Viewer v{ o, 10 };
            auto field = b.compute(v);
            // A grid of points over the viewer's square and a bit beyond.
            for (int iy = -24; iy <= 24; ++iy) {
                for (int ix = -24; ix <= 24; ++ix) {
                    const Vec2 p{ o.x + ix * 0.5f, o.y + iy * 0.5f };
                    const float d2 = dist2(o, p);
                    ARCANE_CHECK(b.line_of_sight(o, p));  // nothing can block
                    if (d2 <= (kInnerSafe * 10) * (kInnerSafe * 10)) ARCANE_CHECK(field->contains(p));
                    if (d2 >= (kOuterSafe * 10) * (kOuterSafe * 10)) ARCANE_CHECK(!field->contains(p));
                }
            }
            ARCANE_CHECK(field->contains(o));  // the viewer's own spot
        }
    }

    void testOutlineIsOrderedAroundViewer() {
        // The outline must go round the viewer once, in angle order, so a renderer
        // can draw it as a fan. Checked with walls so corner points are included.
        PolygonBackend b;
        const std::vector<Segment> walls = { {{3, -2}, {3, 2}}, {{-4, 1}, {-1, 5}}, {{-2, -6}, {2, -3}} };
        b.set_occluders(walls);
        const Vec2 o{ 0.3f, 0.2f };
        const auto outline = b.compute(Viewer{ o, 12 })->outline();
        ARCANE_CHECK(outline.size() >= 3);
        int wraps = 0;
        float prev = std::atan2(outline.back().y - o.y, outline.back().x - o.x);
        for (const Vec2& p : outline) {
            const float a = std::atan2(p.y - o.y, p.x - o.x);
            if (a < prev - kPi) ++wraps;            // passed from +pi back to -pi
            else ARCANE_CHECK(a >= prev - 1e-3f);   // otherwise non-decreasing
            prev = a;
        }
        ARCANE_CHECK(wraps == 1);  // exactly one full turn
    }

    // -------------------------------------------------------------------------
    // 2. One wall in front of the viewer
    // -------------------------------------------------------------------------

    void testOneWall() {
        runCase(Case{
            "one wall",
            { {{5, -2}, {5, 2}} },
            Viewer{ {0, 0}, 20 },
            /*visible*/ { {4, 0}, {8, 5}, {8, -5}, {0, 10}, {-10, 0}, {4.9f, 1.9f} },
            /*hidden */ { {8, 0}, {6, 1}, {19, -1}, {10, 3.5f}, {5.5f, 0} },
            });
    }

    // -------------------------------------------------------------------------
    // 3. Viewer next to a corner (two walls sharing an endpoint)
    // -------------------------------------------------------------------------

    void testCornerOutside() {
        // L-shape with its corner at (2, 2); the viewer is outside the corner.
        runCase(Case{
            "corner, viewer outside",
            { {{2, 2}, {2, 10}}, {{2, 2}, {10, 2}} },
            Viewer{ {1, 1.5f}, 20 },
            /*visible: along both outer faces*/ { {1.5f, 9}, {9, 1.5f}, {1.9f, 9.9f}, {9.9f, 1.9f}, {-5, -5} },
            /*hidden: inside the L, behind the corner*/ { {5, 5}, {2.5f, 2.5f}, {9, 9}, {3, 8} },
            });
    }

    void testCornerInside() {
        // Same L, viewer inside it: sees inside, not through either wall.
        runCase(Case{
            "corner, viewer inside",
            { {{2, 2}, {2, 10}}, {{2, 2}, {10, 2}} },
            Viewer{ {4, 4}, 20 },
            /*visible*/ { {3, 9}, {9, 3}, {2.5f, 2.5f}, {15, 15} },
            /*hidden */ { {1, 5}, {5, 1}, {1, 1.5f}, {0, 9} },
            });
    }

    // -------------------------------------------------------------------------
    // 4. Closed room
    // -------------------------------------------------------------------------

    // Test points avoid lines through the room's exact corners: whether a sight line
    // grazing a corner counts as blocked is a float coin-toss, not a meaningful answer.
    std::vector<Segment> roomWalls() {
        return { {{0, 0}, {10, 0}}, {{10, 0}, {10, 10}}, {{10, 10}, {0, 10}}, {{0, 10}, {0, 0}} };
    }

    void testClosedRoom() {
        runCase(Case{
            "closed room, viewer centre",
            roomWalls(),
            Viewer{ {5, 5}, 30 },
            /*visible: anywhere inside*/ { {1, 1}, {9, 9}, {0.5f, 9.5f}, {5, 9.9f}, {9.9f, 0.1f} },
            /*hidden: anywhere outside*/ { {-1, 5}, {11, 5}, {5, 12}, {15, 14}, {-0.1f, -0.5f} },
            });
        runCase(Case{
            "closed room, viewer near a corner",
            roomWalls(),
            Viewer{ {0.5f, 0.5f}, 30 },
            /*visible*/ { {9.5f, 9.5f}, {9.5f, 0.5f}, {0.5f, 9.5f} },
            /*hidden */ { {-1, -0.5f}, {12, 5}, {5, 11} },
            });
    }

    void testClosedRoomOutlineStaysInside() {
        // The whole visible shape must stay inside the room (small tolerance).
        PolygonBackend b;
        b.set_occluders(roomWalls());
        for (Vec2 o : { Vec2{ 5, 5 }, Vec2{ 1, 8.5f }, Vec2{ 9.9f, 0.1f } }) {
            for (const Vec2& p : b.compute(Viewer{ o, 30 })->outline()) {
                ARCANE_CHECK(p.x >= -1e-3f && p.x <= 10 + 1e-3f);
                ARCANE_CHECK(p.y >= -1e-3f && p.y <= 10 + 1e-3f);
            }
        }
    }

    // -------------------------------------------------------------------------
    // 5. A small gap in a wall
    // -------------------------------------------------------------------------

    void testGap() {
        // Wall along x = 5 with a gap for y in (-0.5, 0.5).
        runCase(Case{
            "gap in a wall",
            { {{5, -10}, {5, -0.5f}}, {{5, 0.5f}, {5, 10}} },
            Viewer{ {0, 0}, 30 },
            // Through the gap: the sight line crosses x = 5 at |y| < 0.5.
            /*visible*/ { {10, 0}, {10, 0.8f}, {10, -0.8f}, {20, 0}, {20, 1.6f}, {4, 8} },
            // Beside the gap: it crosses x = 5 on the wall.
            /*hidden */ { {10, 3}, {10, 1.2f}, {10, -1.2f}, {20, 2.4f}, {20, -2.4f}, {7, 9} },
            });
    }

    // -------------------------------------------------------------------------
    // 6. Viewer exactly on a wall endpoint
    // -------------------------------------------------------------------------

    void checkSaneOutline(const VisibilityField& f, const Viewer& v) {
        const auto outline = f.outline();
        ARCANE_CHECK(outline.size() >= 3);
        for (const Vec2& p : outline) {
            ARCANE_CHECK(isFinite(p));
            ARCANE_CHECK(dist2(v.origin, p) <= v.radius * v.radius * 1.0001f + 1e-6f);
        }
    }

    void testViewerOnWallEndpoint() {
        // "Sane" here means: no crash, a finite outline inside the radius, and the
        // viewer can still see open space that no wall is between it and, and
        // contains() agrees with line_of_sight() for those points.
        PolygonBackend b;

        // A single wall starting at the viewer.
        {
            b.set_occluders(std::vector<Segment>{ {{0, 0}, { 5, 0 }} });
            const Viewer v{ {0, 0}, 10 };
            auto f = b.compute(v);
            checkSaneOutline(*f, v);
            for (Vec2 p : { Vec2{ -3, -3 }, Vec2{ 3, 3 }, Vec2{ 0, 6 }, Vec2{ -6, 0 }, Vec2{ 2, -4 } }) {
                ARCANE_CHECK(b.line_of_sight(v.origin, p));
                ARCANE_CHECK(f->contains(p));
            }
        }
        // Two walls meeting at the viewer (it stands in a corner, open on the outside).
        {
            b.set_occluders(std::vector<Segment>{ {{0, 0}, { 5, 0 }}, { {0, 0}, {0, 5} } });
            const Viewer v{ {0, 0}, 10 };
            auto f = b.compute(v);
            checkSaneOutline(*f, v);
            for (Vec2 p : { Vec2{ 3, 3 }, Vec2{ -3, -3 }, Vec2{ -4, 2 } }) {
                ARCANE_CHECK(b.line_of_sight(v.origin, p));
                ARCANE_CHECK(f->contains(p));
            }
        }
    }

    // -------------------------------------------------------------------------
    // Other traps from the spec
    // -------------------------------------------------------------------------

    void testRayAlongWall() {
        // The viewer is in line with a wall: some rays run exactly along it.
        runCase(Case{
            "ray along a wall",
            { {{2, 0}, {6, 0}} },
            Viewer{ {0, 0}, 20 },
            /*visible: just off the wall's line, either side*/ { {8, 0.5f}, {8, -0.5f}, {1, 0}, {-5, 0}, {4, 1} },
            /*hidden : none are blocked here*/ {},
            });
    }

    void testSetOccludersReplaces() {
        PolygonBackend b;
        const Viewer v{ {0, 0}, 20 };
        const Vec2 behind{ 8, 0 };

        b.set_occluders(std::vector<Segment>{ {{5, -2}, { 5, 2 }} });
        ARCANE_CHECK(!b.compute(v)->contains(behind));
        ARCANE_CHECK(!b.line_of_sight(v.origin, behind));

        // Replacing the walls must forget the old ones entirely.
        b.set_occluders(std::vector<Segment>{ {{-5, -2}, { -5, 2 }} });
        ARCANE_CHECK(b.compute(v)->contains(behind));
        ARCANE_CHECK(b.line_of_sight(v.origin, behind));
        ARCANE_CHECK(!b.compute(v)->contains(Vec2{ -8, 0 }));

        b.set_occluders({});
        ARCANE_CHECK(b.compute(v)->contains(Vec2{ -8, 0 }));
    }

    void testComputeIsRepeatable() {
        // compute() is const: calling it twice must give the same shape.
        const StressScene scene = make_stress_scene(1);
        PolygonBackend b;
        b.set_occluders(scene.walls);
        for (const Viewer& v : scene.viewers) {
            const auto a = b.compute(v)->outline();
            const auto c = b.compute(v)->outline();
            ARCANE_CHECK(a.size() == c.size());
            for (std::size_t i = 0; i < a.size(); ++i)
                ARCANE_CHECK(a[i].x == c[i].x && a[i].y == c[i].y);
        }
    }

    void testTinyRadius() {
        PolygonBackend b;
        b.set_occluders(roomWalls());
        for (float r : { 0.0f, 1e-6f, 0.01f }) {
            auto f = b.compute(Viewer{ {5, 5}, r });
            ARCANE_CHECK(f != nullptr);
            ARCANE_CHECK(!f->contains(Vec2{ 5 + 2 * r + 0.1f, 5 }));  // beyond the radius
            for (const Vec2& p : f->outline()) ARCANE_CHECK(isFinite(p));
        }
    }

    void testName() {
        PolygonBackend b;
        ARCANE_CHECK(b.name() == std::string_view("polygon"));
    }


    // -------------------------------------------------------------------------
    // Regressions found by case 7
    // -------------------------------------------------------------------------

    void testCrossingWalls() {
        // Two walls crossing at (3, 4.4). Where they cross, the visible boundary turns a
        // corner that is NOT an endpoint of either wall, so it needs its own rays.
        // Without them the polygon cuts straight across the corner and points just in
        // front of the crossing are wrongly hidden.
        const std::vector<Segment> walls = { {{3, -3}, {3, 6}}, {{1, 6}, {6, 2}} };
        PolygonBackend b;
        b.set_occluders(walls);
        for (Vec2 o : { Vec2{ 0, 0 }, Vec2{ -0.7f, 0.9f }, Vec2{ 0.4f, -1.3f } }) {
            const Viewer v{ o, 20 };
            auto f = b.compute(v);
            // A fine grid around the crossing: compare each point with the oracle.
            int wrong = 0;
            for (int iy = 0; iy <= 40; ++iy) {
                for (int ix = 0; ix <= 40; ++ix) {
                    const Vec2 p{ 2.6f + ix * 0.01f, 4.0f + iy * 0.01f };
                    if (onAWall(walls, p)) continue;
                    const bool truth = oracle_line_of_sight(walls, o, p);
                    if (f->contains(p) != truth) ++wrong;
                }
            }
            if (wrong) std::fprintf(stderr, "  crossing walls, viewer (%g, %g): %d grid points wrong\n", o.x, o.y, wrong);
            ARCANE_CHECK(wrong == 0);
        }
    }

    void testFarFromOriginNearWall() {
        // A viewer far from (0, 0), with a wall very close to it and many small walls
        // behind that wall. The corners behind produce rays only ~1e-4 rad apart, so
        // their hit points on the near wall are ~1e-5 units apart: smaller than float
        // resolution at coordinates around 5000 (~5e-4). If the field stores points in
        // map coordinates those near-duplicate points get rounded together, the edges
        // between them point in random directions, and contains() lets points behind
        // the near wall through. Storing points relative to the viewer avoids it.
        const Vec2 o{ 5000.37f, 5000.61f };
        std::vector<Segment> walls = { {{o.x + 0.1f, o.y - 30}, {o.x + 0.1f, o.y + 30}} };  // 0.1 to the right
        std::mt19937 g(7);
        for (int i = 0; i < 60; ++i) {
            const Vec2 a{ o.x + uniform(g, 1, 11), o.y + uniform(g, -8, 8) };
            walls.push_back({ a, Vec2{ a.x + uniform(g, -0.5f, 0.5f), a.y + uniform(g, -0.5f, 0.5f) } });
        }
        PolygonBackend b;
        b.set_occluders(walls);
        auto f = b.compute(Viewer{ o, 12 });
        // Everything right of the near wall (and inside the radius) must be hidden.
        int leaked = 0;
        for (int iy = -80; iy <= 80; ++iy) {
            for (int ix = 1; ix <= 100; ++ix) {
                const Vec2 p{ o.x + 0.1f + ix * 0.1f, o.y + iy * 0.1f };
                if (dist2(o, p) <= 11.9f * 11.9f && f->contains(p)) ++leaked;
            }
        }
        if (leaked) std::fprintf(stderr, "  %d points behind the near wall reported visible\n", leaked);
        ARCANE_CHECK(leaked == 0);
        // And open space on the viewer's side still works.
        ARCANE_CHECK(f->contains(Vec2{ o.x - 5, o.y + 1 }));
    }

    // -------------------------------------------------------------------------
    // 7. Stress scene vs oracle: 100,000 random (viewer, point) pairs
    // -------------------------------------------------------------------------

    void testStressSceneAgainstOracle() {
        const StressScene scene = make_stress_scene(1);
        const std::span<const Segment> walls(scene.walls);

        PolygonBackend b;
        b.set_occluders(scene.walls);

        // 1,000 random viewers (radius 12, like the scene's viewers) x 100 points
        // each, the points spread over the viewer's square so ~79% fall inside
        // the radius and the rest test the "beyond the radius" edge.
        constexpr int kViewers = 1000;
        constexpr int kPointsPerViewer = 100;
        constexpr float kRadius = 12;
        std::mt19937 g(20261115u);

        long pairs = 0;
        long containsWrong = 0;       // contains() != oracle truth
        long containsWrongArc = 0;    //   ...of which in the thin band just inside the radius
        long containsFalseVisible = 0, containsFalseHidden = 0;
        long losWrong = 0;            // line_of_sight() != oracle (radius not involved)

        for (int i = 0; i < kViewers; ++i) {
            const Viewer v{ Vec2{ uniform(g, 0, 100), uniform(g, 0, 100) }, kRadius };
            auto field = b.compute(v);
            for (int j = 0; j < kPointsPerViewer; ++j) {
                const Vec2 p{ uniform(g, v.origin.x - kRadius, v.origin.x + kRadius),
                              uniform(g, v.origin.y - kRadius, v.origin.y + kRadius) };
                const bool oracleLos = oracle_line_of_sight(walls, v.origin, p);
                const bool inRadius = dist2(v.origin, p) <= kRadius * kRadius;
                const bool truth = inRadius && oracleLos;

                const bool got = field->contains(p);
                if (got != truth) {
                    ++containsWrong;
                    if (got) ++containsFalseVisible; else ++containsFalseHidden;
                    if (dist2(v.origin, p) >= (kInnerSafe * kRadius) * (kInnerSafe * kRadius))
                        ++containsWrongArc;
                }
                if (b.line_of_sight(v.origin, p) != oracleLos) ++losWrong;
                ++pairs;
            }
        }

        const double n = static_cast<double>(pairs);
        std::printf("    pairs: %ld\n", pairs);
        std::printf("    contains vs oracle: %ld wrong (%.4f%%): %ld said visible, %ld said hidden\n",
            containsWrong, 100.0 * containsWrong / n, containsFalseVisible, containsFalseHidden);
        std::printf("      of which in the outer %.0f%% band of the radius (arc approximation): %ld\n",
            100.0 * (1 - kInnerSafe), containsWrongArc);
        std::printf("    line_of_sight vs oracle: %ld wrong (%.4f%%)\n", losWrong, 100.0 * losWrong / n);

        // Polygon should be essentially exact. Away from the arc band, allow only
        // razor-thin float cases; the arc band itself is reported, with a loose cap.
        ARCANE_CHECK(losWrong <= pairs / 10000);                          // <= 0.01%
        ARCANE_CHECK(containsWrong - containsWrongArc <= pairs / 10000);  // <= 0.01%
        ARCANE_CHECK(containsWrong <= pairs / 200);                       // <= 0.5% in total
    }

    // -------------------------------------------------------------------------

    struct TestCase {
        const char* name;
        void (*fn)();
    };

    const TestCase kTests[] = {
        {"1. no walls", testNoWalls},
        {"   outline is ordered around the viewer", testOutlineIsOrderedAroundViewer},
        {"2. one wall in front", testOneWall},
        {"3. corner, viewer outside", testCornerOutside},
        {"3. corner, viewer inside", testCornerInside},
        {"4. closed room", testClosedRoom},
        {"4. closed room outline stays inside", testClosedRoomOutlineStaysInside},
        {"5. gap in a wall", testGap},
        {"6. viewer on a wall endpoint", testViewerOnWallEndpoint},
        {"   ray along a wall", testRayAlongWall},
        {"   set_occluders replaces walls", testSetOccludersReplaces},
        {"   compute is repeatable", testComputeIsRepeatable},
        {"   tiny radius", testTinyRadius},
        {"   name", testName},
        {"   crossing walls", testCrossingWalls},
        {"   far from origin, wall right beside viewer", testFarFromOriginNearWall},
        {"7. stress scene vs oracle (100,000 pairs)", testStressSceneAgainstOracle},
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