#pragma once

#include <raylib.h>
#include <vector>

namespace fps {

struct Map;

std::vector<int> findPath(const Map& map, Vector3 from, Vector3 to);
int nearestWaypoint(const Map& map, Vector3 p);

} // namespace fps
