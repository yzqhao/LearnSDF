
#include <array>
#include "../common/d3dApp.h"
#include "../math/Color.h"
#include "../math/Vec3.h"
#include "../math/Vec4.h"
#include "../common/d3dUtil.h"
#include "../common/UploadBuffer.h"
#include "../common/Camera.h"

#include "Mesh.h"
#include "D3DImage.h"

struct PassConstants
{
    Math::Mat4 View = Math::Mat4::IDENTITY;
    Math::Mat4 InvView = Math::Mat4::IDENTITY;
    Math::Mat4 Proj = Math::Mat4::IDENTITY;
    Math::Mat4 InvProj = Math::Mat4::IDENTITY;
    Math::Mat4 ViewProj = Math::Mat4::IDENTITY;
    Math::Mat4 InvViewProj = Math::Mat4::IDENTITY;
    Math::Mat4 PrevViewProj = Math::Mat4::IDENTITY;
    Math::Vec3 EyePosW = { 0.0f, 0.0f, 0.0f };
    float cbPassPad1 = 0.0f;
    Math::Vec2 RenderTargetSize = { 0.0f, 0.0f };
    Math::Vec2 InvRenderTargetSize = { 0.0f, 0.0f };
    float NearZ = 0.0f;
    float FarZ = 0.0f;
    float TotalTime = 0.0f;
    float DeltaTime = 0.0f;

    Math::Vec4 FogColor = { 0.7f, 0.7f, 0.7f, 1.0f };
    float gFogStart = 5.0f;
    float gFogRange = 150.0f;
    Math::Vec2 cbPassPad2;
};

struct ObjectConstants
{
    Math::Mat4 World = Math::Mat4::IDENTITY;
    Math::Mat4 PrevWorld = Math::Mat4::IDENTITY;
    Math::Mat4 TexTransform = Math::Mat4::IDENTITY;
    UINT     MaterialIndex;
    UINT     ObjPad0;
    UINT     ObjPad1;
    UINT     ObjPad2;
};

struct SSAOPassConstants
{
    Math::Mat4 ProjTex;

    // Coordinates given in view space.
    float    OcclusionRadius;
    float    OcclusionFadeStart;
    float    OcclusionFadeEnd;
    float    SurfaceEpsilon;
};

struct BlurSettingsConstants
{
    int gBlurRadius;

    // Support up to 11 blur weights.
    float w0;
    float w1;
    float w2;

    float w3;
    float w4;
    float w5;
    float w6;

    float w7;
    float w8;
    float w9;
    float w10;
};

struct ObjectSDFConstants
{
    Math::Mat4 ObjWorld;
    Math::Mat4 ObjInvWorld;

    int SDFIndex;
    int pad1;
    int pad2;
    int pad3;
};

struct LightShaderParameters
{
    Math::Vec3 Color;       // All light
    float Intensity;   // All light
    Math::Mat4 ShadowTransform;
    Math::Vec3 Direction;   // Directional/Spot light only
    int pad1;
};

class LearnSDFApp : public D3DApp
{
public:
	LearnSDFApp(HINSTANCE hInstance);
	~LearnSDFApp();

	virtual bool Initialize()override;

private:
	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;

    void OnKeyboardInput(const GameTimer& gt);
    void UpdateObjectCBs(const GameTimer& gt);
    void UpdateMainPassCB(const GameTimer& gt);

	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

	void BuildShadersAndInputLayout();
	void BuildPSO();
    void BuildMaterials();
    void LoadTextures();
    void BuildRenderItems();

	void BuildDescriptorHeaps();
	void BuildBuffers();
	void BuildRootSignature();

private:
	void InitMesh();

    struct RenderItem
    {
        RenderItem() = default;

        Math::Mat4 World{};
        Math::Mat4 TexTransform{};
        uint ObjCBIndex = -1;

        Material* Mat = nullptr;
        Mesh* Geo = nullptr;
        std::unique_ptr<MeshGeometry> MeshGeo;

        // Primitive topology.
        D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

        // DrawIndexedInstanced parameters.
        uint IndexCount = 0;
        uint StartIndexLocation = 0;
        int BaseVertexLocation = 0;
    };
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList);

    ID3D12DescriptorHeap* mCbvHeap = nullptr;
    ID3D12DescriptorHeap* mSrvDescriptorHeap = nullptr;
    ID3D12DescriptorHeap* mDsvDescriptorHeap = nullptr;
    uint mCbvSrvDescriptorSize = 0;

	Mesh mSphere;
	Mesh mGrid;
	Mesh mColumn;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
    std::vector<D3D12_INPUT_ELEMENT_DESC> mQuadInputLayout;

    std::unordered_map<std::string, ID3DBlob*> mShaders;
    std::unordered_map<std::string, IDxcBlob*> mDxcByteCodes;
    std::unordered_map<std::string, ID3D12RootSignature*> mRootSignatures;
    std::unordered_map<std::string, ID3D12PipelineState*> mPSOs;
    std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
    std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;

    std::unordered_map<std::string, CD3DX12_GPU_DESCRIPTOR_HANDLE> mGPUViews;
    std::unordered_map<std::string, CD3DX12_CPU_DESCRIPTOR_HANDLE> mCPUViews;

    D3DImage* mGBufferA;
    D3DImage* mGBufferB;
    D3DImage* mGBufferC;
    D3DImage* mGBufferD;
    D3DImage* mGBufferE;
    D3DImage* mSceneDepthZ;
    D3DImage* mBufferSSAO;
    D3DImage* mBufferBlurTemp;
    D3DImage* mBufferBRDF;
    D3DImage* mTextureSDF[3];

    D3DResource* mBufferSDF[3];

    std::vector<std::unique_ptr<RenderItem>> mAllRitems;
    std::unique_ptr<MeshGeometry> mScreenFullGeo = nullptr;

    PassConstants mMainPassCB;
    std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
    std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
    std::unique_ptr<UploadBuffer<SSAOPassConstants>> SSAOCB = nullptr;
    std::unique_ptr<UploadBuffer<BlurSettingsConstants>> BlurCB = nullptr;
    std::unique_ptr<UploadBuffer<MeshSDFDescriptor>> MeshSDFCB = nullptr;
    std::unique_ptr<UploadBuffer<ObjectSDFConstants>> ObjectSDFCB = nullptr;
    std::unique_ptr<UploadBuffer<LightShaderParameters>> LightCB = nullptr;

	float mTheta = 1.5f * M_PI;
	float mPhi = M_PI / 4;
	float mRadius = 5.0f;

	POINT mLastMousePos;
    Camera mCamera;
};

