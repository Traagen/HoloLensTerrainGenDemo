#include "pch.h"
#include "SurfacePlaneRenderer.h"
#include "Common\DirectXHelper.h"
#include "Common\StepTimer.h"

using namespace HoloLensTerrainGenDemo;
using namespace DirectX;
using namespace Windows::Perception::Spatial;

SurfacePlaneRenderer::SurfacePlaneRenderer() {
}

SurfacePlaneRenderer::~SurfacePlaneRenderer() {
}

void SurfacePlaneRenderer::CreateVertexResources(ID3D11Device* device) {
	if (m_planeList.size() < 1) {
		// No planes to draw.
		return;
	}

	// Surface mesh resources are created off-thread, so that they don't affect rendering latency.'
	auto taskOptions = Concurrency::task_options();
	auto task = concurrency::create_task([this, device]() {
		// Build a vertex buffer containing 4 vertices for each plane, representing the quad of the bounded plane.
		int numPlanes = m_planeList.size();
		int numVerts = numPlanes * 4;
		XMFLOAT3 *verts = new XMFLOAT3[numVerts];

		for (auto p : m_planeList) {
			// for each plane in our list, build a quad and add the vertices to our verts list.
		}

		// create the vertex buffer.
		CD3D11_BUFFER_DESC descBuffer(sizeof(verts), D3D11_USAGE_DEFAULT);
		D3D11_SUBRESOURCE_DATA dataBuffer;
		dataBuffer.pSysMem = verts;
		DX::ThrowIfFailed(device->CreateBuffer(&descBuffer, &dataBuffer, &m_vertexBuffer));

		// clean up.
		delete verts;
	});
}

void SurfacePlaneRenderer::CreateDeviceDependentResources(ID3D11Device* device) {
	// Create a constant buffer for the plane data.
	CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
	DX::ThrowIfFailed(device->CreateBuffer(&constantBufferDesc, nullptr, &m_constantBuffer));

	m_createdConstantBuffer = true;
}


void SurfacePlaneRenderer::ReleaseDeviceDependentResources() {
	m_constantBuffer.Reset();
	m_vertexBuffer.Reset();

	m_createdConstantBuffer = false;
}
