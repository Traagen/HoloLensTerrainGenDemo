#include "pch.h"
#include "Terrain.h"
#include "Common\DirectXHelper.h"

using namespace HoloLensTerrainGenDemo;
using namespace Concurrency;
using namespace DirectX;
using namespace Windows::Foundation::Numerics;
using namespace Windows::UI::Input::Spatial;

// provide h and w in centimeters.
// res is number of triangle edges per centimeter.
Terrain::Terrain(const std::shared_ptr<DX::DeviceResources>& deviceResources, float h, float w, unsigned int res) :
	m_deviceResources(deviceResources), m_wHeightmap(unsigned int(w * 100)), m_hHeightmap(unsigned int(h * 100)), m_resHeightmap(res) {
	m_heightmap = nullptr;
	InitializeHeightmap();

	SetPosition(float3(-w / 2.0f, -0.2f, h * -2.0f));

	CreateDeviceDependentResources();
}

Terrain::~Terrain() {
	if (m_heightmap) {
		delete[] m_heightmap;
	}
}

// initializes the height map to the supplied dimensions.
void Terrain::InitializeHeightmap() {
	unsigned int h = m_hHeightmap * m_resHeightmap + 1;
	unsigned int w = m_wHeightmap * m_resHeightmap + 1;
	m_heightmap = new float[h * w];

	for (auto i = 0u; i < h * w; ++i) {
		m_heightmap[i] = i < h * w / 2? 0.1f : 0.0f;
	}
}

// This function uses a SpatialPointerPose to position the world-locked hologram
// two meters in front of the user's heading.
void Terrain::PositionHologram(SpatialPointerPose^ pointerPose) {
/*	if (pointerPose != nullptr)	{
		// Get the gaze direction relative to the given coordinate system.
		const float3 headPosition = pointerPose->Head->Position;
		const float3 headDirection = pointerPose->Head->ForwardDirection;

		// The hologram is positioned two meters along the user's gaze direction.
		constexpr float distanceFromUser = 0.1f; // meters
		const float3 gazeAtTwoMeters = headPosition + (distanceFromUser * headDirection);

		// This will be used as the translation component of the hologram's
		// model transform.
		SetPosition(gazeAtTwoMeters);
	}*/

	SetPosition(float3(0.0f, 0.0f, 0.0f));
}

// Called once per frame. Calculates and sets the model matrix
// relative to the position transform indicated by hologramPositionTransform.
void Terrain::Update(const DX::StepTimer& timer) {
	// Position the terrain.
	const XMMATRIX modelTranslation = XMMatrixTranslationFromVector(XMLoadFloat3(&m_position));

	// The view and projection matrices are provided by the system; they are associated
	// with holographic cameras, and updated on a per-camera basis.
	// Here, we provide the model transform for the sample hologram. The model transform
	// matrix is transposed to prepare it for the shader.
	XMStoreFloat4x4(&m_modelConstantBufferData.model, XMMatrixTranspose(modelTranslation));

	// Loading is asynchronous. Resources must be created before they can be updated.
	if (!m_loadingComplete)	{
		return;
	}

	// Use the D3D device context to update Direct3D device-based resources.
	const auto context = m_deviceResources->GetD3DDeviceContext();

	// Update the model transform buffer for the hologram.
	context->UpdateSubresource(
		m_modelConstantBuffer.Get(),
		0,
		nullptr,
		&m_modelConstantBufferData,
		0,
		0
	);
}

