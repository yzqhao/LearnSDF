#include "KdTree.h"
#include <algorithm>
#include <stack>
#include <cmath>
#include "../math/MathUtil.h"

bool TTriangle::Intersect(const Math::Ray& Ray, float& Dist, bool& bBackFace)
{
    const float EPSILON = 0.000001f;

    Math::Vec3 Dir = Ray.direction;
    Math::Vec3 Orig = Ray.origin;

    // Find vectors for two edges sharing PointA
    Math::Vec3 Edge1 = PointB - PointA;
    Math::Vec3 Edge2 = PointC - PointA;

    // Begin calculating determinant, also used to calculate U parameter
    Math::Vec3 PVec = Dir.Cross(Edge2);

    // If determinant is near zero, ray lies in plane of f triangle
    float Det = Edge1.Dot(PVec);

    if (Det > -EPSILON && Det < EPSILON)
    {
        return false;
    }

    float InvDet = 1.0f / Det;

    // Calculate distance from vert to ray origin
    Math::Vec3 TVec = Orig - PointA;

    // Calculate U parameter and test bounds 
    float U = TVec.Dot(PVec) * InvDet;
    if (U < 0.0f || U > 1.0f)
    {
        return false;
    }

    // Prepare to test V parameter
    Math::Vec3 QVec = TVec.Cross(Edge1);

    // Calculate V parameter and test bounds
    float V = Dir.Dot(QVec) * InvDet;
    if (V < 0.0f || U + V > 1.0f)
    {
        return false;
    }

    // Calculate T
    float T = Edge2.Dot(QVec) * InvDet;

    if (T < 0.0f)
    {
        return false;
    }

    float TValue = std::abs(T);
    if (TValue > Ray.MaxDist)
    {
        return false;
    }

    Dist = TValue;
    bBackFace = Det < 0.0f ? true : false;

    return true;
}



TKdTreeAccelerator::TKdTreeAccelerator(std::vector<std::shared_ptr<TTriangle>> AllPrimtives, int InMaxDepth)
{
	// Initial Primitive and bounds
	Math::AABB RootBounds;
	for (size_t i = 0; i < AllPrimtives.size(); i++)
	{
		Math::AABB WorldBound;
		if (AllPrimtives[i]->GetWorldBoundingBox(WorldBound))
		{
			Primitives.push_back(AllPrimtives[i]);
			PrimitiveBounds.push_back(WorldBound);

			RootBounds.Merge(WorldBound);
		}
	}

	MaxDepth = InMaxDepth;
	if (MaxDepth <= 0)
	{
		MaxDepth = (int)std::round(8 + 1.3f * Math::Log2Int(int64_t(Primitives.size())));
	}

	// Init PrimitiveIndices for root node
	std::vector<int> RootPrimitiveIndices;
	RootPrimitiveIndices.reserve(Primitives.size());
	for (int i = 0; i < Primitives.size(); i++)
	{
		RootPrimitiveIndices.push_back(i);
	}

	// Recursive bulid kd-tree
	int TotalNodes = 0;
	RootNode = RecursiveBuild(RootBounds, RootPrimitiveIndices, 0, TotalNodes);

	// Compute representation of depth-first traversal of kd tree
	LinearNodes.resize(TotalNodes);
	int Offset = 0;
	FlattenKdTree(RootNode, Offset);
}


std::unique_ptr<TkdTreeBulidNode> TKdTreeAccelerator::RecursiveBuild(const Math::AABB& NodeBounds, const std::vector<int>& PrimitiveIndices, 
	int Depth, int& OutTotalNodes)
{
	std::unique_ptr<TkdTreeBulidNode> Node = std::make_unique<TkdTreeBulidNode>();

	OutTotalNodes++;

	int PrimitiveCount = int(PrimitiveIndices.size());
	if (PrimitiveCount <= MaxPrimsInNode || Depth == MaxDepth) // Create leaf node
	{
		Node->InitLeaf(PrimitiveIndices);

		return Node;
	}
	else
	{
		// Find the best splitAxis and split position
		int SplitAxis = -1;
		int SplitOffset = -1;
		std::vector<TkdTreeBoundEdge> SplitEdges;
		bool bSuccess = FindBestSplit(NodeBounds, PrimitiveIndices, SplitAxis, SplitOffset, SplitEdges);
		
		if(!bSuccess) //Create leaf node
		{
			Node->InitLeaf(PrimitiveIndices);

			return Node;
		}

		// Classify primitives with respect to split
		std::vector<int> BelowPrimitiveIndices;	
		std::vector<int> AbovePrimitiveIndices;
		for (int i = 0; i < SplitOffset; i++)
		{
			if (SplitEdges[i].Type == TkdTreeBoundEdge::EdgeType::Start)
			{
				BelowPrimitiveIndices.push_back(SplitEdges[i].PrimitiveIndex);
			}
		}
		for (int i = SplitOffset + 1; i < SplitEdges.size(); i++)
		{
			if (SplitEdges[i].Type == TkdTreeBoundEdge::EdgeType::End)
			{
				AbovePrimitiveIndices.push_back(SplitEdges[i].PrimitiveIndex);
			}
		}

		// Recursively initialize children nodes
		float SplitPos = SplitEdges[SplitOffset].Pos;
		Math::AABB BelowBoundingBox = NodeBounds;
		BelowBoundingBox._max[SplitAxis] = SplitPos;
		Math::AABB AboveBoundingBox = NodeBounds;
		AboveBoundingBox._min[SplitAxis] = SplitPos;

		Node->InitInterior(TKdNodeFlag(SplitAxis), NodeBounds, SplitPos,
			RecursiveBuild(BelowBoundingBox, BelowPrimitiveIndices, Depth + 1, OutTotalNodes),
			RecursiveBuild(AboveBoundingBox, AbovePrimitiveIndices, Depth + 1, OutTotalNodes));
	}

	return Node;
}

