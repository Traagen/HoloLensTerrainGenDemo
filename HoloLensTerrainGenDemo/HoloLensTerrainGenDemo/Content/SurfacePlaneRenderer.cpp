#include "pch.h"
#include "SurfacePlaneRenderer.h"
#include "Common\DirectXHelper.h"
#include "Common\StepTimer.h"

using namespace HoloLensTerrainGenDemo;
using namespace DirectX;
using namespace Concurrency;
using namespace Windows::Perception::Spatial;
using namespace PlaneFinding;

SurfacePlaneRenderer::SurfacePlaneRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	m_deviceResources(deviceResources) {
	m_planeList.clear();

	CreateShaders();
}

SurfacePlaneRenderer::~SurfacePlaneRenderer() {
	ReleaseDeviceDependentResources();
}

void SurfacePlaneRenderer::CreateVertexResources() {
	if (m_planeList.size() < 1) {
		// No planes to draw.
		return;
	}

	// resources are created off-thread, so that they don't affect rendering latency.
	auto taskOptions = Concurrency::task_options();
	auto task = concurrency::create_task([this]() {
		// Build a vertex buffer containing 4 vertices for each plane, representing the quad of the bounded plane.
		int numPlanes = m_planeList.size();
		int numVerts = numPlanes * 4;
		XMFLOAT3 *verts = new XMFLOAT3[numVerts];
		int count = 0;
		for (auto p : m_planeList) {
			// for each plane in our list, build a quad and add the vertices to our verts list.
			// for now, we'll define our plane as a vertical quad located at the center of the plane's
			// bounding volume.
			auto center = p.bounds.Center;
			verts[count++] = XMFLOAT3(center.x - 0.1f, center.y - 0.1f, center.z);
			verts[count++] = XMFLOAT3(center.x + 0.1f, center.y - 0.1f, center.z);
			verts[count++] = XMFLOAT3(center.x + 0.1f, center.y + 0.1f, center.z);
			verts[count++] = XMFLOAT3(center.x - 0.1f, center.y + 0.1f, center.z);
		}

		// create the vertex buffer.
		CD3D11_BUFFER_DESC descBuffer(sizeof(verts), D3D11_USAGE_DEFAULT);
		D3D11_SUBRESOURCE_DATA dataBuffer;
		dataBuffer.pSysMem = verts;
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&descBuffer, &dataBuffer, &m_vertexBuffer));

		// clean up.
		delete verts;
	});
}

void SurfacePlaneRenderer::CreateDeviceDependentResources() {
	// Create a constant buffer for the plane data.
	CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
	DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&constantBufferDesc, nullptr, &m_constantBuffer));

	m_createdConstantBuffer = true;
}

void SurfacePlaneRenderer::CreateShaders() {
	m_usingVprtShaders = m_deviceResources->GetDeviceSupportsVprt();

	// On devices that do support the D3D11_FEATURE_D3D11_OPTIONS3::
	// VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature
	// we can avoid using a pass-through geometry shader to set the render
	// target array index, thus avoiding any overhead that would be
	// incurred by setting the geometry shader stage.
	std::wstring vertexShaderFileName = m_usingVprtShaders ? L"ms-appx:///PlaneVprtVertexShader.cso" : L"ms-appx:///PlaneVertexShader.cso";

	// Load shaders asynchronously.
	task<std::vector<byte>> loadVSTask = DX::ReadDataAsync(vertexShaderFileName);
	task<std::vector<byte>> loadPSTask = DX::ReadDataAsync(L"ms-appx:///PlanePixelShader.cso");
	task<std::vector<byte>> loadGSTask;
	if (!m_usingVprtShaders) {
		// Load the pass-through geometry shader.
		loadGSTask = DX::ReadDataAsync(L"ms-appx:///PlaneGeometryShader.cso");
	}

	// After the vertex shader file is loaded, create the shader and input layout.
	auto createVSTask = loadVSTask.then([this](const std::vector<byte>& fileData) {
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateVertexShader(&fileData[0], fileData.size(), 
			nullptr, &m_vertexShader));

		static const D3D11_INPUT_ELEMENT_DESC vertexDesc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateInputLayout(vertexDesc, ARRAYSIZE(vertexDesc),
				&fileData[0], fileData.size(), &m_inputLayout));
	});

	// After the pixel shader file is loaded, create the shader and constant buffer.
	auto createPSTask = loadPSTask.then([this](const std::vector<byte>& fileData) {
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreatePixelShader(&fileData[0], fileData.size(),
				nullptr, &m_pixelShader));
	});

	task<void> createGSTask;
	if (!m_usingVprtShaders) {
		// After the pass-through geometry shader file is loaded, create the shader.
		createGSTask = loadGSTask.then([this](const std::vector<byte>& fileData)
		{
			DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateGeometryShader(&fileData[0],	fileData.size(),
					nullptr, &m_geometryShader));
		});
	}

	// Once all shaders are loaded, create the mesh.
	task<void> shaderTaskGroup = m_usingVprtShaders ? (createPSTask && createVSTask) : (createPSTask && createVSTask && createGSTask);

	// Once the cube is loaded, the object is ready to be rendered.
	auto finishLoadingTask = shaderTaskGroup.then([this]() {
		// Create a default rasterizer state descriptor.
		D3D11_RASTERIZER_DESC rasterizerDesc = CD3D11_RASTERIZER_DESC(D3D11_DEFAULT);

		// Create the default rasterizer state.
		m_deviceResources->GetD3DDevice()->CreateRasterizerState(&rasterizerDesc, m_defaultRasterizerState.GetAddressOf());

		m_loadingComplete = true;
	});
}

void SurfacePlaneRenderer::ReleaseDeviceDependentResources() {
	m_constantBuffer.Reset();
	m_vertexBuffer.Reset();

	m_createdConstantBuffer = false;
}

void SurfacePlaneRenderer::UpdatePlanes(vector<BoundedPlane> newList) {
	// clear the old list and copy the new list.
	m_planeList.clear();
	m_planeList = newList;

	m_vertexBuffer.Reset();
	CreateVertexResources();
}