// Renders one frame using the vertex and pixel shaders.
// On devices that do not support the D3D11_FEATURE_D3D11_OPTIONS3::
// VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature,
// a pass-through geometry shader is also used to set the render 
// target array index.
void Terrain::Render() {
	// Loading is asynchronous. Resources must be created before drawing can occur.
	if (!m_loadingComplete)	{
		return;
	}

	const auto context = m_deviceResources->GetD3DDeviceContext();

	// Each vertex is one instance of the Vertex struct.
	const UINT stride = sizeof(Vertex);
	const UINT offset = 0;
	context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->IASetInputLayout(m_inputLayout.Get());

	// Attach the vertex shader.
	context->VSSetShader(m_vertexShader.Get(),	nullptr, 0);
	// Apply the model constant buffer to the vertex shader.
	context->VSSetConstantBuffers(0, 1,	m_modelConstantBuffer.GetAddressOf());
	// attach the heightmap
	context->VSSetShaderResources(0, 1, m_hmSRV.GetAddressOf());

	if (!m_usingVprtShaders) {
		// On devices that do not support the D3D11_FEATURE_D3D11_OPTIONS3::
		// VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature,
		// a pass-through geometry shader is used to set the render target 
		// array index.
		context->GSSetShader(m_geometryShader.Get(), nullptr, 0);
	}

	// Attach the pixel shader.
	context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
	// attach the heightmap
	context->PSSetShaderResources(0, 1, m_hmSRV.GetAddressOf());

	// Draw the objects.
	context->DrawIndexedInstanced(m_indexCount,	2, 0, 0, 0);
}

