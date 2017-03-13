#include "pch.h"
#include "Terrain.h"
#include "Common\DirectXHelper.h"
#include <stdlib.h>

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
		m_heightmap[i] = 0.0f;
	}

//	srand(23412342);

/*	for (int i = 0; i < 100; ++i) {
		IterateFaultFormation(5, 0.002f);
	}
	for (int i = 0; i < 20; ++i) {
		FIRFilter(0.2f);
	}*/
}

void Terrain::ResetHeightMap() {
	unsigned int h = m_hHeightmap * m_resHeightmap + 1;
	unsigned int w = m_wHeightmap * m_resHeightmap + 1;

	for (auto i = 0u; i < h * w; ++i) {
		m_heightmap[i] = 0.0f;
	}

	m_iIter = 0;
}

// Basic Fault Formation Algorithm
// FIR erosion filter
void Terrain::FIRFilter(float filter) {
	unsigned int h = m_hHeightmap * m_resHeightmap + 1;
	unsigned int w = m_wHeightmap * m_resHeightmap + 1;
	float prev;

	for (int y = 1; y < h - 1; ++y) {
		prev = m_heightmap[y * w];
		for (int x = 1; x < w - 1; ++x) {
			prev = m_heightmap[x + y * w] = filter * prev + (1 - filter) * m_heightmap[x + y * w];
		}

		prev = m_heightmap[(w - 1) + y * w];
		for (int x = w - 2; x >= 1; --x) {
			prev = m_heightmap[x + y * w] = filter * prev + (1 - filter) * m_heightmap[x + y * w];
		}
	}

	for (int x = 1; x < w - 1; ++x) {
		prev = m_heightmap[x];
		for (int y = 1; y < h - 1; ++y) {
			prev = m_heightmap[x + y * w] = filter * prev + (1 - filter) * m_heightmap[x + y * w];
		}

		prev = m_heightmap[x + w * (h - 1)];
		for (int y = h - 2; y >= 1; --y) {
			prev = m_heightmap[x + y * w] = filter * prev + (1 - filter) * m_heightmap[x + y * w];
		}
	}
}

void Terrain::IterateFaultFormation(unsigned int treeDepth, float treeAmplitude) {
	BSPNode root;
	BuildBSPTree(&root, treeDepth);

	// for each point in the height map, walk the BSP Tree to determine height of the point.
	// Don't run on the edges
	for (unsigned int y = 1; y < m_hHeightmap * m_resHeightmap; ++y) {
		for (unsigned int x = 1; x < m_wHeightmap * m_resHeightmap; ++x) {
			BSPNode* current = &root;
			float amp = treeAmplitude;
			float h = 0;

			for (unsigned int d = 1; d <= treeDepth; ++d) {
				auto start = current->GetStartPos();
				auto end = current->GetEndPos();
				float dx = end.x - start.x;
				float dy = end.y - start.y;
				float ddx = x - start.x;
				float ddy = y - start.y;

				if (ddx * dy - dx * ddy > 0) {
					current = current->GetRightChild();
					h += amp;
				} else {
					current = current->GetLeftChild();
					h -= amp;
				}

				amp /= 2.0f;
			}

			// Use F to attenuate the amplitude of the fault by the distance from the edge.
			// F = 0 on the edge. F = 1 in the exact center of the height map.
			float F = CalcManhattanDistFromCenter({ (float)x, (float)y });
			m_heightmap[y * (m_wHeightmap * m_resHeightmap + 1) + x] += h * F;
			// ensure that the height value never drops below zero since that
			// would put it beneath a surface in the real world.
			if (m_heightmap[y * (m_wHeightmap * m_resHeightmap + 1) + x] < 0) m_heightmap[y * (m_wHeightmap * m_resHeightmap + 1) + x] = 0;
		}
	}
}

// Calculates a distance value for point p from the edge of the height map.
// Calculation is calculated as Dx * Dy
// Dx = 1 - (|w/2 - px| / (w/2))
// Dy = 1 - (|h/2 - py| / (h/2))
float Terrain::CalcManhattanDistFromCenter(Windows::Foundation::Numerics::float2 p) {
	unsigned int h = m_hHeightmap * m_resHeightmap + 1;
	unsigned int w = m_wHeightmap * m_resHeightmap + 1;
	float h2 = (float)h / 2.0f;
	float w2 = (float)w / 2.0f;
	float Dx = 1 - (abs(w2 - p.x) / w2);
	float Dy = 1 - (abs(h2 - p.y) / h2);

	return min(Dx * Dy * 4.0f, 1.0f);
}

