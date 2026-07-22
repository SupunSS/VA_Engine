#pragma once
#include <memory>
#include "Model.h"

// Procedurally-generated primitive meshes — no source files, no Assimp
// involved. Useful for placeholder/default shapes (sphere, and easy to
// extend with more later) without needing to source, license, or ship a
// model file for something this geometrically simple.
namespace Primitives {

// Generates a UV sphere (latitude/longitude grid) of the given radius.
// latitudeSegments/longitudeSegments control tessellation density — higher
// values look smoother but cost more vertices; the defaults are a
// reasonable middle ground for a default placeholder shape.
std::shared_ptr<Model> CreateSphere(float radius = 0.5f, int latitudeSegments = 16, int longitudeSegments = 24);

// Generates a square-base pyramid, flat-shaded (each of its 5 faces gets
// its own vertices so lighting reads as distinct flat facets rather than
// smoothed edges, which is what a pyramid should look like). Centered on
// the origin: base sits at -height/2, apex at +height/2.
std::shared_ptr<Model> CreatePyramid(float baseHalfWidth = 0.5f, float height = 1.0f);

} // namespace Primitives