
#include "LearnSDFApp.h"
#include <filesystem>

std::vector<std::string> g_texNames =
{
    "columnDiffuseMap",
    "columnNormalMap",
    "columnMetallic",
    "columnRoughness",

    "grayDiffuseMap",
    "grayNormalMap",
    "grayMetallic",
    "grayRoughness",

    "sphereDiffuseMap",
    "sphereNormalMap",
    "sphereMetallic",
    "sphereRoughness",
};

LearnSDFApp::LearnSDFApp(HINSTANCE hInstance)
	: D3DApp(hInstance)
{
}

LearnSDFApp::~LearnSDFApp()
{
}

auto ReadFloat = [](unsigned char*& p) -> float {
    float* pval = (float*)p;
	float res = *pval++;
    p = (unsigned char*)pval;
    return res;
    };

auto ReadInt = [](unsigned char*& p) -> int {
    int* pval = (int*)p;
    int res = *pval++;
    p = (unsigned char*)pval;
    return res;
    };

auto ReadShort = [](unsigned char*& p) -> short {
    short* pval = (short*)p;
    short res = *pval++;
    p = (unsigned char*)pval;
    return res;
    };

static void LoadMesh(const std::string& meshname, const std::string& indexname, Mesh& outmesh)
{
    size_t fileSize = 0;
    unsigned char* fileContent = d3dUtil::LoadFileContent(indexname.c_str(), fileSize);
	for (int i = 0; i < fileSize / 2; ++i)
	{
		uint idx = ReadShort(fileContent);
        outmesh.Indices32.push_back(idx);
	}

    fileContent = d3dUtil::LoadFileContent(meshname.c_str(), fileSize);
	for (int i = 0; i < outmesh.Indices32.size(); ++i)
    {
        Vertex Vertex;
        Vertex.Position.x = ReadFloat(fileContent);
        Vertex.Position.y = ReadFloat(fileContent);
        Vertex.Position.z = ReadFloat(fileContent);
        Vertex.Normal.x = ReadFloat(fileContent);
        Vertex.Normal.y = ReadFloat(fileContent);
        Vertex.Normal.z = ReadFloat(fileContent);
        Vertex.TangentU.x = ReadFloat(fileContent);
        Vertex.TangentU.y = ReadFloat(fileContent);
        Vertex.TangentU.z = ReadFloat(fileContent);
        Vertex.TexC.x = ReadFloat(fileContent);
        Vertex.TexC.y = ReadFloat(fileContent);
        outmesh.Vertices.push_back(Vertex);
	}
}

static bool IsFileExit(const std::string& FileName)
{
    return std::filesystem::exists(FileName);
}

static void CreateMeshSDFTexture(Mesh* pmesh)
{
    std::vector<uint8_t> MeshSDF;

    std::string MeshSDFPath = "res/MeshSDF/" + (pmesh->MeshName) + ".sdf";
    if (IsFileExit(MeshSDFPath)) //Read SDF from file
    {
        size_t fileSize = 0;
        unsigned char* fileContent = d3dUtil::LoadFileContent(MeshSDFPath.c_str(), fileSize);
        MeshSDFDescriptor& Descriptor = pmesh->SDFDescriptor;
        Descriptor.Center.x = ReadFloat(fileContent);
        Descriptor.Center.y = ReadFloat(fileContent);
        Descriptor.Center.z = ReadFloat(fileContent);
        Descriptor.Extent = ReadFloat(fileContent);
        Descriptor.Resolution = ReadInt(fileContent);

        int SDFDataCount = Descriptor.Resolution * Descriptor.Resolution * Descriptor.Resolution;
        const uint8_t* SDFData = fileContent;
        MeshSDF.assign(SDFData, SDFData + SDFDataCount);
    }
    else // Bulid SDF and save to file
    {
    }

    int SDFResolution = pmesh->SDFDescriptor.Resolution;
}

void LearnSDFApp::InitMesh()
{
    LoadMesh("res/column.bin", "res/columnindex.bin", mColumn);
	mColumn.MeshName = "Column";
    mSphere.CreateSphere(0.5f, 20, 20);
    mSphere.MeshName = ("SphereMesh");
    mGrid.CreateGrid(20.0f, 30.0f, 60, 40);
    mGrid.MeshName =("GridMesh");

	Mesh* meshs[3] = {&mSphere, &mGrid, &mColumn};
	for (int i = 0; i < 3; ++i)
    {
        CreateMeshSDFTexture(meshs[i]);
	}
}

bool LearnSDFApp::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc, nullptr));

    mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    LoadTextures();
    BuildBuffers();
	BuildDescriptorHeaps();
	BuildRootSignature();
    BuildShadersAndInputLayout();
    InitMesh();
    BuildMaterials();
    BuildRenderItems();
	BuildPSO();

    PassCB = std::make_unique<UploadBuffer<PassConstants>>(md3dDevice, 1, true);
    ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(md3dDevice, 3, true);
    SSAOCB = std::make_unique<UploadBuffer<SSAOPassConstants>>(md3dDevice, 1, true);
    BlurCB = std::make_unique<UploadBuffer<BlurSettingsConstants>>(md3dDevice, 1, true);

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();


	return true;
}

void LearnSDFApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void LearnSDFApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void LearnSDFApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = MATH_DEG_TO_RAD(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = MATH_DEG_TO_RAD(0.25f * static_cast<float>(y - mLastMousePos.y));

		// Update angles based on input to orbit camera around box.
		mTheta += dx;
		mPhi += dy;

		// Restrict the angle mPhi.
		mPhi = Clamp(mPhi, 0.1f, M_PI - 0.1f);
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		// Make each pixel correspond to 0.005 unit in the scene.
		float dx = 0.005f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.005f * static_cast<float>(y - mLastMousePos.y);

		// Update the camera radius based on input.
		mRadius += dx - dy;

		// Restrict the radius.
		mRadius = Clamp(mRadius, 3.0f, 15.0f);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void LearnSDFApp::OnResize()
{
	D3DApp::OnResize();

	Math::Mat4::CreatePerspective(45, AspectRatio(), 1.0f, 1000.0f, &mProj);
}

void LearnSDFApp::Update(const GameTimer& gt)
{
    OnKeyboardInput(gt);

    UpdateObjectCBs(gt);
    UpdateMainPassCB(gt);
}

void LearnSDFApp::OnKeyboardInput(const GameTimer& gt)
{

}

void LearnSDFApp::UpdateObjectCBs(const GameTimer& gt)
{
    for (auto& e : mAllRitems)
    {
        ObjectConstants objConstants;
        objConstants.PrevWorld = e->World;
        objConstants.World = e->World;
        objConstants.TexTransform = e->TexTransform;
        objConstants.MaterialIndex = e->Mat->MatCBIndex;

        ObjectCB->CopyData(e->ObjCBIndex, objConstants);
    }
}

std::vector<float> CalcGaussWeights(float sigma)
{
    float twoSigma2 = 2.0f * sigma * sigma;

    // Estimate the blur radius based on sigma since sigma controls the "width" of the bell curve.
    // For example, for sigma = 3, the width of the bell curve is 
    int blurRadius = (int)ceil(2.0f * sigma);

    const int MaxBlurRadius = 5;
    assert(blurRadius <= MaxBlurRadius);

    std::vector<float> weights;
    weights.resize(2 * blurRadius + 1);

    float weightSum = 0.0f;

    for (int i = -blurRadius; i <= blurRadius; ++i)
    {
        float x = (float)i;

        weights[i + blurRadius] = expf(-x * x / twoSigma2);

        weightSum += weights[i + blurRadius];
    }

    // Divide by the sum so all the weights add up to 1.0.
    for (int i = 0; i < weights.size(); ++i)
    {
        weights[i] /= weightSum;
    }

    return weights;
}

void LearnSDFApp::UpdateMainPassCB(const GameTimer& gt)
{
    //mMainPassCB.View = mCamera.GetView();
    //mMainPassCB.Proj = mCamera.GetProj();
    //mMainPassCB.ViewProj = mMainPassCB.View * mMainPassCB.Proj;
    //mMainPassCB.InvView = mMainPassCB.View.GetInversed();
    //mMainPassCB.InvProj = mMainPassCB.Proj.GetInversed();
    //mMainPassCB.InvViewProj = mMainPassCB.ViewProj.GetInversed();
    mMainPassCB.View = Math::Mat4(-0.5, 0, -0.86603, 0,
        0.00, 1.0, 0.00, 0.00,
        0.866, 0, -0.5, 0.0,
        0.3609, -3.2, 8.625, 1.0
    );
    mMainPassCB.Proj = Math::Mat4(1.358, 0, 0, 0,
        0.00, 2.414, 0.00, 0.00,
        -0.00059, -0.00015, 1.001, 1.0,
        0, 0, -0.1001, 0.0
    );
    mMainPassCB.ViewProj = Math::Mat4(-0.67917, -0.00067, -0.86689, -0.86603,
        0.00, 2.41421, 0.00, 0.00,
        1.17596, -0.00039, -0.5005, -0.50,
        0.49178, -7.71883, 8.53363, 8.62509
    );
    mMainPassCB.InvProj = Math::Mat4(0.73638, 0, 0, 0,
        0.00, 0.41421, 0.00, 0.00,
        0, 0, 0, -9.99,
        -0.00014, -0.00032, 1, 10
    );

    //mMainPassCB.EyePosW = mCamera.GetPosition();
    mMainPassCB.RenderTargetSize.Set((float)mClientWidth, (float)mClientHeight);
    mMainPassCB.InvRenderTargetSize.Set(1.0f / mClientWidth, 1.0f / mClientHeight);
    mMainPassCB.NearZ = 1.0f;
    mMainPassCB.FarZ = 1000.0f;
    mMainPassCB.TotalTime = gt.TotalTime();
    mMainPassCB.DeltaTime = gt.DeltaTime();

    auto currPassCB = PassCB.get();
    currPassCB->CopyData(0, mMainPassCB);

    // SSAO
    SSAOPassConstants SSAOPassCB;
    SSAOPassCB.ProjTex = Math::Mat4(0.679, 0, 0, 0,
        0, -1.207, 0, 0,
        0.5, 0.5, 1.0001, 1,
        0, 0, -0.1001, 0
    );
    SSAOPassCB.OcclusionRadius = 0.03f;
    SSAOPassCB.OcclusionFadeStart = 0.01f;
    SSAOPassCB.OcclusionFadeEnd = 0.03f;
    SSAOPassCB.SurfaceEpsilon = 0.001f;
    auto currSSAOCB = SSAOCB.get();
    currSSAOCB->CopyData(0, SSAOPassCB);

    //Blur
    auto Weights = CalcGaussWeights(2.5f);
    int BlurRadius = (int)Weights.size() / 2;
    BlurSettingsConstants BlurConstants;
    BlurConstants.gBlurRadius = BlurRadius;
    size_t DataSize = Weights.size() * sizeof(float);
    memcpy_s(&(BlurConstants.w0), DataSize, Weights.data(), DataSize);
    auto blurPassCB = BlurCB.get();
    blurPassCB->CopyData(0, BlurConstants);
}

void LearnSDFApp::DrawRenderItems(ID3D12GraphicsCommandList* mCommandList)
{
    uint objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

    ID3D12Resource* objectCB = ObjectCB->Resource();

    // For each render item...
    for (size_t i = 0; i < mAllRitems.size(); ++i)
    {
        auto ri = mAllRitems[i].get();

        mCommandList->IASetVertexBuffers(0, 1, &ri->MeshGeo->VertexBufferView());
        mCommandList->IASetIndexBuffer(&ri->MeshGeo->IndexBufferView());
        mCommandList->IASetPrimitiveTopology(ri->PrimitiveType);

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;

        mCommandList->SetGraphicsRootConstantBufferView(0, objCBAddress);

        mCommandList->DrawIndexedInstanced(ri->IndexCount, 1, 0, 0, 0);
    }
}

void LearnSDFApp::Draw(const GameTimer& gt)
{
	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(mDirectCmdListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc, nullptr));

    ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Set the viewport and scissor rect.  This needs to be reset whenever the command list is reset.
	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

    {	// basepass
        D3D12_RESOURCE_BARRIER barriers[6];
        barriers[0] = InitResourceBarrier(mGBufferA->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
        barriers[1] = InitResourceBarrier(mGBufferB->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
        barriers[2] = InitResourceBarrier(mGBufferC->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
        barriers[3] = InitResourceBarrier(mGBufferD->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
        barriers[4] = InitResourceBarrier(mGBufferE->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
        barriers[5] = InitResourceBarrier(mSceneDepthZ->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);

        D3D12_CPU_DESCRIPTOR_HANDLE colorRT[5] = { mCPUViews["GBufferARTV"], mCPUViews["GBufferBRTV"], mCPUViews["GBufferCRTV"], mCPUViews["GBufferDRTV"], mCPUViews["GBufferERTV"] };
        D3D12_CPU_DESCRIPTOR_HANDLE dsRT = mDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        mCommandList->OMSetRenderTargets(5, colorRT, FALSE, &dsRT);
        float clearColor[] = { 0.0f,0.0f,0.0f,0.0f };
        float clearColor2[] = { 0.0f,0.0f };
        mCommandList->ClearRenderTargetView(colorRT[0], clearColor, 0, nullptr);
        mCommandList->ClearRenderTargetView(colorRT[1], clearColor, 0, nullptr);
        mCommandList->ClearRenderTargetView(colorRT[2], clearColor, 0, nullptr);
        mCommandList->ClearRenderTargetView(colorRT[3], clearColor, 0, nullptr);
        mCommandList->ClearRenderTargetView(colorRT[4], clearColor2, 0, nullptr);
        mCommandList->ClearDepthStencilView(dsRT, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

        mCommandList->SetPipelineState(mPSOs["basepass"]);
        mCommandList->SetGraphicsRootSignature(mRootSignatures["basepass"]);

        auto passCB = PassCB->Resource();
        mCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

        mCommandList->SetGraphicsRootDescriptorTable(2, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

        DrawRenderItems(mCommandList);

        barriers[0] = InitResourceBarrier(mGBufferA->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
        barriers[1] = InitResourceBarrier(mGBufferB->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
        barriers[2] = InitResourceBarrier(mGBufferC->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
        barriers[3] = InitResourceBarrier(mGBufferD->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
        barriers[4] = InitResourceBarrier(mGBufferE->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
        barriers[5] = InitResourceBarrier(mSceneDepthZ->mResource, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);
    }

    {	// SSAO
        D3D12_RESOURCE_BARRIER barriers[3];
        barriers[0] = InitResourceBarrier(mGBufferB->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        barriers[1] = InitResourceBarrier(mSceneDepthZ->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        barriers[2] = InitResourceBarrier(mBufferSSAO->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);

        D3D12_CPU_DESCRIPTOR_HANDLE colorRT[1] = { mCPUViews["BufferSSAORTV"] };
        mCommandList->OMSetRenderTargets(1, colorRT, FALSE, nullptr);
        float clearColor[] = { 0.0f,0.0f,0.0f,0.0f };
        mCommandList->ClearRenderTargetView(colorRT[0], clearColor, 0, nullptr);

        mCommandList->SetPipelineState(mPSOs["SSAO"]);
        mCommandList->SetGraphicsRootSignature(mRootSignatures["SSAO"]);

        auto passCB = PassCB->Resource();
        mCommandList->SetGraphicsRootConstantBufferView(0, passCB->GetGPUVirtualAddress());
        mCommandList->SetGraphicsRootConstantBufferView(1, SSAOCB->Resource()->GetGPUVirtualAddress());
        mCommandList->SetGraphicsRootDescriptorTable(2, mGPUViews["GBufferBSRV"]);//mGPUViews["GBufferBSRV"]
        mCommandList->SetGraphicsRootDescriptorTable(3, mGPUViews["SceneDepthZSRV"]);//mGPUViews["SceneDepthZSRV"]

        mCommandList->IASetVertexBuffers(0, 1, &mScreenFullGeo->VertexBufferView());
        mCommandList->IASetIndexBuffer(&mScreenFullGeo->IndexBufferView());
        mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        mCommandList->DrawIndexedInstanced(mScreenFullGeo->IndexCount, 1, 0, 0, 0);

        barriers[0] = InitResourceBarrier(mGBufferB->mResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
        barriers[1] = InitResourceBarrier(mSceneDepthZ->mResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
        barriers[2] = InitResourceBarrier(mBufferSSAO->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);
    }

    {   //Blur
        D3D12_RESOURCE_BARRIER barriers[2];
        barriers[0] = InitResourceBarrier(mBufferSSAO->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        barriers[1] = InitResourceBarrier(mBufferBlurTemp->mResource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);

        mCommandList->SetPipelineState(mPSOs["HorzBlur"]);
        mCommandList->SetComputeRootSignature(mRootSignatures["Blur"]);
        auto passCB = BlurCB->Resource();
        mCommandList->SetComputeRootConstantBufferView(0, passCB->GetGPUVirtualAddress());
        mCommandList->SetComputeRootDescriptorTable(1, mGPUViews["BufferSSAOSRV"]);
        mCommandList->SetComputeRootDescriptorTable(2, mGPUViews["BufferBlurUAV"]);
        UINT NumGroupsX = (UINT)ceilf(mClientWidth / 256.0f);
        UINT NumGroupsY = mClientHeight;
        mCommandList->Dispatch(NumGroupsX, NumGroupsY, 1);

        barriers[0] = InitResourceBarrier(mBufferSSAO->mResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        barriers[1] = InitResourceBarrier(mBufferBlurTemp->mResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);

        mCommandList->SetComputeRootDescriptorTable(1, mGPUViews["BufferBlurSRV"]);
        mCommandList->SetComputeRootDescriptorTable(2, mGPUViews["BufferSSAOUAV"]);
        mCommandList->SetPipelineState(mPSOs["VertBlur"]);
        NumGroupsX = mClientWidth;
        NumGroupsY = (UINT)ceilf(mClientHeight / 256.0f);
        mCommandList->Dispatch(NumGroupsX, NumGroupsY, 1);

        barriers[0] = InitResourceBarrier(mBufferSSAO->mResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
        barriers[1] = InitResourceBarrier(mBufferBlurTemp->mResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
        mCommandList->ResourceBarrier(_countof(barriers), barriers);
    }

	// Clear the back buffer and depth buffer.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Math::Color::WHITE.getPtr(), 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Wait until frame commands are complete.  This waiting is inefficient and is
	// done for simplicity.  Later we will show how to organize our rendering code
	// so we do not have to wait per frame.
	FlushCommandQueue();
}

// 
// app define
//

void LearnSDFApp::BuildShadersAndInputLayout()
{
	HRESULT hr = S_OK;

    mDxcByteCodes["basepassVS"] = d3dUtil::DxcCompileShader(L"LearnSDF\\shader\\basepass.hlsl", nullptr, 0, L"VS", L"vs_6_0");
    mDxcByteCodes["basepassPS"] = d3dUtil::DxcCompileShader(L"LearnSDF\\shader\\basepass.hlsl", nullptr, 0, L"PS", L"ps_6_0");
    mDxcByteCodes["SSAOVS"] = d3dUtil::DxcCompileShader(L"LearnSDF\\shader\\SSAO.hlsl", nullptr, 0, L"VS", L"vs_6_0");
    mDxcByteCodes["SSAOPS"] = d3dUtil::DxcCompileShader(L"LearnSDF\\shader\\SSAO.hlsl", nullptr, 0, L"PS", L"ps_6_0");
    mShaders["HorzBlurCS"] = d3dUtil::CompileShader(L"LearnSDF\\shader\\Blur.hlsl", nullptr, "HorzBlurCS", "cs_5_0");
    mShaders["VertBlurCS"] = d3dUtil::CompileShader(L"LearnSDF\\shader\\Blur.hlsl", nullptr, "VertBlurCS", "cs_5_0");

    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    mQuadInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

}

void LearnSDFApp::BuildPSO()
{
	{   // basepass
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
        ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
        psoDesc.InputLayout = { mInputLayout.data(), (uint)mInputLayout.size() };
        psoDesc.pRootSignature = mRootSignatures["basepass"];
        psoDesc.VS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["basepassVS"]->GetBufferPointer()),
            mDxcByteCodes["basepassVS"]->GetBufferSize()
        };
        psoDesc.PS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["basepassPS"]->GetBufferPointer()),
            mDxcByteCodes["basepassPS"]->GetBufferSize()
        };
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 5;
        psoDesc.RTVFormats[0] = mGBufferA->mRTVFormat;
        psoDesc.RTVFormats[1] = mGBufferB->mRTVFormat;
        psoDesc.RTVFormats[2] = mGBufferC->mRTVFormat;
        psoDesc.RTVFormats[3] = mGBufferD->mRTVFormat;
        psoDesc.RTVFormats[4] = mGBufferE->mRTVFormat;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;
        psoDesc.DSVFormat = mSceneDepthZ->mDSVFormat;
        ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs["basepass"])));
    }
    {   // SSAO
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
        ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
        psoDesc.InputLayout = { mQuadInputLayout.data(), (uint)mQuadInputLayout.size() };
        psoDesc.pRootSignature = mRootSignatures["SSAO"];
        psoDesc.VS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["SSAOVS"]->GetBufferPointer()),
            mDxcByteCodes["SSAOVS"]->GetBufferSize()
        };
        psoDesc.PS =
        {
            reinterpret_cast<BYTE*>(mDxcByteCodes["SSAOPS"]->GetBufferPointer()),
            mDxcByteCodes["SSAOPS"]->GetBufferSize()
        };
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable = FALSE;        // 关闭深度测试
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 关闭深度写入
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = mBufferSSAO->mRTVFormat;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;
        psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
        ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs["SSAO"])));
    }
    {   //Blur
        D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
        computePsoDesc.pRootSignature = mRootSignatures["Blur"];
        computePsoDesc.CS =
        {
            reinterpret_cast<BYTE*>(mShaders["HorzBlurCS"]->GetBufferPointer()),
            mShaders["HorzBlurCS"]->GetBufferSize()
        };
        computePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        ThrowIfFailed(md3dDevice->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&mPSOs["HorzBlur"])));
    }
    {   //Blur
        D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
        computePsoDesc.pRootSignature = mRootSignatures["Blur"];
        computePsoDesc.CS =
        {
            reinterpret_cast<BYTE*>(mShaders["VertBlurCS"]->GetBufferPointer()),
            mShaders["VertBlurCS"]->GetBufferSize()
        };
        computePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        ThrowIfFailed(md3dDevice->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&mPSOs["VertBlur"])));
    }
}

void LearnSDFApp::BuildMaterials()
{
    auto column = std::make_unique<Material>();
    column->Name = "column";
    column->MatCBIndex = 0;

    auto gray = std::make_unique<Material>();
    gray->Name = "gray";
    gray->MatCBIndex = 1;

    auto sphere = std::make_unique<Material>();
    sphere->Name = "sphere";
    sphere->MatCBIndex = 2;

    mMaterials["column"] = std::move(column);
    mMaterials["gray"] = std::move(gray);
    mMaterials["sphere"] = std::move(sphere);
}

void LearnSDFApp::LoadTextures()
{
    std::vector<std::wstring> texFilenames =
    {
        L"res/column_albedo.dds",
        L"res/column_normal.dds",
        L"res/column_metallic.dds",
        L"res/column_roughness.dds",

        L"res/hardwood-brown-planks-albedo.dds",
        L"res/hardwood-brown-planks-normal-dx.dds",
        L"res/hardwood-brown-planks-Metallic.dds",
        L"res/hardwood-brown-planks-roughness.dds",

        L"res/rustediron2_basecolor.dds",
        L"res/rustediron2_normal.dds",
        L"res/rustediron2_metallic.dds",
        L"res/rustediron2_roughness.dds"
    };

    for (int i = 0; i < (int)g_texNames.size(); ++i)
    {
        auto texMap = std::make_unique<Texture>();
        texMap->Name = g_texNames[i];
        texMap->Filename = texFilenames[i];
        bool issrgb = i % 4 == 0;       // basecolor是srgb
        ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice,
            mCommandList, texMap->Filename.c_str(),
            texMap->Resource, texMap->UploadHeap, 0, nullptr, issrgb));

        mTextures[texMap->Name] = std::move(texMap);
    }
}

void LearnSDFApp::BuildRenderItems()
{
    std::unique_ptr<RenderItem> gridRitem = std::make_unique<RenderItem>();
    gridRitem->World.Scale(2.0f, 1.0f, 2.0f);
    gridRitem->World.Translate(0.0f, 0.0f, 0.0f);
    gridRitem->TexTransform.Scale(8.0f, 8.0f, 1.0f);
    gridRitem->ObjCBIndex = 1;
    gridRitem->Mat = mMaterials["gray"].get();
    gridRitem->Geo = &mGrid;
    gridRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    gridRitem->IndexCount = mGrid.Indices32.size();
    mAllRitems.push_back(std::move(gridRitem));

    auto sphereRitem = std::make_unique<RenderItem>();
    sphereRitem->World.Scale(1.0f, 1.0f, 1.0f);
    sphereRitem->World.Translate(4.0f, 2.0f, 0.0f);
    sphereRitem->ObjCBIndex = 2;
    sphereRitem->Mat = mMaterials["sphere"].get();
    sphereRitem->Geo = &mSphere;
    sphereRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    sphereRitem->IndexCount = mSphere.Indices32.size();
    mAllRitems.push_back(std::move(sphereRitem));

    auto columnRitem = std::make_unique<RenderItem>();
    columnRitem->World.Scale(10.0f, 10.0f, 10.0f);
    columnRitem->World.Translate(0.0f, -0.1f, 0.0f);
    columnRitem->ObjCBIndex = 0;
    columnRitem->Mat = mMaterials["column"].get();
    columnRitem->Geo = &mColumn;
    columnRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    columnRitem->IndexCount = mColumn.Indices32.size();
    mAllRitems.push_back(std::move(columnRitem));

    for (int i = 0; i < mAllRitems.size(); ++i)
    {
        Mesh* mesh = mAllRitems[i]->Geo;
        mAllRitems[i]->MeshGeo = std::make_unique<MeshGeometry>();

        const UINT vbByteSize = (UINT)mesh->Vertices.size() * sizeof(Vertex);
        const UINT ibByteSize = (UINT)mesh->Indices32.size() * sizeof(std::uint32_t);

        ThrowIfFailed(D3DCreateBlob(vbByteSize, &mAllRitems[i]->MeshGeo->VertexBufferCPU));
        CopyMemory(mAllRitems[i]->MeshGeo->VertexBufferCPU->GetBufferPointer(), &mesh->Vertices[0], vbByteSize);

        ThrowIfFailed(D3DCreateBlob(ibByteSize, &mAllRitems[i]->MeshGeo->IndexBufferCPU));
        CopyMemory(mAllRitems[i]->MeshGeo->IndexBufferCPU->GetBufferPointer(), mesh->Indices32.data(), ibByteSize);

        mAllRitems[i]->MeshGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice,
            mCommandList, (const void*)&mesh->Vertices[0], vbByteSize, mAllRitems[i]->MeshGeo->VertexBufferUploader);

        mAllRitems[i]->MeshGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice,
            mCommandList, mesh->Indices32.data(), ibByteSize, mAllRitems[i]->MeshGeo->IndexBufferUploader);

        mAllRitems[i]->MeshGeo->VertexByteStride = sizeof(Vertex);
        mAllRitems[i]->MeshGeo->VertexBufferByteSize = vbByteSize;
        mAllRitems[i]->MeshGeo->IndexFormat = DXGI_FORMAT_R32_UINT;
        mAllRitems[i]->MeshGeo->IndexBufferByteSize = ibByteSize;
    }
    {
        mScreenFullGeo = std::make_unique<MeshGeometry>();
        std::vector<float> vertexData = {
            -3.0f, -1.0f, 0.0f, -1.0f, 1.0f,
            1.0f, 3.0f, 0.0f, 1.0f, -1.0f,
            1.0f, -1.0f, 0.0f, 1.0f, 1.0f
        };
        short indexData[3] = { 0, 1, 2 };

        const UINT vbByteSize = (UINT)vertexData.size() * sizeof(float);
        const UINT ibByteSize = (UINT)3 * sizeof(std::uint16_t);

        ThrowIfFailed(D3DCreateBlob(vbByteSize, &mScreenFullGeo->VertexBufferCPU));
        CopyMemory(mScreenFullGeo->VertexBufferCPU->GetBufferPointer(), vertexData.data(), vbByteSize);

        ThrowIfFailed(D3DCreateBlob(ibByteSize, &mScreenFullGeo->IndexBufferCPU));
        CopyMemory(mScreenFullGeo->IndexBufferCPU->GetBufferPointer(), indexData, ibByteSize);

        mScreenFullGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice,
            mCommandList, vertexData.data(), vbByteSize, mScreenFullGeo->VertexBufferUploader);

        mScreenFullGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice,
            mCommandList, indexData, ibByteSize, mScreenFullGeo->IndexBufferUploader);

        mScreenFullGeo->VertexByteStride = sizeof(float) * 5;
        mScreenFullGeo->VertexBufferByteSize = vbByteSize;
        mScreenFullGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
        mScreenFullGeo->IndexBufferByteSize = ibByteSize;

        mScreenFullGeo->IndexCount = 3;
    }
}