// Recursively generate a BSP Tree of specified depth for use in Fault Formation algorithm.
// depth of 1 is a leaf node.
/*void Terrain::BuildBSPTree(BSPNode* current, unsigned int depth) {
	unsigned int h = m_hHeightmap * m_resHeightmap + 1;
	unsigned int w = m_wHeightmap * m_resHeightmap + 1;
	std::uniform_int_distribution<int> distX(0, w);
	std::uniform_int_distribution<int> distY(0, h);
	
	current->SetStartPos(distX(generator), distY(generator));
	current->SetEndPos(distX(generator), distY(generator));
	//current->SetStartPos(rand() % w, rand() % h);
	//current->SetEndPos(rand() % w, rand() % h);

	if (depth <= 1) return;

	BuildBSPTree(current->CreateLeftChild(), depth - 1);
	BuildBSPTree(current->CreateRightChild(), depth - 1);
}*/

// Perform intersection test between two line segments
// take in 4 points. Make p1 and p2 the current line segment. Make p3 and p4 the one to intersect against.
// Return A: The point of intersection. Pass by reference.
// return j: j = ua. How far along the current line the intersection happens. Pass by reference.
// return k: k = ub. How far along the intersecting line the intersection happens. Pass by reference.
// return boolean value. True if intersection happens, false if it doesn't.
bool Intersect(float px1, float py1, float px2, float py2, float px3, float py3, float px4, float py4, 
	float &ax, float &ay, float &j, float &k) {
	float denom = (py4 - py3) * (px2 - px1) - (px4 - px3) * (py2 - py1);

	if (denom == 0) { // then the two lines are parallel and will never intersect.
		ax = ay = j = k = 0;
		return false;
	}

	float ua = ((px4 - px3) * (py1 - py3) - (py4 - py3) * (px1 - px3)) / denom;
	float ub = ((px2 - px1) * (py1 - py3) - (py2 - py1) * (px1 - px3)) / denom;
	j = ua;
	k = ub;

	ax = px1 + ua * (px2 - px1);
	ay = py1 + ua * (py2 - py1);

	return true;
}

