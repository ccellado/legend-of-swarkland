#include "path_finding.hpp"

#include "swarkland.hpp"
#include "heap.hpp"

#include <math.h>

bool do_i_think_i_can_move_here(Individual individual, Coord location) {
    if (!is_in_bounds(location))
        return false;
    if (individual->knowledge.tiles[location].tile_type == TileType_WALL)
        return false;
    if (find_individual_at(location) != NULL)
        return false;
    return true;
}

// start with the cardinal directions, because these are more "direct"
const Coord directions[] = {
    {-1,  0},
    { 0, -1},
    { 1,  0},
    { 0,  1},
    {-1, -1},
    { 1, -1},
    { 1,  1},
    {-1,  1},
};

struct Node {
    Coord coord;
    float f;
    float g;
    float h;
    Node * parent;
};

static float heuristic(Coord start, Coord end) {
    return sqrtf(distance_squared(start, end));
}

static int compare_nodes(Node *a, Node *b) {
    return signf(a->f - b->f);
}

bool find_path(Coord start, Coord end, Individual according_to_whom, List<Coord> & output_path) {
    Matrix<bool> closed_set(map_size);
    closed_set.set_all(false);

    Heap<Node*, compare_nodes> open_heap;
    Matrix<bool> open_set(map_size);
    open_set.set_all(false);

    Matrix<Node> nodes(map_size);
    Node *start_node = &nodes[start];
    start_node->coord = start;
    start_node->h = heuristic(start, end);
    start_node->g = 0.0;
    start_node->f = start_node->g + start_node->h;
    start_node->parent = NULL;
    open_heap.insert(start_node);
    open_set[start_node->coord] = true;
    bool found_goal = false;
    Node *best_node = start_node;
    while (open_heap.size() > 0) {
        Node *node = open_heap.extract_min();
        open_set[node->coord] = false;

        if (node->h < best_node->h)
            best_node = node;
        if (node->coord == end) {
            found_goal = true;
            break;
        }
        // not done yet
        closed_set[node->coord] = true;
        for (int i = 0; i < 8; i++) {
            Coord direction = directions[i];
            Coord neighbor_coord = {node->coord.x + direction.x, node->coord.y + direction.y};
            if (!is_in_bounds(neighbor_coord))
                continue;

            if (neighbor_coord != end && !do_i_think_i_can_move_here(according_to_whom, neighbor_coord))
                continue;
            if (closed_set[neighbor_coord])
                continue;

            Node *neighbor = &nodes[neighbor_coord];
            neighbor->coord = neighbor_coord;

            float g_from_this_node = node->g + 1.0f;
            if (!open_set[neighbor->coord] || g_from_this_node < neighbor->g) {
                neighbor->parent = node;
                neighbor->g = g_from_this_node;
                neighbor->h = heuristic(neighbor->coord, end);
                neighbor->f = neighbor->g + neighbor->h;
                if (!open_set[neighbor->coord]) {
                    open_heap.insert(neighbor);
                    open_set[neighbor->coord] = true;
                }
            }
        }
    }
    // construct path
    // take a double dump
    List<Coord> backwards_path;
    Node *it = best_node;
    while (it != NULL) {
        backwards_path.add(it->coord);
        it = it->parent;
    }
    for (int i = backwards_path.size() - 2; i >= 0; i--) {
        output_path.add(backwards_path[i]);
    }
    return found_goal;
}
