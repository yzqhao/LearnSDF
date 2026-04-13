#pragma once

#include "../common/d3dUtil.h"

struct MeshSDFDescriptor
{
    Math::Vec3 Center;
    float Extent;

    int Resolution;
    int pad1;
    int pad2;
    int pad3;
};

struct Vertex
{
    Vertex() {}
    Vertex(
        const Math::Vec3& p,
        const Math::Vec3& n,
        const Math::Vec3& t,
        const Math::Vec2& uv) :
        Position(p),
        Normal(n),
        TangentU(t),
        TexC(uv) {
    }
    Vertex(
        float px, float py, float pz,
        float nx, float ny, float nz,
        float tx, float ty, float tz,
        float u, float v) :
        Position(px, py, pz),
        Normal(nx, ny, nz),
        TangentU(tx, ty, tz),
        TexC(u, v) {
    }

    Math::Vec3 Position;
    Math::Vec3 Normal;
    Math::Vec3 TangentU;
    Math::Vec2 TexC;
};


class Mesh
{
public:
    using uint16 = std::uint16_t;
    using uint32 = std::uint32_t;

    Mesh();

    Mesh(Mesh&&) = default;

    Mesh(const Mesh&) = delete;

    Mesh& operator=(const Mesh&) = delete;

public:

    /// Creates a box centered at the origin with the given dimensions, where each
    /// face has m rows and n columns of vertices.
    void CreateBox(float width, float height, float depth, uint32 numSubdivisions);

    /// Creates a sphere centered at the origin with the given radius.  The
    /// slices and stacks parameters control the degree of tessellation.
    void CreateSphere(float radius, uint32 sliceCount, uint32 stackCount);


    /// Creates a cylinder parallel to the y-axis, and centered about the origin.  
    /// The bottom and top radius can vary to form various cone shapes rather than true
    // cylinders.  The slices and stacks parameters control the degree of tessellation.
    void CreateCylinder(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount);


    /// Creates an mxn grid in the xz-plane with m rows and n columns, centered
    /// at the origin with the specified width and depth.
    void CreateGrid(float width, float depth, uint32 m, uint32 n);

    /// Creates a quad aligned with the screen.  This is useful for postprocessing and screen effects.
    void CreateQuad(float x, float y, float w, float h, float depth);

    const std::vector<uint16>& GetIndices16() const;

    std::string GetInputLayoutName() const;

    void GenerateIndices16();

    void GenerateBoundingBox();

private:

    void Subdivide();

    Vertex MidPoint(const Vertex& v0, const Vertex& v1);

    void BuildCylinderTopCap(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount);

    void BuildCylinderBottomCap(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount);

public:
    std::string MeshName;

    std::vector<Vertex> Vertices;

    std::vector<uint32> Indices32;

    std::vector<uint16> Indices16;

    std::string InputLayoutName;

    MeshSDFDescriptor SDFDescriptor;

    Math::AABB BoundingBox;
};