static void CreateTexture2DSRV(ID3D12Device* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE inMemory, ID3D12Resource* inResource, DXGI_FORMAT inFormat, int inMipMapLevel)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = inFormat;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    pDevice->CreateShaderResourceView(inResource, &srvDesc, inMemory);
}

static void CreateTexture2DUAV(ID3D12Device* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE inMemory, ID3D12Resource* inResource, DXGI_FORMAT inFormat, int inMipMapLevel)
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = inFormat;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Texture2D.MipSlice = inMipMapLevel;
    pDevice->CreateUnorderedAccessView(inResource, nullptr, &uavDesc, inMemory);
}

static void CreateTexture2DRTV(ID3D12Device* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE inMemory, ID3D12Resource* inResource, DXGI_FORMAT inFormat)
{
    D3D12_RENDER_TARGET_VIEW_DESC srvDesc = {};
    srvDesc.Format = inFormat;
    srvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    pDevice->CreateRenderTargetView(inResource, &srvDesc, inMemory);
}

static void CreateTexture2DDSV(ID3D12Device* pDevice, D3D12_CPU_DESCRIPTOR_HANDLE inMemory, D3DImage* inDSRT, D3D12_DSV_FLAGS inFlags = D3D12_DSV_FLAG_NONE)
{
    D3D12_DESCRIPTOR_HEAP_DESC d3dDescriptorHeapDescDSV = {};
    d3dDescriptorHeapDescDSV.NumDescriptors = 1;
    d3dDescriptorHeapDescDSV.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

    D3D12_DEPTH_STENCIL_VIEW_DESC d3dDSViewDesc = {};
    d3dDSViewDesc.Format = inDSRT->mDSVFormat;
    d3dDSViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    d3dDSViewDesc.Flags = inFlags;

    pDevice->CreateDepthStencilView(inDSRT->mResource, &d3dDSViewDesc, inMemory);
}

