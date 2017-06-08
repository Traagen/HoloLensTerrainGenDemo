#include "pch.h"
#include "SurfacePlaneRenderer.h"
#include "Common\DirectXHelper.h"
#include "Common\StepTimer.h"
#include "Common\MathFunctions.h"

using namespace HoloLensTerrainGenDemo;
using namespace DirectX;
using namespace Concurrency;
using namespace Windows::Perception::Spatial;
using namespace Windows::UI::Input::Spatial;
using namespace PlaneFinding;
using namespace std::placeholders;

SurfacePlaneRenderer::SurfacePlaneRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	m_deviceResources(deviceResources) {
	m_planeList.clear();

	CreateDeviceDependentResources();

	// Set up a general gesture recognizer for input.
	m_gestureRecognizer = ref new SpatialGestureRecognizer(SpatialGestureSettings::Tap);

	m_tapGestureEventToken =
		m_gestureRecognizer->Tapped +=
		ref new Windows::Foundation::TypedEventHandler<SpatialGestureRecognizer^, SpatialTappedEventArgs^>(
			std::bind(&SurfacePlaneRenderer::OnTap, this, _1, _2)
			);
}

SurfacePlaneRenderer::~SurfacePlaneRenderer() {
	ReleaseDeviceDependentResources();

	if (m_gestureRecognizer) {
		m_gestureRecognizer->Tapped -= m_tapGestureEventToken;
	}
}

void SurfacePlaneRenderer::CreateVertexResources() {
	if (m_planeList.size() < 1) {
		// No planes to draw.
		return;
	}

	// define the unit space vertices of the plane we are rendering.
	static const XMVECTOR verts[6] = {
		{ -1.0f, -1.0f, 0.0f, 0.0f },
		{ -1.0f,  1.0f, 0.0f, 0.0f },
		{ 1.0f, -1.0f, 0.0f, 0.0f },
		{ -1.0f,  1.0f, 0.0f, 0.0f },
		{ 1.0f,  1.0f, 0.0f, 0.0f },
		{ 1.0f, -1.0f, 0.0f, 0.0f }
	};

	// resources are created off-thread, so that they don't affect rendering latency.
	auto taskOptions = Concurrency::task_options();
	auto task = concurrency::create_task([this]() {
		// lock the vertex buffer down until we are done rebuilding it.
		std::lock_guard<std::mutex> guard(m_planeListLock);

		// reset the existing vertex buffer.
		m_vertexBuffer.Reset();

		// Build a vertex buffer containing 6 vertices (2 triangles) for each plane, representing the quad of the bounded plane.
		std::vector<XMFLOAT3> vertexList;

		for (auto p : m_planeList) {
			// for each plane in our list, build a quad and add the vertices to our verts list.
			// Our plane is defined as being centered in the bounding box,
			// with the z axis always being the thinnest axis.
			auto center = p.bounds.Center;
			auto extents = p.bounds.Extents;

			// transformation matrices to go from unit space to object space.
			XMMATRIX world = XMMatrixRotationQuaternion(XMLoadFloat4(&p.bounds.Orientation));
			XMMATRIX scale = XMMatrixScaling(extents.x, extents.y, extents.z);
			XMMATRIX translate = XMMatrixTranslation(center.x, center.y, center.z);
			XMMATRIX transform = XMMatrixMultiply(scale, world);
			transform = XMMatrixMultiply(transform, translate);

			for (auto i = 0; i < 6; ++i) {
				XMVECTOR v = XMVector3Transform(verts[i], transform);
				XMFLOAT3 vec;
				XMStoreFloat3(&vec, v);
				vertexList.push_back(vec);
			}
		}

		// create the vertex buffer.
		CD3D11_BUFFER_DESC descBuffer(sizeof(XMFLOAT3) * vertexList.size(), D3D11_BIND_VERTEX_BUFFER);
		D3D11_SUBRESOURCE_DATA dataBuffer;
		dataBuffer.pSysMem = vertexList.data();
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&descBuffer, &dataBuffer, &m_vertexBuffer));
	});
}

void SurfacePlaneRenderer::CreateDeviceDependentResources() {
	// Create a constant buffer for the plane data.
	CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
	DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&constantBufferDesc, nullptr, &m_constantBuffer));

	CreateShaders();
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
		rasterizerDesc.CullMode = D3D11_CULL_NONE;

		// Create the default rasterizer state.
		m_deviceResources->GetD3DDevice()->CreateRasterizerState(&rasterizerDesc, m_defaultRasterizerState.GetAddressOf());

		m_shadersReady = true;
	});
}

