/*
 * Copyright 2012, 2013 Moritz Hilscher
 *
 * This file is part of mapcrafter.
 *
 * mapcrafter is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * mapcrafter is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with mapcrafter.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "tile.h"

#include "../mc/pos.h"

#include "render.h"

#include "../util.h"

#include <iostream>
#include <sstream>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <stdint.h>
#include <set>

namespace mapcrafter {
namespace render {

TilePos::TilePos()
		: x(0), y(0) {
}

TilePos::TilePos(int x, int y)
		: x(x), y(y) {
}

int TilePos::getX() const {
	return x;
}

int TilePos::getY() const {
	return y;
}

TilePos& TilePos::operator+=(const TilePos& p) {
	x += p.x;
	y += p.y;
	return *this;
}

TilePos& TilePos::operator-=(const TilePos& p) {
	x -= p.x;
	y -= p.y;
	return *this;
}

TilePos TilePos::operator+(const TilePos& p2) const {
	TilePos p = *this;
	return p += p2;
}

TilePos TilePos::operator-(const TilePos& p2) const {
	TilePos p = *this;
	return p -= p2;
}

bool TilePos::operator==(const TilePos& other) const {
	return x == other.x && y == other.y;
}

bool TilePos::operator<(const TilePos& other) const {
	if (x == other.x)
		return y < other.y;
	return x < other.x;
}

Path::Path() {
}

Path::Path(const std::vector<int>& path)
		: path(path) {
}

Path::~Path() {
}

const std::vector<int>& Path::getPath() const {
	return path;
}

int Path::getDepth() const {
	return path.size();
}

/**
 * This method calculates the
 */
TilePos Path::getTilePos() const {
	// calculate the radius of all tiles on the top zoom level (2^zoomlevel / 2)
	int radius = pow(2, path.size()) / 2;
	// the startpoint is top left
	int x = -radius;
	int y = -radius;
	for (size_t i = 0; i < path.size(); i++) {
		// now for every zoom level:
		// get the current tile
		int tile = path[i];
		// increase x by the radius if this tile is on the right side (2 or 4)
		if (tile == 2 || tile == 4)
			x += radius;
		// increase y by the radius if this tile is bottom (3 or 4)
		if (tile == 3 || tile == 4)
			y += radius;
		// divide size by two, because the next zoom level has only the half radius
		radius /= 2;
	}
	return TilePos(x, y);
}

std::string Path::toString() const {
	std::stringstream ss;
	for (size_t i = 0; i < path.size(); i++) {
		ss << path[i];
		if (i != path.size() - 1)
			ss << "/";
	}
	return ss.str();
}

Path& Path::operator+=(int node) {
	path.push_back(node);
	return *this;
}

Path Path::operator+(int node) const {
	Path copy(path);
	copy.path.push_back(node);
	return copy;
}

Path Path::parent() const {
	Path copy(path);
	copy.path.pop_back();
	return copy;
}

bool Path::operator==(const Path& other) const {
	return path == other.path;
}

bool Path::operator<(const Path& other) const {
	return path < other.path;
}

/**
 * This method calculates the path by a specific tile position on a specific level.
 * This is the opposite of Path::getTilePos().
 */
Path Path::byTilePos(const TilePos& tile, int depth) {
	Path path;

	// at first calculate the radius in tiles of this zoom level
	int radius = pow(2, depth) / 2;
	// check if the tile is in this bounds
	if (tile.getX() > radius || tile.getY() > radius || tile.getX() < -radius
	        || tile.getY() < -radius)
		throw std::runtime_error(
		        "Invalid tile position " + util::str(tile.getX()) + ":" + util::str(tile.getY())
		                + " on depth " + util::str(depth));
	// the tactic is here to calculate the bounds where the tile is inside
	int bounds_left = -radius;
	int bounds_right = radius;
	int bounds_top = radius;
	int bounds_bottom = -radius;

	for (int level = 1; level <= depth; level++) {
		// for each zoom level (but we already have the 1st zoom level):
		// calculate the midpoint in the bounds
		int middle_x = (bounds_right + bounds_left) / 2;
		int middle_y = (bounds_top + bounds_bottom) / 2;
		// with this midpoint we can decide what the next part in the path is
		// when tile x < midpoint x:
		//   and tile y < midpoint y -> top left tile
		//   and tile y >= midpoint y -> bottom left tile
		// when tile y >= midpoint y:
		//   and tile y < midpoint y -> top right tile
		//   and tile y >= midpoint y -> bottom right tile

		// if we have a midpoint, we update the bounds with the midpoint
		// the (smaller) bounds are now the part of the path, we found out
		if (tile.getX() < middle_x) {
			if (tile.getY() < middle_y) {
				path += 1;
				bounds_right = middle_x;
				bounds_top = middle_y;
			} else {
				path += 3;
				bounds_right = middle_x;
				bounds_bottom = middle_y;
			}
		} else {
			if (tile.getY() < middle_y) {
				path += 2;
				bounds_left = middle_x;
				bounds_top = middle_y;
			} else {
				path += 4;
				bounds_left = middle_x;
				bounds_bottom = middle_y;
			}
		}
	}

	return path;
}