void LearnSDFApp::BuildDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 200;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

    //
    // Fill out the heap with actual descriptors.
    //
    CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), 0, mCbvSrvDescriptorSize);

    
    std::vector<ID3D12Resource*> tex2DList;
    for (int i = 0; i < g_texNames.size(); ++i)
    {
        tex2DList.push_back(mTextures[g_texNames[i]]->Resource);
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    for (UINT i = 0; i < (UINT)tex2DList.size(); ++i)
    {
        srvDesc.Format = tex2DList[i]->GetDesc().Format;
        srvDesc.Texture2D.MipLevels = tex2DList[i]->GetDesc().MipLevels;
        md3dDevice->CreateShaderResourceView(tex2DList[i], &srvDesc, hCpuDescriptor);

        // next descriptor
        hCpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
    }

    hCpuDescriptor = (mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    int viewCount = tex2DList.size();
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize), mGBufferA->mResource, mGBufferA->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize), mGBufferB->mResource, mGBufferB->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize), mGBufferC->mResource, mGBufferC->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize), mGBufferD->mResource, mGBufferD->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize), mGBufferE->mResource, mGBufferE->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize), mSceneDepthZ->mResource, mSceneDepthZ->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize), mBufferSSAO->mResource, mBufferSSAO->mSRVFormat, 0);
    CreateTexture2DSRV(md3dDevice, hCpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize), mBufferBlurTemp->mResource, mBufferBlurTemp->mSRVFormat, 0);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize), mBufferSSAO->mResource, mBufferSSAO->mFormat, 0);
    CreateTexture2DUAV(md3dDevice, hCpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize), mBufferBlurTemp->mResource, mBufferBlurTemp->mFormat, 0);
    viewCount = tex2DList.size();
    hCpuDescriptor = (mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    mCPUViews["GBufferASRV"] = hCpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize);
    mCPUViews["GBufferBSRV"] = hCpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize);
    mCPUViews["GBufferCSRV"] = hCpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize);
    mCPUViews["GBufferDSRV"] = hCpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize);
    mCPUViews["GBufferESRV"] = hCpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize);
    mCPUViews["SceneDepthZSRV"] = hCpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize);
    mCPUViews["BufferSSAOSRV"] = hCpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize);
    mCPUViews["BufferBlurSRV"] = hCpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize);
    mCPUViews["BufferSSAOUAV"] = hCpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize);
    mCPUViews["BufferBlurUAV"] = hCpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize);
    viewCount = tex2DList.size();
    mGPUViews["GBufferASRV"] = hGpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize);
    mGPUViews["GBufferBSRV"] = hGpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize);
    mGPUViews["GBufferCSRV"] = hGpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize);
    mGPUViews["GBufferDSRV"] = hGpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize);
    mGPUViews["GBufferESRV"] = hGpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize);
    mGPUViews["SceneDepthZSRV"] = hGpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize);
    mGPUViews["BufferSSAOSRV"] = hGpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize);
    mGPUViews["BufferBlurSRV"] = hGpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize);
    mGPUViews["BufferSSAOUAV"] = hGpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize);
    mGPUViews["BufferBlurUAV"] = hGpuDescriptor.Offset(viewCount++, mCbvSrvDescriptorSize);

    viewCount = SwapChainBufferCount;
    hCpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), 0, mRtvDescriptorSize);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(viewCount++, mRtvDescriptorSize), mGBufferA->mResource, mGBufferA->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(viewCount++, mRtvDescriptorSize), mGBufferB->mResource, mGBufferB->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(viewCount++, mRtvDescriptorSize), mGBufferC->mResource, mGBufferC->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(viewCount++, mRtvDescriptorSize), mGBufferD->mResource, mGBufferD->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(viewCount++, mRtvDescriptorSize), mGBufferE->mResource, mGBufferE->mRTVFormat);
    CreateTexture2DRTV(md3dDevice, hCpuDescriptor.Offset(viewCount++, mRtvDescriptorSize), mBufferSSAO->mResource, mBufferSSAO->mRTVFormat);
    viewCount = SwapChainBufferCount;
    hCpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), 0, mRtvDescriptorSize);
    mCPUViews["GBufferARTV"] = hCpuDescriptor.Offset(viewCount++, mRtvDescriptorSize);
    mCPUViews["GBufferBRTV"] = hCpuDescriptor.Offset(viewCount++, mRtvDescriptorSize);
    mCPUViews["GBufferCRTV"] = hCpuDescriptor.Offset(viewCount++, mRtvDescriptorSize);
    mCPUViews["GBufferDRTV"] = hCpuDescriptor.Offset(viewCount++, mRtvDescriptorSize);
    mCPUViews["GBufferERTV"] = hCpuDescriptor.Offset(viewCount++, mRtvDescriptorSize);
    mCPUViews["BufferSSAORTV"] = hCpuDescriptor.Offset(viewCount++, mRtvDescriptorSize);

    // DSV
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
    dsvHeapDesc.NumDescriptors = 100;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
        &dsvHeapDesc, IID_PPV_ARGS(&mDsvDescriptorHeap)));

    viewCount = 0;
    hCpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(mDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 0, mDsvDescriptorSize);
    CreateTexture2DDSV(md3dDevice, hCpuDescriptor.Offset(viewCount++, mDsvDescriptorSize), mSceneDepthZ);

    viewCount = 0;
    hCpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(mDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 0, mDsvDescriptorSize);
    mCPUViews["SceneDepthZDSV"] = hCpuDescriptor.Offset(viewCount++, mDsvDescriptorSize);
}