void SurfacePlaneRenderer::ReleaseDeviceDependentResources() {
	m_planeList.clear();
	m_constantBuffer.Reset();
	m_vertexBuffer.Reset();
	m_vertexShader.Reset();
	m_geometryShader.Reset();
	m_pixelShader.Reset();
}

void SurfacePlaneRenderer::UpdatePlanes(vector<BoundedPlane> newList, Windows::Perception::Spatial::SpatialCoordinateSystem^ cs) {
	// clear the old list and copy the new list.
	m_planeList.clear();

	for (auto p : newList) {
		// for each plane in our list, check if it
		// is a PlaneFinding::FLOOR. This means it has a
		// normal pointing directly up. These are the only
		// planes we want.
		if (p.plane.surface == PlaneFinding::FLOOR) {
			m_planeList.push_back(p);
		}
	}

	// Update the coordinate system
	m_coordinateSystem = cs;

	CreateVertexResources();
}

void SurfacePlaneRenderer::Render() {
	if (m_planeList.size() < 1 || !m_shadersReady) {
		// nothing to render.
		return;
	}

	auto context = m_deviceResources->GetD3DDeviceContext();

	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->IASetInputLayout(m_inputLayout.Get());

	// Attach our vertex shader.
	context->VSSetShader(m_vertexShader.Get(), nullptr,	0);

	// The constant buffer is per-mesh, and will be set for each one individually.
	if (!m_usingVprtShaders) {
		// Attach the passthrough geometry shader.
		context->GSSetShader(m_geometryShader.Get(), nullptr, 0);
	}

	// Use the default rasterizer state.
	m_deviceResources->GetD3DDeviceContext()->RSSetState(m_defaultRasterizerState.Get());

	// Attach a pixel shader that can do lighting.
	context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

	// Apply the model constant buffer to the vertex shader.
	context->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());

	// Set layout.
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->IASetInputLayout(m_inputLayout.Get());
		
	{
		std::lock_guard<std::mutex> guard(m_planeListLock);

		// draw the meshes.
		// Each vertex is one instance of the Vertex struct.
		const UINT stride = sizeof(XMFLOAT3);
		const UINT offset = 0;
		context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);

		// Draw the objects.
		context->DrawInstanced(m_planeList.size() * 6, 2, 0, 0);
	}
}

void SurfacePlaneRenderer::Update(SpatialCoordinateSystem^ baseCoordinateSystem) {
	if (m_planeList.size() < 1) {
		return;
	}

	// Transform to the correct coordinate system from our anchor's coordinate system.
	auto tryTransform = m_coordinateSystem->TryGetTransformTo(baseCoordinateSystem);
	XMMATRIX transform;
	if (tryTransform) {
		// If the transform can be acquired, this spatial mesh is valid right now and
		// we have the information we need to draw it this frame.
		transform = XMLoadFloat4x4(&tryTransform->Value);
	}
	else {
		// just use the identity matrix if we can't load the transform for some reason.
		transform = XMMatrixIdentity();
	}

	XMStoreFloat4x4(&m_constantBufferData.modelToWorld, XMMatrixTranspose(transform));
	XMStoreFloat4x4(&m_modelToWorld, transform);

	// Use the D3D device context to update Direct3D device-based resources.
	const auto context = m_deviceResources->GetD3DDeviceContext();

	// Update the model transform buffer for the hologram.
	context->UpdateSubresource(m_constantBuffer.Get(), 0, nullptr, &m_constantBufferData, 0, 0);
}

