#pragma once

#include "arcane\core\vision\visibility_backend.hpp"

class PolygonBackend final : public IVisibilityBackend {
public:
    std::string_view name() const override;
    void set_occluders(std::span<const Segment> walls) override;
    std::unique_ptr<VisibilityField> compute(const Viewer& viewer) const override;
    bool line_of_sight(Vec2 a, Vec2 b) const override;
private:
    std::span<const Segment> walls;
};