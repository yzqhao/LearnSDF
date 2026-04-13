#pragma once
#include <vector>
#include "../math/Color.h"
#include "../math/Vec3.h"
#include "../math/Vec4.h"
#include "../math/AABB.h"
#include "../math/Mat4.h"
#include "../math/Ray.h"
//#include "Mesh/Primitive.h"
#include <memory>

class TTriangle
{
public:
    TTriangle() = default;

    TTriangle(const Math::Vec3& InPointA, const Math::Vec3& InPointB, const Math::Vec3& InPointC, const Math::Color& InColor)
        :PointA(InPointA), PointB(InPointB), PointC(InPointC), Color(InColor)
    {
    }

    TTriangle(const TTriangle& Other) = default;

	bool GetWorldBoundingBox(Math::AABB& OutBox) const
	{
        Math::AABB LocalBox = BoundingBox;
        OutBox = LocalBox.Transform(WorldTransform);
		return true;
	}

    void GenerateBoundingBox()
    {
        std::vector<Math::Vec3> Points;
        Points.push_back(PointA);
        Points.push_back(PointB);
        Points.push_back(PointC);

        for (auto& point : Points)
            BoundingBox.Merge(point);
    }

	bool Intersect(const Math::Ray& ray, float& Dist, bool& bBackFace);

public:
    Math::Vec3 PointA;
    Math::Vec3 PointB;
    Math::Vec3 PointC;
    Math::Color Color;
    Math::AABB BoundingBox;
	Math::Mat4 WorldTransform;
};




enum TKdNodeFlag
{
	SplitX,
	SplitY,
	SplitZ,
	Leaf
};

struct TkdTreeBulidNode
{
public:
	void InitLeaf(const std::vector<int>& Indices)
	{
		Flag = TKdNodeFlag::Leaf;
		PrimitiveIndices = Indices;
	}

	void InitInterior(TKdNodeFlag SplitAxis, const Math::AABB& InBounds, float InSplitPos, std::unique_ptr<TkdTreeBulidNode>& Below,
		std::unique_ptr<TkdTreeBulidNode>& Above)
	{
		Flag = SplitAxis;
		Bounds = InBounds;
		SplitPos = InSplitPos;
		BelowChild = std::move(Below);
		AboveChild = std::move(Above);
	}

	bool IsLeafNode() const
	{
		return Flag == TKdNodeFlag::Leaf;
	}

public:
	TKdNodeFlag Flag;

	// For interior node
	Math::AABB Bounds;

	float SplitPos;

	std::unique_ptr<TkdTreeBulidNode> BelowChild = nullptr;

	std::unique_ptr<TkdTreeBulidNode> AboveChild = nullptr;

	// For leaf node
	std::vector<int> PrimitiveIndices;
};

struct TkdTreeBoundEdge
{
public:
	enum class EdgeType 
	{ 
		Start, 
		End 
	};

	TkdTreeBoundEdge(EdgeType InType, float InPos, int InPrimitiveIndex)
		:Type(InType), Pos(InPos), PrimitiveIndex(InPrimitiveIndex)
	{}

public:
	EdgeType Type;

	float Pos;

	int PrimitiveIndex;
};

struct TKdTreeLinearNode
{
public:
	bool IsLeafNode() const
	{
		return Flag == TKdNodeFlag::Leaf;
	}

public:
	TKdNodeFlag Flag;

	// For interior node
	Math::AABB Bounds;

	float SplitPos;

	int AboveChildOffset = -1;

	// For leaf node
	std::vector<int> PrimitiveIndices;
};

struct TkdTreeNodeToVisit
{
	TkdTreeNodeToVisit(int Index, float Min, float Max)
		:NodeIndex(Index), tMin(Min), tMax(Max)
	{}

	int NodeIndex;

	float tMin, tMax;
};

class TWorld;

class TKdTreeAccelerator
{
public:
	TKdTreeAccelerator(std::vector<std::shared_ptr<TTriangle>> AllPrimtives, int InMaxDepth = -1);

	bool Intersect(const Math::Ray& ray, float& Dist, bool& bBackFace) const;

	// Just for debug
	bool IntersectBruteForce(const Math::Ray& ray, float& Dist, bool& bBackFace) const;

private:
    std::unique_ptr<TkdTreeBulidNode> RecursiveBuild(const Math::AABB& NodeBounds, const std::vector<int>& PrimitiveIndices,
        int Depth, int& OutTotalNodes);

	bool FindBestSplit(const Math::AABB& NodeBounds, const std::vector<int>& PrimitiveIndices, int& SplitAxis, int& SplitOffset, 
		std::vector<TkdTreeBoundEdge>& SplitEdges);

	int FlattenKdTree(std::unique_ptr<TkdTreeBulidNode>& Node, int& Offset);

	Math::Color MapDepthToColor(int Depth) const;

private:
	std::vector<std::shared_ptr<TTriangle>> Primitives;

	std::vector<Math::AABB> PrimitiveBounds;

	std::unique_ptr<TkdTreeBulidNode> RootNode = nullptr;

	std::vector<TKdTreeLinearNode> LinearNodes;

	const int MaxPrimsInNode = 1;

	int MaxDepth = -1;

	const float EmptyBonus = 0.5f;

	const int TraversalCost = 1;

	const int IsectCost = 80;
};