void Terrain::BuildBSPTree(BSPNode *current, unsigned int depth) {
	unsigned int h = m_hHeightmap * m_resHeightmap + 1;
	unsigned int w = m_wHeightmap * m_resHeightmap + 1;
	std::uniform_int_distribution<int> distX(0, w);
	std::uniform_int_distribution<int> distY(0, h);
	std::uniform_real_distribution<float> distL(0.0f, 1.0f);
	std::uniform_real_distribution<float> distT(-45.0f, 45.0f);

	BSPNode* parent = current->GetParent();
	if (parent) {
		auto start = parent->GetStartPos();
		auto end = parent->GetEndPos();
		float m = (end.y - start.y) / (end.x - start.x); // find the slope of the parent line.
		float b = start.y - m * start.x; // find b for the equation y = mx + b by solving for b using the start point b = y - mx.
		float dx = end.x - start.x;
		float qdx = dx / 4;
		float hdx = dx / 2;

		// find random start point along parent line, somewhere between 0.25 and 0.75 along the segment.
		float x = start.x + qdx + distL(generator) * hdx;
		float y = m * x + b;
		current->SetStartPos(x, y);

		// find random direction off of point and look for end point. Line will either intersect with the border of the terrain, or it will intersect with an ancestor.
		// we want the random direction off of our child's start point to be somewhere between 45 and 135 degrees from the parent line.
		// we can find the perpendicular left vector as our direction (e - s) expressed as (x, y) inverted to (-y, x)
		// we can find the perpendicular right vector as our direction (e - s) expressed as (x, y) inverted to (y, -x)
		float lx, ly;
		// figure out if this is the left or right child.
		if (current == parent->GetLeftChild()) {
			lx = -1 * (end.y - start.y);
			ly = end.x - start.x;
		}
		else {
			lx = end.y - start.y;
			ly = -1 * (end.x - start.x);
		}

		// Now randomly choose a value between -45 degrees and 45 degrees (expressed in radians).
		float theta = DirectX::XMConvertToRadians(distT(generator));
		// Now rotate our perpendicular direction vector by theta
		float llx = lx * cos(theta) - ly * sin(theta);
		float lly = lx * sin(theta) + ly * cos(theta);

		// Now find the end point by intersecting first with the borders of the map and then with ancestors.
		float x2 = x + llx;
		float y2 = y + lly;
		// first the bottom border (0,0) to (MAPSIZE - 1, 0). Since this is the first, if it intersects at all with ua > 0, A will become
		// our new end point to test against.
		float x3 = 0;
		float y3 = 0;
		float x4 = w - 1;
		float y4 = 0;
		float ua, ub, ax, ay;
		bool in = Intersect(x, y, x2, y2, x3, y3, x4, y4, ax, ay, ua, ub);
		if (in && (ua > 0)) { // we don't want to use it if in is false
			x2 = ax;
			y2 = ay;
		}
		// test against right border (MAPSIZE - 1, 0) to (MAPSIZE - 1, MAPSIZE - 1). Be more picky. 0 < ua < 1 and 0 < ub < 1.
		x3 = x4;
		y3 = y4;
		y4 = w - 1;
		in = Intersect(x, y, x2, y2, x3, y3, x4, y4, ax, ay, ua, ub);
		if (in && (ua > 0) && (ua < 1) && (ub > 0) && (ub < 1)) {
			x2 = ax;
			y2 = ay;
		}
		// test against top border (MAPSIZE - 1, MAPSIZE - 1) to (0, MAPSIZE - 1).
		y3 = y4;
		x4 = 0;
		in = Intersect(x, y, x2, y2, x3, y3, x4, y4, ax, ay, ua, ub);
		if (in && (ua > 0) && (ua < 1) && (ub > 0) && (ub < 1)) {
			x2 = ax;
			y2 = ay;
		}
		// test against left border (0, MAPSIZE - 1) to (0, 0)
		x3 = x4;
		y4 = 0;
		in = Intersect(x, y, x2, y2, x3, y3, x4, y4, ax, ay, ua, ub);
		if (in && (ua > 0) && (ua < 1) && (ub > 0) && (ub < 1)) {
			x2 = ax;
			y2 = ay;
		}
		// now that we know where the line segment intersects the border of the map, we need to see if it intersects an ancestor before that happens.
		// It cannot intersect its immediate parent anywhere but at the start point which we already have so ignore the parent and move on to the
		// grandparent.
		parent = parent->GetParent();
		while (parent) { // This will kick out once the ancestor is NULL
			auto p3 = parent->GetStartPos();
			auto p4 = parent->GetEndPos();
			in = Intersect(x, y, x2, y2, p3.x, p3.y, p4.x, p4.y, ax, ay, ua, ub);
			if (in && (ua > 0) && (ua < 1) && (ub > 0) && (ub < 1)) {
				x2 = ax;
				y2 = ay;
			}

			parent = parent->GetParent();
		}
		current->SetEndPos(x2, y2);
	} else {
		// if this is the root node, we just randomly set the initial divider.
		current->SetStartPos(distX(generator), distY(generator));
		current->SetEndPos(distX(generator), distY(generator));
	}

	if (depth <= 1) return;
	
	BuildBSPTree(current->CreateLeftChild(), depth - 1);
	BuildBSPTree(current->CreateRightChild(), depth - 1);
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
	// Loading is asynchronous. Resources must be created before drawing can occur.
	if (!m_loadingComplete) {
		return;
	}

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

	// Update the terrain generator.
	if (m_iIter < 500) {
		IterateFaultFormation(5, 0.002f);
		FIRFilter(0.1f);

		D3D11_MAPPED_SUBRESOURCE mappedTex = { 0 };
		DX::ThrowIfFailed(context->Map(m_hmTexture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedTex));
		// Texture data on GPU may have padding added to each row so we need to
		// take that padding into account when we upload the data.
		unsigned int rowSpan = (m_wHeightmap * m_resHeightmap + 1) * sizeof(float);
		BYTE* mappedData = reinterpret_cast<BYTE*>(mappedTex.pData);
		BYTE* buffer = reinterpret_cast<BYTE*>(m_heightmap);
		for (unsigned int i = 0; i < (m_hHeightmap * m_resHeightmap + 1); ++i) {
			memcpy(mappedData, buffer, rowSpan);
			mappedData += mappedTex.RowPitch;
			buffer += rowSpan;
		}

		context->Unmap(m_hmTexture.Get(), 0);
	} 
	m_iIter = m_iIter < 500 ? m_iIter + 1 : m_iIter;
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

