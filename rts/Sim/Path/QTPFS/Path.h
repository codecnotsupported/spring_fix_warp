/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef QTPFS_PATH_H_
#define QTPFS_PATH_H_

#include <algorithm>
#include <vector>

#include "Sim/MoveTypes/MoveDefHandler.h"

#include "System/float3.h"

#include "Map/ReadMap.h"

#include "System/TimeProfiler.h"

class CSolidObject;

namespace QTPFS {
	struct IPath {
		struct PathNodeData {
			uint32_t nodeId;
			float2 netPoint;
			int pathPointIndex = -1;
			int xmin = 0;
			int zmin = 0;
			int xmax = 0;
			int zmax = 0;
		};

		IPath() {}
		IPath(const IPath& other) { *this = other; }
		IPath& operator = (const IPath& other) {
			if (this == &other) return *this;

			pathID = other.pathID;
			pathType = other.pathType;

			nextPointIndex = other.nextPointIndex;
			numPathUpdates = other.numPathUpdates;

			hash   = other.hash;
			virtualHash = other.virtualHash;
			radius = other.radius;
			synced = other.synced;
			haveFullPath = other.haveFullPath;
			havePartialPath = other.havePartialPath;
			boundingBoxOverride = other.boundingBoxOverride;
			points = other.points;
			nodes = other.nodes;

			boundingBoxMins = other.boundingBoxMins;
			boundingBoxMaxs = other.boundingBoxMaxs;

			owner = other.owner;
			searchTime = other.searchTime;
			return *this;
		}
		IPath(IPath&& other) { *this = std::move(other); }
		IPath& operator = (IPath&& other) {
			if (this == &other) return *this;

			pathID = other.pathID;
			pathType = other.pathType;

			nextPointIndex = other.nextPointIndex;
			numPathUpdates = other.numPathUpdates;

			hash   = other.hash;
			virtualHash = other.virtualHash;
			radius = other.radius;
			synced = other.synced;
			haveFullPath = other.haveFullPath;
			havePartialPath = other.havePartialPath;
			boundingBoxOverride = other.boundingBoxOverride;
			points = std::move(other.points);
			nodes = std::move(other.nodes);

			boundingBoxMins = other.boundingBoxMins;
			boundingBoxMaxs = other.boundingBoxMaxs;

			owner = other.owner;
			searchTime = other.searchTime;

			return *this;
		}
		~IPath() { points.clear(); nodes.clear(); }

		void SetID(unsigned int pathID) { this->pathID = pathID; }
		unsigned int GetID() const { return pathID; }

		void SetNextPointIndex(unsigned int nextPointIndex) { this->nextPointIndex = nextPointIndex; }
		void SetNumPathUpdates(unsigned int numPathUpdates) { this->numPathUpdates = numPathUpdates; }
		unsigned int GetNextPointIndex() const { return nextPointIndex; }
		unsigned int GetNumPathUpdates() const { return numPathUpdates; }

		void SetHash(std::uint64_t hash) { this->hash = hash; }
		void SetVirtualHash(std::uint64_t virtualHash) { this->virtualHash = virtualHash; }
		void SetRadius(float radius) { this->radius = radius; }
		void SetSynced(bool synced) { this->synced = synced; }
		void SetHasFullPath(bool fullPath) { this->haveFullPath = fullPath; }
		void SetHasPartialPath(bool partialPath) { this->havePartialPath = partialPath; }

		float GetRadius() const { return radius; }
		std::uint64_t GetHash() const { return hash; }
		std::uint64_t GetVirtualHash() const { return virtualHash; }
		bool IsSynced() const { return synced; }
		bool IsFullPath() const { return haveFullPath; }
		bool IsPartialPath() const { return havePartialPath; }

		void SetBoundingBox() {
			boundingBoxMins.x = 1e6f; boundingBoxMaxs.x = -1e6f;
			boundingBoxMins.z = 1e6f; boundingBoxMaxs.z = -1e6f;

			for (unsigned int n = 0; n < points.size(); n++) {
				boundingBoxMins.x = std::min(boundingBoxMins.x, points[n].x);
				boundingBoxMins.z = std::min(boundingBoxMins.z, points[n].z);
				boundingBoxMaxs.x = std::max(boundingBoxMaxs.x, points[n].x);
				boundingBoxMaxs.z = std::max(boundingBoxMaxs.z, points[n].z);
			}

			boundingBoxOverride = false;

			checkPointInBounds(boundingBoxMins);
			checkPointInBounds(boundingBoxMaxs);
		}

		void SetBoundingBox(float3 mins, float3 maxs) {
			boundingBoxMins = mins;
			boundingBoxMaxs = maxs;

			boundingBoxOverride = true;

			checkPointInBounds(boundingBoxMins);
			checkPointInBounds(boundingBoxMaxs);
		}

		bool IsBoundingBoxOverriden() const { return boundingBoxOverride; }

		// // This version is only safe if decltype(points)::value_type == float4
		// void SetBoundingBoxSse() {
		// 	float minResults[4] = { -1e6f, -1e6f, -1e6f, -1e6f };
		// 	float maxResults[4] = { 1e6f, 1e6f, 1e6f, 1e6f };
		// 	const int endIdx = points.size();

