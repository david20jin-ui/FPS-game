#include "Nav.h"

#include "../core/Math.h"
#include "../world/Map.h"

#include <algorithm>
#include <limits>
#include <queue>
#include <unordered_map>
#include <vector>

namespace fps {

int nearestWaypoint(const Map& map, Vector3 p) {
    int best = -1;
    float bestD = std::numeric_limits<float>::max();
    for (size_t i = 0; i < map.navGraph.size(); ++i) {
        float d = v3Len(v3Sub(p, map.navGraph[i].position));
        if (d < bestD) { bestD = d; best = static_cast<int>(i); }
    }
    return best;
}

std::vector<int> findPath(const Map& map, Vector3 from, Vector3 to) {
    int start = nearestWaypoint(map, from);
    int goal  = nearestWaypoint(map, to);
    if (start < 0 || goal < 0) return {};
    if (start == goal) return {goal};

    struct Node { float f; int idx; };
    auto cmp = [](const Node& a, const Node& b) { return a.f > b.f; };
    std::priority_queue<Node, std::vector<Node>, decltype(cmp)> open(cmp);
    std::unordered_map<int, float> gScore;
    std::unordered_map<int, int>   cameFrom;

    auto h = [&](int i) {
        return v3Len(v3Sub(map.navGraph[i].position, map.navGraph[goal].position));
    };

    gScore[start] = 0.0f;
    open.push({h(start), start});

    while (!open.empty()) {
        Node cur = open.top();
        open.pop();
        if (cur.idx == goal) {
            std::vector<int> path;
            int i = goal;
            while (true) {
                path.push_back(i);
                if (i == start) break;
                auto it = cameFrom.find(i);
                if (it == cameFrom.end()) return {};
                i = it->second;
            }
            std::reverse(path.begin(), path.end());
            return path;
        }
        for (int nb : map.navGraph[cur.idx].neighbors) {
            float step = v3Len(v3Sub(map.navGraph[cur.idx].position,
                                     map.navGraph[nb].position));
            float tentative = gScore[cur.idx] + step;
            auto it = gScore.find(nb);
            if (it == gScore.end() || tentative < it->second) {
                gScore[nb]  = tentative;
                cameFrom[nb] = cur.idx;
                open.push({tentative + h(nb), nb});
            }
        }
    }
    return {};
}

} // namespace fps
