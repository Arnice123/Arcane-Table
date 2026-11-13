#pragma once

// includes: <cstdint>, <memory>, <span>, <string_view>, <vector>,
//           your IVisibilityBackend header, the r_tree header

#include "arcane\core\util\r_tree.hpp"
#include "arcane\core\vision\visibility_backend.hpp"
#include "arcane\core\geometry\segment.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

// Visibility polygon by casting rays at wall endpoints.
// Walls live in an R*-tree so compute() and line_of_sight() only look at walls near the viewer / near the sight line, not every wall in the map.
class PolygonBackend final : public IVisibilityBackend {
public:
    std::string_view name() const override; // "polygon"
    void set_occluders(std::span<const Segment> walls) override;
    std::unique_ptr<VisibilityField> compute(const Viewer& viewer) const override;
    bool line_of_sight(Vec2 a, Vec2 b) const override;

private:
    // A copy of the walls. The caller's span may not outlive the call, so the tree must not point into it.
    std::vector<Segment> walls_;

    // The tree's value for each wall is that wall's index in walls_.
    rstar::RStarTree<std::uint32_t> index_;
};