		// 	// Main loop for finding maximum height values
		// 	__m128 bestMin = _mm_loadu_ps(minResults);
		// 	__m128 bestMax = _mm_loadu_ps(maxResults);
		// 	for (int i = 0; i < endIdx; ++i) {
		// 		__m128 next = _mm_loadu_ps((float*)&points[i]);
		// 		bestMin = _mm_min_ps(bestMin, next);
		// 		bestMax = _mm_max_ps(bestMax, next);
		// 	}
		// 	_mm_storeu_ps(minResults, bestMin);
		// 	_mm_storeu_ps(maxResults, bestMax);

		// 	boundingBoxMins.x = minResults[0]; boundingBoxMaxs.x = maxResults[0];
		// 	boundingBoxMins.y = minResults[2]; boundingBoxMaxs.y = maxResults[2];

		// 	checkPointInBounds(boundingBoxMins);
		// 	checkPointInBounds(boundingBoxMaxs);
		// }

		const float3& GetBoundingBoxMins() const { return boundingBoxMins; }
		const float3& GetBoundingBoxMaxs() const { return boundingBoxMaxs; }

		void SetPoint(unsigned int i, const float3& p) {
			checkPointInBounds(p);
			points[std::min(i, NumPoints() - 1)] = p;
		}
		const float3& GetPoint(unsigned int i) const { return points[std::min(i, NumPoints() - 1)]; }

		void RemovePoint(unsigned int index) {
			unsigned int start = std::min(index, NumPoints() - 1), end = NumPoints() - 1;
			for (unsigned int i = start; i < end; ++i) { points[i] = points[i+1]; }
			points.pop_back();
		}

		void SetNode(unsigned int i, uint32_t nodeId, float2&& netpoint, int pointIdx) {
			nodes[i].netPoint = netpoint;
			nodes[i].nodeId = nodeId;
			nodes[i].pathPointIndex = pointIdx;
		}
		void SetNodeBoundary(unsigned int i, int xmin, int zmin, int xmax, int zmax) {
			nodes[i].xmin = xmin;
			nodes[i].zmin = zmin;
			nodes[i].xmax = xmax;
			nodes[i].zmax = zmax;
		}
		const PathNodeData& GetNode(unsigned int i) const { return nodes[i]; };

		void SetSourcePoint(const float3& p) { /* checkPointInBounds(p); */ assert(points.size() >= 2); points[                0] = p; }
		void SetTargetPoint(const float3& p) { /* checkPointInBounds(p); */ assert(points.size() >= 2); points[points.size() - 1] = p; }
		const float3& GetSourcePoint() const { return points[                0]; }
		const float3& GetTargetPoint() const { return points[points.size() - 1]; }

		void checkPointInBounds(const float3& p) {
			assert(p.x >= 0.f);
			assert(p.z >= 0.f);
			assert(p.x / SQUARE_SIZE <= mapDims.mapx);
			assert(p.z / SQUARE_SIZE <= mapDims.mapy);
		}

		void SetOwner(const CSolidObject* o) { owner = o; }
		const CSolidObject* GetOwner() const { return owner; }

		unsigned int NumPoints() const { return (points.size()); }
		void AllocPoints(unsigned int n) {
			points.clear();
			points.resize(n);
		}
		void CopyPoints(const IPath& p) {
			AllocPoints(p.NumPoints());

			for (unsigned int n = 0; n < p.NumPoints(); n++) {
				points[n] = p.GetPoint(n);
			}
		}
		void AllocNodes(unsigned int n) {
			nodes.clear();
			nodes.resize(n);
		}
		void CopyNodes(const IPath& p) {
			AllocNodes(p.nodes.size());

			for (unsigned int n = 0; n < p.nodes.size(); n++) {
				nodes[n] = p.GetNode(n);
			}
		}

		void SetPathType(int newPathType) { assert(pathType < moveDefHandler.GetNumMoveDefs()); pathType = newPathType; }
		int GetPathType() const { return pathType; }

		std::vector<PathNodeData>& GetNodeList() { return nodes; };

		void SetSearchTime(spring_time time) { searchTime = time; }

		spring_time GetSearchTime() const { return searchTime; }

	private:
		unsigned int pathID = 0;
		int pathType = 0;

		unsigned int nextPointIndex = 0; // index of the next waypoint to be visited
		unsigned int numPathUpdates = 0; // number of times this path was invalidated

		// Identifies the layer, target quad and source quad for a search query so that similar
		// searches can be combined.
		std::uint64_t hash = -1;

		// Similar to hash, but the target quad and source quad numbers may not relate to actual
		// leaf nodes in the quad tree. They repesent the quad that would be there if the leaf node
		// was exactly the size of QTPFS_PARTIAL_SHARE_PATH_MAX_SIZE. This allows searches that
		// start and/or end in different, but close, quads. This is used to handle partially-
		// shared path searches.
		std::uint64_t virtualHash = -1;
		float radius = 0.f;
		bool synced = true;
		bool haveFullPath = true;
		bool havePartialPath = false;
		bool boundingBoxOverride = false;

		std::vector<float3> points;
		std::vector<PathNodeData> nodes;

		// corners of the bounding-box containing all our points
		float3 boundingBoxMins;
		float3 boundingBoxMaxs;

		// object that requested this path (NULL if none)
		const CSolidObject* owner = nullptr;

		spring_time searchTime;
	};
}

#endif