std::ostream& operator<<(std::ostream& stream, const TilePos& tile) {
	stream << tile.getX() << ":" << tile.getY();
	return stream;
}

std::ostream& operator<<(std::ostream& stream, const Path& path) {
	stream << path.toString();
	return stream;
}

TileSet::TileSet()
		: min_depth(0), depth(0) {
}

TileSet::TileSet(const mc::World& world)
		: min_depth(0), depth(0) {
	scan(world);
}

TileSet::~TileSet() {
}

void TileSet::scan(const mc::World& world) {
	findRenderTiles(world);
	setDepth(min_depth);
}

/**
 * This method finds all render tiles, which where changed since a specific timestamp.
 */
void TileSet::scanRequiredByTimestamp(int last_change) {
	required_render_tiles.clear();

	for (std::map<TilePos, int>::iterator it = tile_timestamps.begin();
			it != tile_timestamps.end(); ++it) {
		if (it->second >= last_change)
			required_render_tiles.insert(it->first);
	}

	required_composite_tiles.clear();
	findRequiredCompositeTiles(required_render_tiles, required_composite_tiles);
}

/**
 * This method finds all render tiles, which are required based on the modification
 * times of the tile image files.
 */
void TileSet::scanRequiredByFiletimes(const fs::path& output_dir) {
	required_render_tiles.clear();

	for (std::map<TilePos, int>::iterator it = tile_timestamps.begin();
			it != tile_timestamps.end(); ++it) {
		Path path = Path::byTilePos(it->first, depth);
		fs::path file = output_dir / (path.toString() + ".png");
		//std::cout << file.string() << " " << fs::exists(file) << std::endl ;
		if (!fs::exists(file) || fs::last_write_time(file) <= it->second)
			required_render_tiles.insert(it->first);
	}

	required_composite_tiles.clear();
	findRequiredCompositeTiles(required_render_tiles, required_composite_tiles);
}

int TileSet::getMinDepth() const {
	return min_depth;
}

int TileSet::getDepth() const {
	return depth;
}

void TileSet::setDepth(int depth) {
	// only calculate the composite tiles new when the depth has changed
	if (this->depth == depth || depth < min_depth)
		return;

	this->depth = depth;

	// clear already calculated composite tiles and recalculate them
	composite_tiles.clear();
	required_composite_tiles.clear();

	findRequiredCompositeTiles(render_tiles, composite_tiles);
	findRequiredCompositeTiles(required_render_tiles, required_composite_tiles);
}

/**
 * This function calculates the tiles, a chunk covers.
 */
void getChunkTiles(const mc::ChunkPos& chunk, std::set<TilePos>& tiles) {
	// at first get row and column of the top of the chunk
	int row = chunk.getRow();
	int col = chunk.getCol();
	// then get from this the tile position (every tile is 2 columns wide and 4 rows tall)
	int base_x = col / 2;
	int base_y = row / 4;
	// now add all tiles,
	// a chunk is 9 tiles tall
	// and one or two tiles wide (two tiles if column is even)
	for (int tile_y = base_y - 1; tile_y <= base_y + 8; tile_y++) {
		tiles.insert(TilePos(base_x, tile_y));
		if (col % 2 == 0)
			tiles.insert(TilePos(base_x - 1, tile_y));
	}
}

/**
 * This method finds out, which top level tiles a world has and which of them need to
 * get rendered.
 */