bool TKdTreeAccelerator::FindBestSplit(const Math::AABB& NodeBounds, const std::vector<int>& PrimitiveIndices, int& SplitAxis, 
	int& SplitOffset, std::vector<TkdTreeBoundEdge>& SplitEdges)
{
	SplitAxis = -1;
	SplitOffset = -1;

	int PrimitiveCount = int(PrimitiveIndices.size());

	Math::Vec3 TotalSize = NodeBounds.GetSize();
	float TotalSA = NodeBounds.GetSurfaceArea();
	float InvTotalSA = 1.0f / TotalSA;
	float BestCost = FLT_MAX;

	for (int Axis = 0; Axis < 3; Axis++)
	{
		int OldSplitAixs = SplitAxis;

		// Initialize edges for axis
		std::vector<TkdTreeBoundEdge> Edges;
		for (int Index : PrimitiveIndices)
		{
			const Math::AABB& Bound = PrimitiveBounds[Index];

			// Add start and end edge for this primitive
			Edges.push_back(TkdTreeBoundEdge(TkdTreeBoundEdge::EdgeType::Start, Bound._min[Axis], Index));
			Edges.push_back(TkdTreeBoundEdge(TkdTreeBoundEdge::EdgeType::End, Bound._max[Axis], Index));
		}

		// Sort edges for axis
		std::sort(Edges.begin(), Edges.end(),
			[](const TkdTreeBoundEdge& Edge0, const TkdTreeBoundEdge& Edge1) -> bool
			{
				if (Edge0.Pos == Edge1.Pos)
					return (int)Edge0.Type < (int)Edge1.Type;
				else
					return Edge0.Pos < Edge1.Pos;
			});

		// Compute cost of all splits for axis to find best
		int BelowCount = 0, AboveCount = PrimitiveCount;
		for (int EdgeIdx = 0; EdgeIdx < Edges.size(); EdgeIdx++)
		{
			TkdTreeBoundEdge& Edge = Edges[EdgeIdx];

			if (Edge.Type == TkdTreeBoundEdge::EdgeType::End)
			{
				AboveCount--;
			}

			float EdgePos = Edge.Pos;
			if (EdgePos > NodeBounds._min[Axis] && EdgePos < NodeBounds._max[Axis])
			{
				int OtherAxis0 = (Axis + 1) % 3;
				int OtherAxis1 = (Axis + 2) % 3;

				float BelowSA = 2 * (TotalSize[OtherAxis0] * TotalSize[OtherAxis1] +
					(EdgePos - NodeBounds._min[Axis]) *
					(TotalSize[OtherAxis0] + TotalSize[OtherAxis1]));
				float AboveSA = 2 * (TotalSize[OtherAxis0] * TotalSize[OtherAxis1] +
					(NodeBounds._max[Axis] - EdgePos) *
					(TotalSize[OtherAxis0] + TotalSize[OtherAxis1]));

				float pBelow = BelowSA * InvTotalSA;
				float pAbove = AboveSA * InvTotalSA;
				float Bonus = (AboveCount == 0 || BelowCount == 0) ? EmptyBonus : 0;
				float Cost = TraversalCost + IsectCost * (1 - Bonus) * (pBelow * BelowCount + pAbove * AboveCount);

				// Update best split if this is lowest cost so far
				if (Cost < BestCost)
				{
					BestCost = Cost;
					SplitAxis = Axis;
					SplitOffset = EdgeIdx;
				}
			}

			if (Edge.Type == TkdTreeBoundEdge::EdgeType::Start)
			{
				BelowCount++;
			}
		}
		assert(BelowCount == PrimitiveCount && AboveCount == 0);

		if (SplitAxis != OldSplitAixs)
		{
			SplitEdges = Edges;
		}
	}

	//TODO: BAD REFINE

	if ((BestCost > 4 * IsectCost * PrimitiveCount && PrimitiveCount < 16) || SplitAxis == -1) // Create leaf node
	{
		return false;
	}

	return true;
}

