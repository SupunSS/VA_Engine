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

// Generates a cylinder wheel oriented along the X axis (width along X, radius in YZ).
std::shared_ptr<Model> CreateWheel(float radius = 0.35f, float width = 0.2f, int segments = 24);

// Generates a flat-shaded box centered at the origin with given half-extents.
// Each face has its own vertices so lighting reads as distinct flat facets.
std::shared_ptr<Model> CreateBox(float halfX = 0.5f, float halfY = 0.5f, float halfZ = 0.5f);

// Generates a low-poly vehicle body mesh centered at the origin.
// The body consists of a wide lower section plus a narrower raised cabin,
// all flat-shaded for a stylized look. Dimensions should match the physics
// chassis half-extents (halfX, halfY, halfZ) passed in.
// cabinHeightFraction controls how tall the cabin is relative to halfY.
std::shared_ptr<Model> CreateVehicleBody(
    float halfX = 0.9f, float halfY = 0.4f, float halfZ = 1.8f,
    float cabinHeightFraction = 1.1f);

} // namespace Primitives