void LearnSDFApp::BuildBuffers()
{
    int bufferWidth = mClientWidth;
    int bufferHeight = mClientHeight;

    mSceneDepthZ = Init2DRTImage(md3dDevice, mCommandList, bufferWidth, bufferHeight, 0,
        DXGI_FORMAT_R32G8X24_TYPELESS, DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,
        DXGI_FORMAT_D32_FLOAT_S8X24_UINT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
        { DXGI_FORMAT_D32_FLOAT_S8X24_UINT, {0.0f,0} });
    mSceneDepthZ->mResource->SetName(L"SceneDepthZ");
    mGBufferA = Init2DRTImage(md3dDevice, mCommandList, bufferWidth, bufferHeight, 0,
        DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT,
        DXGI_FORMAT_R32G32B32A32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
        { DXGI_FORMAT_R32G32B32A32_FLOAT, {0.0f,0.0f,0.0f,0.0f} });
    mGBufferA->mResource->SetName(L"GBufferA");
    mGBufferB = Init2DRTImage(md3dDevice, mCommandList, bufferWidth, bufferHeight, 0,
        DXGI_FORMAT_R8G8B8A8_SNORM, DXGI_FORMAT_R8G8B8A8_SNORM,
        DXGI_FORMAT_R8G8B8A8_SNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
        { DXGI_FORMAT_R8G8B8A8_SNORM, {0.0f,0.0f,0.0f,0.0f} });
    mGBufferB->mResource->SetName(L"GBufferB");
    mGBufferC = Init2DRTImage(md3dDevice, mCommandList, bufferWidth, bufferHeight, 0,
        DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT,
        DXGI_FORMAT_R32G32B32A32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
        { DXGI_FORMAT_R32G32B32A32_FLOAT, {0.0f,0.0f,0.0f,0.0f} });
    mGBufferC->mResource->SetName(L"GBufferC");
    mGBufferD = Init2DRTImage(md3dDevice, mCommandList, bufferWidth, bufferHeight, 0,
        DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM,
        DXGI_FORMAT_B8G8R8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
        { DXGI_FORMAT_B8G8R8A8_UNORM, {0.0f,0.0f,0.0f,1.0f} });
    mGBufferD->mResource->SetName(L"GBufferD");
    mGBufferE = Init2DRTImage(md3dDevice, mCommandList, bufferWidth, bufferHeight, 0,
        DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32G32_FLOAT,
        DXGI_FORMAT_R32G32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
        { DXGI_FORMAT_R32G32_FLOAT, {0.0f,0.0f,0.0f,1.0f} });
    mGBufferE->mResource->SetName(L"GBufferE");
    mBufferSSAO = Init2DRTImage(md3dDevice, mCommandList, bufferWidth, bufferHeight, 0,
        DXGI_FORMAT_R16_FLOAT, DXGI_FORMAT_R16_FLOAT,
        DXGI_FORMAT_R16_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        { DXGI_FORMAT_R16_FLOAT, {0.0f,0.0f,0.0f,1.0f} });
    mBufferSSAO->mResource->SetName(L"BufferSSAO");
    mBufferBlurTemp = Init2DRTImage(md3dDevice, mCommandList, bufferWidth, bufferHeight, 0,
        DXGI_FORMAT_R16_FLOAT, DXGI_FORMAT_R16_FLOAT,
        DXGI_FORMAT_R16_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        { DXGI_FORMAT_R16_FLOAT, {0.0f,0.0f,0.0f,1.0f} });
    mBufferBlurTemp->mResource->SetName(L"BufferBlur");
}

