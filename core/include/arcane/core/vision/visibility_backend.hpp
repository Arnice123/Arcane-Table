#pragma once

#include <string>
#include <span>
#include <vector>
#include <memory>

#include "arcane/core/geometry/segment.hpp"
#include "arcane/core/geometry/math.hpp"

struct Viewer {
	Vec2 origin;
	float radius;
};

// what does this viewer see
class VisibilityField {
public:
	virtual ~VisibilityField() = default;

	// In: A point
	// Out: True if point is within view
	virtual bool contains(Vec2 point) const = default;

	// Out: The boundry of the viewers view 
	virtual std::vector<Vec2> outline() const = 0;
};

class IVisibilityBackend {
public:
	virtual ~IVisibilityBackend() = default;

	// short name for the algo
	virtual std::string_view name() const = 0;

	// returns a list of walls that block visibility
	virtual void set_occuluders(std::span<const Segment> walls) = 0;

	// In: A viewer
	// Out: Outputs a field with what the viewers see
	virtual std::unique_ptr<VisibilityField> compute(const Viewer& viewer) const = 0;

	// In: Two points A and B
	// Out: True if a can see b (no walls between)
	virtual bool line_of_sight(Vec2 a, Vec2 b) const = 0;
};