int TKdTreeAccelerator::FlattenKdTree(std::unique_ptr<TkdTreeBulidNode>& Node, int& Offset)
{
	TKdTreeLinearNode& LinearNode = LinearNodes[Offset];
	int MyOffset = Offset;
	Offset++;

	LinearNode.Flag = Node->Flag;
	if (Node->IsLeafNode()) // Left node
	{
		assert(Node->BelowChild == nullptr);
		assert(Node->AboveChild == nullptr);

		LinearNode.PrimitiveIndices = Node->PrimitiveIndices;
	}
	else // Interior node
	{
		LinearNode.Bounds = Node->Bounds;

		LinearNode.SplitPos = Node->SplitPos;

		FlattenKdTree(Node->BelowChild, Offset);

		LinearNode.AboveChildOffset = FlattenKdTree(Node->AboveChild, Offset);
	}

	return MyOffset;
}

Math::Color TKdTreeAccelerator::MapDepthToColor(int Depth) const
{
	Math::Color Color;
	switch (Depth)
	{
	case 0:
		Color = Math::Color::RED;
		break;
	case 1:
		Color = Math::Color::YELLOW;
		break;
	case 2:
		Color = Math::Color::GREEN;
		break;
	case 3:
		Color = Math::Color::CYAN;
		break;
	case 4:
		Color = Math::Color::BLUE;
		break;
	default:
		Color = Math::Color::MAGENTA;
		break;
	}

	return Color;
}

bool TKdTreeAccelerator::Intersect(const Math::Ray& ray, float& Dist, bool& bBackFace) const
{
	if (LinearNodes.empty())
	{
		return false;
	}

	// Compute initial parametric range of ray inside kd-tree extent
	float tMin, tMax;
	Math::AABB RootBounds = LinearNodes[0].Bounds;
	if (!Math::MathUtil::intersects(ray, RootBounds, tMin, tMax)) 
	{
		return false;
	}

	// Traverse kd-tree nodes in order for ray
	bool Hit = false;
	Math::Vec3 InvDir(1.0f / ray.direction.x, 1.0f / ray.direction.y, 1.0f / ray.direction.z);

	int CurrentNodeIdx = 0;
	std::stack<TkdTreeNodeToVisit> NodesToVisit;

	while (true)
	{
		if (ray.MaxDist < tMin)
		{
			break;
		}

		const TKdTreeLinearNode& Node = LinearNodes[CurrentNodeIdx];

		if (Node.IsLeafNode()) // Leaf node
		{
			for (int Index : Node.PrimitiveIndices)
			{
				auto Primitive = Primitives[Index];
				if (Primitive->Intersect(ray, Dist, bBackFace))
				{
					Hit = true;
					ray.MaxDist = Dist;
				}
			}

			if (NodesToVisit.empty())
			{
				break;
			}
			else
			{
				TkdTreeNodeToVisit NodeToVisit = NodesToVisit.top();
				NodesToVisit.pop();

				CurrentNodeIdx = NodeToVisit.NodeIndex;
				tMin = NodeToVisit.tMin;
				tMax = NodeToVisit.tMax;
			}
		}
		else // Interior node
		{
			// Compute parametric distance along ray to split plane
			int Axis = int(Node.Flag);
			float tPlane = (Node.SplitPos - ray.origin[Axis]) * InvDir[Axis];

			// Get node children pointers for ray
			int FirstChildIdx = -1;
			int SecondChildIdx = -1;
			int belowFirst =
				(ray.origin[Axis] < Node.SplitPos) ||
				(ray.origin[Axis] == Node.SplitPos && ray.direction[Axis] <= 0); //???
			if (belowFirst) 
			{
				FirstChildIdx = CurrentNodeIdx + 1;
				SecondChildIdx = Node.AboveChildOffset;
			}
			else 
			{
				FirstChildIdx = Node.AboveChildOffset;
				SecondChildIdx = CurrentNodeIdx + 1;
			}

			// Advance to next child node, possibly enqueue other child
			if (tPlane > tMax || tPlane <= 0)
			{
				CurrentNodeIdx = FirstChildIdx;
			}
			else if (tPlane < tMin)
			{
				CurrentNodeIdx = SecondChildIdx;
			}
			else 
			{	
				CurrentNodeIdx = FirstChildIdx;

				// Enqueue secondChild in todo list
				NodesToVisit.emplace(SecondChildIdx, tPlane, tMax);

				tMax = tPlane;
			}
		}
	}
	
	return Hit;
}