static std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers()
{
    // Applications usually only need a handful of samplers.  So just define them all up front
    // and keep them available as part of the root signature.  

    const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
        0, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
        1, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
        2, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
        3, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
        4, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
        0.0f,                             // mipLODBias
        8);                               // maxAnisotropy

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
        5, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
        0.0f,                              // mipLODBias
        8);                                // maxAnisotropy

    return {
        pointWrap, pointClamp,
        linearWrap, linearClamp,
        anisotropicWrap, anisotropicClamp };
}

void LearnSDFApp::BuildRootSignature()
{
	{   // basepass
        CD3DX12_ROOT_PARAMETER slotRootParameter[3];

        CD3DX12_DESCRIPTOR_RANGE texTable1;
        texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 200, 0, 0);

        // Create root CBVs.
        slotRootParameter[0].InitAsConstantBufferView(0);
        slotRootParameter[1].InitAsConstantBufferView(1);
        slotRootParameter[2].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);

        // A root signature is an array of root parameters.
        auto staticSamplers = GetStaticSamplers();
        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter, staticSamplers.size(), staticSamplers.data(),
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
        ID3DBlob* serializedRootSig = nullptr;
        ID3DBlob* errorBlob = nullptr;
        HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
            &serializedRootSig, &errorBlob);

        if (errorBlob != nullptr)
        {
            ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        ThrowIfFailed(hr);

        ThrowIfFailed(md3dDevice->CreateRootSignature(
            0,
            serializedRootSig->GetBufferPointer(),
            serializedRootSig->GetBufferSize(),
            IID_PPV_ARGS(&mRootSignatures["basepass"])));
	}
    {   // SSAO
        CD3DX12_ROOT_PARAMETER slotRootParameter[4];

        CD3DX12_DESCRIPTOR_RANGE texTable1;
        texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
        CD3DX12_DESCRIPTOR_RANGE texTable2;
        texTable2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);

        // Create root CBVs.
        slotRootParameter[0].InitAsConstantBufferView(0);
        slotRootParameter[1].InitAsConstantBufferView(1);
        slotRootParameter[2].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);
        slotRootParameter[3].InitAsDescriptorTable(1, &texTable2, D3D12_SHADER_VISIBILITY_PIXEL);

        // A root signature is an array of root parameters.
        auto staticSamplers = GetStaticSamplers();
        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter, staticSamplers.size(), staticSamplers.data(),
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
        ID3DBlob* serializedRootSig = nullptr;
        ID3DBlob* errorBlob = nullptr;
        HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
            &serializedRootSig, &errorBlob);

        if (errorBlob != nullptr)
        {
            ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        ThrowIfFailed(hr);

        ThrowIfFailed(md3dDevice->CreateRootSignature(
            0,
            serializedRootSig->GetBufferPointer(),
            serializedRootSig->GetBufferSize(),
            IID_PPV_ARGS(&mRootSignatures["SSAO"])));
    }
    {   // Blur
        CD3DX12_ROOT_PARAMETER slotRootParameter[3];

        CD3DX12_DESCRIPTOR_RANGE texTable1;
        texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
        CD3DX12_DESCRIPTOR_RANGE uavTable1;
        uavTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

        // Create root CBVs.
        slotRootParameter[0].InitAsConstantBufferView(0);
        slotRootParameter[1].InitAsDescriptorTable(1, &texTable1);
        slotRootParameter[2].InitAsDescriptorTable(1, &uavTable1);

        // A root signature is an array of root parameters.
        auto staticSamplers = GetStaticSamplers();
        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter, staticSamplers.size(), staticSamplers.data(),
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
        ID3DBlob* serializedRootSig = nullptr;
        ID3DBlob* errorBlob = nullptr;
        HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
            &serializedRootSig, &errorBlob);

        if (errorBlob != nullptr)
        {
            ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        ThrowIfFailed(hr);

        ThrowIfFailed(md3dDevice->CreateRootSignature(
            0,
            serializedRootSig->GetBufferPointer(),
            serializedRootSig->GetBufferSize(),
            IID_PPV_ARGS(&mRootSignatures["Blur"])));
    }
}

