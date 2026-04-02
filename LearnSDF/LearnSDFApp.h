
#include <array>
#include "../common/d3dApp.h"
#include "../math/Color.h"
#include "../math/Vec3.h"
#include "../math/Vec4.h"
#include "../common/d3dUtil.h"
#include "../common/UploadBuffer.h"

class LearSDFApp : public D3DApp
{
public:
	LearSDFApp(HINSTANCE hInstance);
	~LearSDFApp();

	virtual bool Initialize()override;

private:
	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

	void BuildBoxGeometry();
	void BuildShadersAndInputLayout();
	void BuildPSO();

	void BuildDescriptorHeaps();
	void BuildConstantBuffers();
	void BuildRootSignature();

private:
	struct Vertex
	{
		Math::Vec3 Pos;
		Math::Color Color;
	}; 

	struct ObjectConstants
	{
		Math::Mat4 View;
		Math::Mat4 Proj;
	};

	ID3D12RootSignature* mRootSignature = nullptr;
	ID3D12DescriptorHeap* mCbvHeap = nullptr;

	std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB = nullptr;

	std::unique_ptr<MeshGeometry> mBoxGeo = nullptr;

	ID3DBlob* mvsByteCode = nullptr;
	ID3DBlob* mpsByteCode = nullptr;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	ID3D12PipelineState* mPSO = nullptr;

	Math::Mat4 mWorld{};
	Math::Mat4 mView{};
	Math::Mat4 mProj{};

	float mTheta = 1.5f * M_PI;
	float mPhi = M_PI / 4;
	float mRadius = 5.0f;

	POINT mLastMousePos;

};