bool SurfacePlaneRenderer::CaptureInteraction(SpatialInteraction^ interaction) {
	if (m_planeList.size() < 1) {
		// No planes to intersect with.
		return false;
	}

	// define the unit space vertices of the plane we are testing.
	static const XMVECTOR verts[6] = {
		{ -1.0f, -1.0f, 0.0f, 0.0f },
		{ -1.0f,  1.0f, 0.0f, 0.0f },
		{ 1.0f, -1.0f, 0.0f, 0.0f },
		{ -1.0f,  1.0f, 0.0f, 0.0f },
		{ 1.0f,  1.0f, 0.0f, 0.0f },
		{ 1.0f, -1.0f, 0.0f, 0.0f }
	};

	// Get the user's gaze
	auto gaze = interaction->SourceState->TryGetPointerPose(m_coordinateSystem);
	auto head = gaze->Head;
	auto position = head->Position;
	auto look = head->ForwardDirection;

	// For each surface plane quad, check if the gaze intersects it.
	// track the closest such intersection.
	int index = -1; // index of closest intersected plane.
	float dist = D3D11_FLOAT32_MAX; // initial distance value.
	int i = 0;
	for (auto p : m_planeList) {
		// build the matrices to properly orient the quad.
		auto center = p.bounds.Center;
		auto extents = p.bounds.Extents;

		// transformation matrices to go from unit space to world space.
		XMMATRIX modelToWorld = XMLoadFloat4x4(&m_modelToWorld);
		XMMATRIX world = XMMatrixRotationQuaternion(XMLoadFloat4(&p.bounds.Orientation));
		world = XMMatrixMultiply(modelToWorld, world);
		XMMATRIX scale = XMMatrixScaling(extents.x, extents.y, extents.z);
		XMMATRIX translate = XMMatrixTranslation(center.x, center.y, center.z);
		XMMATRIX transform = XMMatrixMultiply(scale, world);
		transform = XMMatrixMultiply(transform, translate);
		// add in the model to world transform so that the quad and gaze are in the same coordinate system.


		// generate oriented quad.
		std::vector<XMFLOAT3> vertexList;
		for (int j = 0; j < 6; ++j) {
			XMVECTOR v = XMVector3Transform(verts[j], transform);
			XMFLOAT3 vec;
			XMStoreFloat3(&vec, v);
			vertexList.push_back(vec);
		}

		// perform intersection test.
		// if intersection test returns an intersection, check
		// that this is the closest intersection in case the gaze intersects multiple planes.
		// save the index of the closest plane so we can create the correct anchor onTap.

		// perform 2 ray-triangle intersection tests and save the lowest value above 0.
		float d = MathUtil::RayTriangleIntersect(position, look, 
			float3(vertexList[0].x, vertexList[0].y, vertexList[0].z), 
			float3(vertexList[1].x, vertexList[1].y, vertexList[1].z), 
			float3(vertexList[2].x, vertexList[2].y, vertexList[2].z));

		float d2 = MathUtil::RayTriangleIntersect(position, look,
			float3(vertexList[3].x, vertexList[3].y, vertexList[3].z),
			float3(vertexList[4].x, vertexList[4].y, vertexList[4].z),
			float3(vertexList[5].x, vertexList[5].y, vertexList[5].z));

		if (d > 0 && d < dist) {
			dist = d;
			index = i;
		}
		if (d2 > 0 && d2 < dist) {
			dist = d2;
			index = i;
		}
		
		// next plane
		++i;
	}

	// if we have an index > -1, then we need to handle this interaction.
	if (index > -1) {
		// if so, handle the interaction and return true.
		m_intersectedPlane = index;

		m_gestureRecognizer->CaptureInteraction(interaction);
		return true;
	}

	// if it doesn't intersect, then don't handle the interaction and return false.
	return false;
}

void SurfacePlaneRenderer::OnTap(SpatialGestureRecognizer^ sender, SpatialTappedEventArgs^ args) {
	m_WasTapped = true;
}

SpatialAnchor^ SurfacePlaneRenderer::GetAnchor() {
	auto plane = m_planeList[m_intersectedPlane];

	// build the matrices to properly orient the quad.
	auto center = plane.bounds.Center;
	auto extents = plane.bounds.Extents;

	// transformation matrices to go from unit space to world space.
	XMMATRIX modelToWorld = XMLoadFloat4x4(&m_modelToWorld);
	XMMATRIX world = XMMatrixRotationQuaternion(XMLoadFloat4(&plane.bounds.Orientation));
	world = XMMatrixMultiply(modelToWorld, world);
	XMMATRIX scale = XMMatrixScaling(extents.x, extents.y, extents.z);
	XMMATRIX translate = XMMatrixTranslation(center.x, center.y, center.z);
	XMMATRIX transform = XMMatrixMultiply(scale, world);
	transform = XMMatrixMultiply(transform, translate);

	XMVECTOR v = { 0.0f, 0.0f, -1.0f, 0.0f };
	v = XMVector3Transform(v, transform);
	XMFLOAT3 vec;
	XMStoreFloat3(&vec, v);
	return SpatialAnchor::TryCreateRelativeTo(m_coordinateSystem, float3(vec.x, vec.y, vec.z));
}

float2 SurfacePlaneRenderer::GetDimensions() {
	auto plane = m_planeList[m_intersectedPlane];

	// build the matrices to properly orient the quad.
	auto extents = plane.bounds.Extents;

	return float2(extents.x, extents.y);
}