void TileSet::findRenderTiles(const mc::World& world) {
	// clear maybe already calculated tiles
	render_tiles.clear();
	required_render_tiles.clear();

	// the min/max x/y coordinates of the tiles in the world
	int tiles_x_min = 0, tiles_x_max = 0, tiles_y_min = 0, tiles_y_max = 0;

	// go through all chunks in the world
	auto regions = world.getAvailableRegions();
	for (auto region_it = regions.begin(); region_it != regions.end(); ++region_it) {
		mc::RegionFile region;
		if (!world.getRegion(*region_it, region) || !region.loadHeaders())
			continue;
		const std::set<mc::ChunkPos>& region_chunks = region.getContainingChunks();
		for (auto chunk_it = region_chunks.begin(); chunk_it != region_chunks.end();
		        ++chunk_it) {
			int timestamp = region.getChunkTimestamp(*chunk_it);

			// now get all tiles of the chunk
			std::set<TilePos> tiles;
			getChunkTiles(*chunk_it, tiles);
			for (std::set<TilePos>::const_iterator tile_it = tiles.begin();
			        tile_it != tiles.end(); ++tile_it) {

				// and update the bounds
				tiles_x_min = MIN(tiles_x_min, tile_it->getX());
				tiles_x_max = MAX(tiles_x_max, tile_it->getX());
				tiles_y_min = MIN(tiles_y_min, tile_it->getY());
				tiles_y_max = MAX(tiles_y_max, tile_it->getY());

				// update tile timestamp
				if (!render_tiles.count(*tile_it))
					tile_timestamps[*tile_it] = timestamp;
				else
					tile_timestamps[*tile_it] = MAX(tile_timestamps[*tile_it], timestamp);

				// insert the tile in the set of available render tiles
				// and also make it required by default
				render_tiles.insert(*tile_it);
				required_render_tiles.insert(*tile_it);
			}
		}
	}

	// now get the necessary depth of the tile quadtree
	for (min_depth = 0; min_depth < 32; min_depth++) {
		// for each level calculate the radius and check if the tiles fit in this bounds
		int radius = pow(2, min_depth) / 2;
		if (tiles_x_min > -radius && tiles_x_max < radius && tiles_y_min > -radius
		        && tiles_y_max < radius)
			break;
	}
}

/**
 * This method finds out, which composite tiles are needed, depending on a
 * list of available/required render tiles, and puts them into a set. So we can find out
 * which composite tiles are available and which composite tiles need to get rendered.
 */
void TileSet::findRequiredCompositeTiles(const std::set<TilePos>& render_tiles,
		std::set<Path>& tiles) {

	// iterate through the render tiles on the max zoom level
	// add their parent composite tiles
	for (std::set<TilePos>::iterator it = render_tiles.begin(); it != render_tiles.end(); ++it) {
		Path path = Path::byTilePos(*it, depth);
		tiles.insert(path.parent());
	}

	// now iterate through the composite tiles from bottom to top
	// and also add their parent composite tiles
	for (int d = depth - 1; d > 0; d--) {
		std::set<Path> tmp;
		for (std::set<Path>::iterator it = tiles.begin(); it != tiles.end(); ++it) {
			if (it->getDepth() == d)
				tmp.insert(it->parent());
		}
		for (std::set<Path>::iterator it = tmp.begin(); it != tmp.end(); ++it)
			tiles.insert(*it);
	}
}

bool TileSet::hasTile(const Path& path) const {
	if (path.getDepth() == depth)
		return render_tiles.count(path.getTilePos()) != 0;
	return composite_tiles.count(path) != 0;
}

bool TileSet::isTileRequired(const Path& path) const {
	if(path.getDepth() == depth)
		return required_render_tiles.count(path.getTilePos()) != 0;
	return required_composite_tiles.count(path) != 0;
}

const std::set<TilePos>& TileSet::getAvailableRenderTiles() const {
	return render_tiles;
}

const std::set<Path>& TileSet::getAvailableCompositeTiles() const {
	return composite_tiles;
}

const std::set<TilePos>& TileSet::getRequiredRenderTiles() const {
	return required_render_tiles;
}

const std::set<Path>& TileSet::getRequiredCompositeTiles() const {
	return required_composite_tiles;
}

int TileSet::getRequiredRenderTilesCount() const {
	return required_render_tiles.size();
}

int TileSet::getRequiredCompositeTilesCount() const {
	return required_composite_tiles.size();
}

/**
 * This function counts the render tiles, which are in composite tile. The childs map is a map
 * with tiles and a list of their childs.
 */
int countTiles(const Path& tile, std::map<Path, std::set<Path> >& childs, int level) {
	int count = 0;
	if (tile.getDepth() == level)
		return 1;
	for (std::set<Path>::const_iterator it = childs[tile].begin(); it != childs[tile].end();
			++it) {
		count += countTiles(*it, childs, level);
	}
	return count;
}

/**
 * A render task with costs (count of render tiles) and a start composite tile to begin
 * then recursive rendering.
 */
struct Task {
	Path tile;
	int costs;
};

/**
 * A worker with assigned render tasks.
 */
struct Worker {
	int work;
	std::vector<Task> tasks;
};

bool compareTasks(const Task& t1, const Task& t2) {
	return t1.costs < t2.costs;
}

bool compareWorkers(const Worker& w1, const Worker& w2) {
	return w1.work < w2.work;
}

int sumTasks(std::vector<Task>& vec) {
	int s = 0;
	for (size_t i = 0; i < vec.size(); i++)
		s += vec[i].costs;
	return s;
}

/**
 * This function assigns a list of tasks with specific costs to a list of workers. It uses
 * a simple greedy algorithm. It orders the tasks by their costs (ascending), iterates
 * through them and adds the task to the worker with the lowest work.
 *
 * The function returns the maximum difference from the average work as percentage.
 */
