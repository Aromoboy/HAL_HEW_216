#pragma once

#include <vector>

#include "MeshBuffer.h"
#include "Vector.h"

namespace Collision
{
    // Extracts XY positions from a mesh description and computes a
    // normalized convex hull centered around its centroid for collider use.
    std::vector<float2> BuildConvexHullFromMesh(const MeshBuffer::Description& desc);
}

