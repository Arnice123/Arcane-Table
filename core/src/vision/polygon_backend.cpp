#include "arcane\core\vision\polygon_backend.hpp"

std::string_view PolygonBackend::name() const
{
	return "polygon";
}

void PolygonBackend::set_occuluders(std::span<const Segment> walls)
{
	(void)walls;
}

std::unique_ptr<VisibilityField> PolygonBackend::compute(const Viewer& viewer) const
{
	(void)viewer;
	return nullptr;
}

bool PolygonBackend::line_of_sight(Vec2 a, Vec2 b) const
{
	(void)a; (void)b;
	return false;
}