double assignTasks(std::vector<Task> tasks, std::vector<Worker>& workers) {
	// sort the tasks ascending
	std::sort(tasks.begin(), tasks.end(), compareTasks);

	// create a list of workers
	int worker_count = workers.size();
	for (int i = 0; i < worker_count; i++)
		workers[i].work = 0;

	// create a heap of the workers
	std::make_heap(workers.begin(), workers.end(), compareWorkers);

	// go through all tasks
	for (size_t i = 0; i < tasks.size(); i++) {
		// get the worker with the lowest work and add this ask
		workers.front().work += tasks[i].costs;
		workers.front().tasks.push_back(tasks[i]);

		// sort heap
		std::sort_heap(workers.begin(), workers.end(), compareWorkers);
	}

	// calculate the max difference from the average
	int max_diff = 0;
	int avg = sumTasks(tasks) / worker_count;
	for (int i = 0; i < worker_count; i++)
		max_diff = MAX(max_diff, std::abs(workers[i].work - avg));
	return (double) max_diff / sumTasks(tasks);
}

struct Assigment {

	int level;
	int remaining;
	double difference;
	std::vector<std::map<Path, int> > workers;

	bool operator<(const Assigment& other) const {
		return difference < other.difference;
	};
};

/**
 * This method tries to find an assignment of render tasks to a specific count of workers.
 * The workers should do the same amount of work.
 */
int TileSet::findRenderTasks(int worker_count,
		std::vector<std::map<Path, int> >& workers) const {
	//std::cout << "Render tiles: " << required_render_tiles.size() << std::endl;
	//std::cout << "Composite tiles: " << required_composite_tiles.size() << std::endl;

	// at first create two lists:
	// a list with tiles and their childs
	std::map<Path, std::set<Path> > tile_childs;
	// for every zoom level a list with tiles on it
	std::vector<std::set<Path> > tiles_by_zoom;
	tiles_by_zoom.resize(depth + 1);
	// go through all required composite tiles
	for (std::set<Path>::iterator it = required_composite_tiles.begin();
			it != required_composite_tiles.end(); ++it) {
		std::set<Path> childs;
		// check if we're at the level before the render tiles
		if (it->getDepth() == depth - 1) {
			// then check render tile childs
			for (int i = 1; i <= 4; i++) {
				TilePos pos = (*it + i).getTilePos();
				if (required_render_tiles.count(pos))
					childs.insert(*it + i);
			}
		} else {
			// else check composite tile childs
			for (int i = 1; i <= 4; i++) {
				if(required_composite_tiles.count(*it + i))
					childs.insert(*it + i);
			}
		}
		// now insert into lists
		tile_childs[*it] = childs;
		tiles_by_zoom[it->getDepth()].insert(*it);
	}

	// list of possible assignments
	std::vector<Assigment> assignments;

	// the count of composite tiles, the renderer needs to render at the end
	int composite_tiles = 1;
	// go through the composite tiles of all zoom levels
	// maximum zoom level 6, because on zoom level 5 are already 4**5=1025 tiles
	for (int zoom = 1; zoom <= 6 && zoom < depth; zoom++) {
		// a list of "tasks" - composite tiles to start rendering
		std::vector<Task> tasks;
		for (std::set<Path>::iterator it = tiles_by_zoom[zoom].begin();
				it != tiles_by_zoom[zoom].end(); ++it) {
			// create tasks
			Task task;
			task.tile = *it;
			// count render tiles for this task
			task.costs = countTiles(*it, tile_childs, depth);
			tasks.push_back(task);
		}

		// create a list of workers
		std::vector<Worker> workers_zoomlevel;
		workers_zoomlevel.resize(worker_count);
		// assign tasks, get maximum difference from the average
		double difference = assignTasks(tasks, workers_zoomlevel);

		// create assignment and put it in the list
		Assigment assignment;
		assignment.level = zoom;
		assignment.remaining = composite_tiles;
		assignment.difference = difference;
		assignment.workers.resize(worker_count);
		for (int i = 0; i < worker_count; i++) {
			Worker w = workers_zoomlevel[i];
			for (size_t j = 0; j < w.tasks.size(); j++)
				assignment.workers[i][w.tasks[j].tile] = countTiles(w.tasks[j].tile,
						tile_childs, depth);
		}
		assignments.push_back(assignment);

		// then go to the next zoom level
		// but before, add all composite tiles on this level to the composite tiles,
		// which are rendered at the end
		composite_tiles += tiles_by_zoom[zoom].size();
	}

	// sort the assignments by the differences ascending
	std::sort(assignments.begin(), assignments.end());

	// get the assignment with the lowest differences
	Assigment assignment = assignments.front();
	workers = assignment.workers;

	return assignment.remaining;
}

}
}
