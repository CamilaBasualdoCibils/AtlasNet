#pragma once
#include "AABB.hpp"

template <uint8_t Dim, typename Type>
struct AABBTreeNode
{
	using AABB_t = AABB<Dim, Type>;
	using Index = int32_t;

	AABB_t bounds;

	// Leaf data
	Index object = -1;	// index into external object array

	// Children
	Index left = -1;
	Index right = -1;

	bool isLeaf() const { return left < 0 && right < 0; }
};
template <uint8_t Dim, typename Type>
class AABBTree
{
   public:
	using AABB_t = AABB<Dim, Type>;
	using Node = AABBTreeNode<Dim, Type>;
	using Index = typename Node::Index;

	struct Item
	{
		AABB_t bounds;
		Index object;
	};

	// ----------------------------------
	// Build API
	// ----------------------------------

	void build(const std::vector<Item>& items)
	{
		nodes.clear();
		nodes.reserve(items.size() * 2);

		std::vector<Item> local = items;
		root = buildRecursive(local, 0, (Index)local.size());
	}

	// ----------------------------------
	// Queries
	// ----------------------------------

	template <typename Fn>
	void query(const AABB_t& box, Fn&& fn) const
	{
		if (root < 0)
			return;
		queryRecursive(root, box, fn);
	}

	template <typename Fn>
	void rayQuery(const typename AABB_t::vectype& origin, const typename AABB_t::vectype& dir,
				  Fn&& fn) const
	{
		if (root < 0)
			return;

		Type t0, t1;
		rayRecursive(root, origin, dir, t0, t1, fn);
	}
	template <typename Fn>
	bool queryPoint(const typename AABB_t::vectype& point, Fn&& fn) const
	{
		if (root < 0)
			return false;

		bool hit = false;
		queryPointRecursive(root, point, fn, hit);
		return hit;
	}

   private:
	std::vector<Node> nodes;
	Index root = -1;

	// ----------------------------------
	// Recursive build
	// ----------------------------------

	Index buildRecursive(std::vector<Item>& items, Index begin, Index end)
	{
		Node node;

		// Compute bounds
		for (Index i = begin; i < end; ++i) node.bounds.expand(items[i].bounds);

		Index count = end - begin;

		if (count == 1)
		{
			node.object = items[begin].object;
			Index id = (Index)nodes.size();
			nodes.push_back(node);
			return id;
		}

		// Choose split axis (largest extent)
		typename AABB_t::vectype size = node.bounds.size();
		uint8_t axis = 0;
		for (uint8_t i = 1; i < Dim; ++i)
			if (size[i] > size[axis])
				axis = i;

		// Partition by center along axis
		Index mid = begin + count / 2;
		std::nth_element(items.begin() + begin, items.begin() + mid, items.begin() + end,
						 [axis](const Item& a, const Item& b)
						 { return a.bounds.center()[axis] < b.bounds.center()[axis]; });

		node.left = buildRecursive(items, begin, mid);
		node.right = buildRecursive(items, mid, end);

		Index id = (Index)nodes.size();
		nodes.push_back(node);
		return id;
	}

	// ----------------------------------
	// Queries
	// ----------------------------------

	template <typename Fn>
	void queryRecursive(Index nodeId, const AABB_t& box, Fn& fn) const
	{
		const Node& n = nodes[nodeId];

		if (!n.bounds.intersects(box))
			return;

		if (n.isLeaf())
		{
			fn(n.object);
			return;
		}

		queryRecursive(n.left, box, fn);
		queryRecursive(n.right, box, fn);
	}
	template <typename Fn>
	void queryPointRecursive(Index nodeId, const typename AABB_t::vectype& point, Fn& fn,
							 bool& hit) const
	{
		const Node& n = nodes[nodeId];

		// Early out: point not inside node bounds
		if (!n.bounds.contains(point))
			return;

		if (n.isLeaf())
		{
			fn(n.object);
			hit = true;
			return;
		}

		queryPointRecursive(n.left, point, fn, hit);
		queryPointRecursive(n.right, point, fn, hit);
	}

	template <typename Fn>
	void rayRecursive(Index nodeId, const typename AABB_t::vectype& origin,
					  const typename AABB_t::vectype& dir, Type& tMin, Type& tMax, Fn& fn) const
	{
		const Node& n = nodes[nodeId];

		if (!n.bounds.intersectsRay(origin, dir, tMin, tMax))
			return;

		if (n.isLeaf())
		{
			fn(n.object, tMin);
			return;
		}

		rayRecursive(n.left, origin, dir, tMin, tMax, fn);
		rayRecursive(n.right, origin, dir, tMin, tMax, fn);
	}
};