void Terrain::CreateDeviceDependentResources() {
	m_usingVprtShaders = m_deviceResources->GetDeviceSupportsVprt();

	// On devices that do support the D3D11_FEATURE_D3D11_OPTIONS3::
	// VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature
	// we can avoid using a pass-through geometry shader to set the render
	// target array index, thus avoiding any overhead that would be 
	// incurred by setting the geometry shader stage.
	std::wstring vertexShaderFileName = m_usingVprtShaders ? L"ms-appx:///VprtVertexShader.cso" : L"ms-appx:///VertexShader.cso";

	// Load shaders asynchronously.
	task<std::vector<byte>> loadVSTask = DX::ReadDataAsync(vertexShaderFileName);
	task<std::vector<byte>> loadPSTask = DX::ReadDataAsync(L"ms-appx:///PixelShader.cso");

	task<std::vector<byte>> loadGSTask;
	if (!m_usingVprtShaders) {
		// Load the pass-through geometry shader.
		loadGSTask = DX::ReadDataAsync(L"ms-appx:///GeometryShader.cso");
	}

	// After the vertex shader file is loaded, create the shader and input layout.
	task<void> createVSTask = loadVSTask.then([this](const std::vector<byte>& fileData) {
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateVertexShader(fileData.data(), fileData.size(), nullptr, &m_vertexShader));

		constexpr std::array<D3D11_INPUT_ELEMENT_DESC, 2> vertexDesc =
		{ {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			} };

		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateInputLayout(vertexDesc.data(), vertexDesc.size(), fileData.data(), fileData.size(), &m_inputLayout));
	});

	// After the pixel shader file is loaded, create the shader and constant buffer.
	task<void> createPSTask = loadPSTask.then([this](const std::vector<byte>& fileData) {
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreatePixelShader(fileData.data(), fileData.size(), nullptr, &m_pixelShader));

		const CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&constantBufferDesc, nullptr,	&m_modelConstantBuffer));
	});

	task<void> createGSTask;
	if (!m_usingVprtShaders) {
		// After the pass-through geometry shader file is loaded, create the shader.
		createGSTask = loadGSTask.then([this](const std::vector<byte>& fileData) {
			DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateGeometryShader(fileData.data(), fileData.size(), nullptr, &m_geometryShader));
		});
	}

	// Once all shaders are loaded, create the mesh.
	task<void> shaderTaskGroup = m_usingVprtShaders ? (createPSTask && createVSTask) : (createPSTask && createVSTask && createGSTask);
	task<void> createMeshTask = shaderTaskGroup.then([this]() {
		// Load mesh vertices. Each vertex has a position and a color.
		auto h = m_hHeightmap * m_resHeightmap + 1;
		auto w = m_wHeightmap * m_resHeightmap + 1;
		float d = 100.0f * m_resHeightmap;
		std::vector<Vertex> terrainVertices;
		for (auto i = 0u; i < h; ++i) {
			float y = (float)i / d;
			for (auto j = 0u; j < w; ++j) {
				float x = (float)j / d;
				terrainVertices.push_back({XMFLOAT3(x, 0.0f, y), XMFLOAT2((float)j / (float)w, (float)i / (float)h)});
			}
		}

		D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
		vertexBufferData.pSysMem = terrainVertices.data();
		vertexBufferData.SysMemPitch = 0;
		vertexBufferData.SysMemSlicePitch = 0;
		const CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(Vertex) * terrainVertices.size(), D3D11_BIND_VERTEX_BUFFER);
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &m_vertexBuffer));

		// Load mesh indices. Each trio of indices represents
		// a triangle to be rendered on the screen.
		std::vector<unsigned short> terrainIndices;
		for (auto i = 0u; i < h - 1; ++i) {
			for (auto j = 0u; j < w - 1; ++j) {
				auto index = j + (i * h);

				terrainIndices.push_back(index);
				terrainIndices.push_back(index + h + 1);
				terrainIndices.push_back(index + h);

				terrainIndices.push_back(index);
				terrainIndices.push_back(index + 1);
				terrainIndices.push_back(index + h + 1);
			}
		}

		m_indexCount = terrainIndices.size();

		D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
		indexBufferData.pSysMem = terrainIndices.data();
		indexBufferData.SysMemPitch = 0;
		indexBufferData.SysMemSlicePitch = 0;
		CD3D11_BUFFER_DESC indexBufferDesc(sizeof(unsigned short) * m_indexCount, D3D11_BIND_INDEX_BUFFER);
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&indexBufferDesc, &indexBufferData, &m_indexBuffer));
	});

	// we need to create a texture and shader resource view for the height map.
	task<void> createHeightmapTextureTask = createMeshTask.then([this]() {
		D3D11_TEXTURE2D_DESC descTex = { 0 };
		descTex.MipLevels = 1;
		descTex.ArraySize = 1;
		descTex.Width = m_wHeightmap * m_resHeightmap + 1;
		descTex.Height = m_hHeightmap * m_resHeightmap + 1;
		descTex.Format = DXGI_FORMAT_R32_FLOAT;
		descTex.SampleDesc.Count = 1;
		descTex.SampleDesc.Quality = 0;
		descTex.Usage = D3D11_USAGE_DYNAMIC;
		descTex.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		descTex.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		D3D11_SUBRESOURCE_DATA dataTex = { 0 };
		dataTex.pSysMem = m_heightmap;
		dataTex.SysMemPitch = (m_wHeightmap * m_resHeightmap + 1) * sizeof(float);
		dataTex.SysMemSlicePitch = (m_hHeightmap * m_resHeightmap + 1) * (m_wHeightmap * m_resHeightmap + 1) * sizeof(float);
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateTexture2D(&descTex, &dataTex, &m_hmTexture));

		D3D11_SHADER_RESOURCE_VIEW_DESC descSRV = {};
		descSRV.Texture2D.MipLevels = descTex.MipLevels;
		descSRV.Texture2D.MostDetailedMip = 0;
		descSRV.Format = descTex.Format;
		descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateShaderResourceView(m_hmTexture.Get(), &descSRV, &m_hmSRV));
	});

	// Once the cube is loaded, the object is ready to be rendered.
	createHeightmapTextureTask.then([this]() { m_loadingComplete = true; });
}

void Terrain::ReleaseDeviceDependentResources() {
	m_loadingComplete = false;
	m_usingVprtShaders = false;
	m_vertexShader.Reset();
	m_inputLayout.Reset();
	m_pixelShader.Reset();
	m_geometryShader.Reset();
	m_modelConstantBuffer.Reset();
	m_vertexBuffer.Reset();
	m_indexBuffer.Reset();
	m_hmTexture.Reset();
	m_hmSRV.Reset();
}

