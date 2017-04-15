//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "pch.h"

#include <ppltasks.h>

#include "Common\DirectXHelper.h"
#include "Common\StepTimer.h"
#include "GetDataFromIBuffer.h"
#include "SurfaceMesh.h"
#include <DirectXPackedVector.h>

using namespace HoloLensTerrainGenDemo;
using namespace DirectX;
using namespace Windows::Perception::Spatial;
using namespace Windows::Perception::Spatial::Surfaces;
using namespace Windows::Foundation::Numerics;
using namespace PlaneFinding;
using namespace DirectX::PackedVector;

SurfaceMesh::SurfaceMesh() {
	std::lock_guard<std::mutex> lock(m_meshResourcesMutex);

	ReleaseDeviceDependentResources();
	m_lastUpdateTime.UniversalTime = 0;

	// initialize m_localMesh
	m_localMesh.indices = nullptr;
	m_localMesh.normals = nullptr;
	m_localMesh.verts = nullptr;
	m_localMesh.transform = XMFloat4x4Identity;
	m_localMesh.indexCount = 0;
	m_localMesh.vertCount = 0;
}

SurfaceMesh::~SurfaceMesh() {
	std::lock_guard<std::mutex> lock(m_meshResourcesMutex);

	ReleaseDeviceDependentResources();

	ClearLocalMesh();
}

void SurfaceMesh::UpdateSurface(SpatialSurfaceMesh^ surfaceMesh) {
	m_surfaceMesh = surfaceMesh;
	m_updateNeeded = true;

	UpdateLocalMesh();

	// the snap to gravity threshold appears to be an angle in degrees from horizontal that we will snap to horizontal.
	// see planefinding/util.cpp for SnapToGravity() method where it is used.
	auto planeList = FindPlanes(1, &m_localMesh, 5.0f); 

	for (auto p : planeList) {
		auto c = p.bounds.Center;
		auto o = p.bounds.Orientation;
		auto e = p.bounds.Extents;
		auto a = p.area;
	}
}

void SurfaceMesh::UpdateDeviceBasedResources(ID3D11Device* device) {
	std::lock_guard<std::mutex> lock(m_meshResourcesMutex);

	ReleaseDeviceDependentResources();
	CreateDeviceDependentResources(device);
}

// Spatial Mapping surface meshes each have a transform. This transform is updated every frame.
void SurfaceMesh::UpdateTransform(ID3D11Device* device, ID3D11DeviceContext* context, DX::StepTimer const& timer,
	SpatialCoordinateSystem^ baseCoordinateSystem) {
	if (m_surfaceMesh == nullptr) {
		// Not yet ready.
		m_isActive = false;
	}

	if (m_updateNeeded) {
		CreateVertexResources(device);
		m_updateNeeded = false;
	} else {
		std::lock_guard<std::mutex> lock(m_meshResourcesMutex);

		if (m_updateReady) {
			// Surface mesh resources are created off-thread so that they don't affect rendering latency.
			// When a new update is ready, we should begin using the updated vertex position, normal, and 
			// index buffers.
			SwapVertexBuffers();
			m_updateReady = false;
		}
	}

	// If the surface is active this frame, we need to update its transform.
	XMMATRIX transform;
	if (m_isActive)	{
		// In this example, new surfaces are treated differently by highlighting them in a different
		// color. This allows you to observe changes in the spatial map that are due to new meshes,
		// as opposed to mesh updates.
		if (m_colorFadeTimeout > 0.f) {
			m_colorFadeTimer += static_cast<float>(timer.GetElapsedSeconds());
			if (m_colorFadeTimer < m_colorFadeTimeout) {
				float colorFadeFactor = min(1.f, m_colorFadeTimeout - m_colorFadeTimer);
				m_constantBufferData.colorFadeFactor = XMFLOAT4(colorFadeFactor, colorFadeFactor, colorFadeFactor, 1.f);
			} else {
				m_constantBufferData.colorFadeFactor = XMFLOAT4(0.f, 0.f, 0.f, 0.f);
				m_colorFadeTimer = m_colorFadeTimeout = -1.f;
			}
		}

		// The transform is updated relative to a SpatialCoordinateSystem. In the SurfaceMesh class, we
		// expect to be given the same SpatialCoordinateSystem that will be used to generate view
		// matrices, because this class uses the surface mesh for rendering.
		// Other applications could potentially involve using a SpatialCoordinateSystem from a stationary
		// reference frame that is being used for physics simulation, etc.
		auto tryTransform = m_surfaceMesh->CoordinateSystem->TryGetTransformTo(baseCoordinateSystem);
		if (tryTransform != nullptr) {
			// If the transform can be acquired, this spatial mesh is valid right now and
			// we have the information we need to draw it this frame.
			transform = XMLoadFloat4x4(&tryTransform->Value);

			// save the transform to the local MeshData object as well.
			XMStoreFloat4x4(&m_localMesh.transform, transform);

			m_lastActiveTime = static_cast<float>(timer.GetTotalSeconds());
		} else {
			// If the transform is not acquired, the spatial mesh is not valid right now
			// because its location cannot be correlated to the current space.
			m_isActive = false;
		}
	}

	if (!m_isActive) {
		// If for any reason the surface mesh is not active this frame - whether because
		// it was not included in the observer's collection, or because its transform was
		// not located - we don't have the information we need to update it.
		return;
	}

	// Set up a transform from surface mesh space, to world space.
	XMMATRIX scaleTransform = XMMatrixScalingFromVector(XMLoadFloat3(&m_surfaceMesh->VertexPositionScale));
	XMStoreFloat4x4(&m_constantBufferData.modelToWorld,	XMMatrixTranspose(scaleTransform * transform));

	// Surface meshes come with normals, which are also transformed from surface mesh space, to world space.
	XMMATRIX normalTransform = transform;
	// Normals are not translated, so we remove the translation component here.
	normalTransform.r[3] = XMVectorSet(0.f, 0.f, 0.f, XMVectorGetW(normalTransform.r[3]));
	XMStoreFloat4x4(&m_constantBufferData.normalToWorld, XMMatrixTranspose(normalTransform));

	if (!m_constantBufferCreated) {
		// If loading is not yet complete, we cannot actually update the graphics resources.
		// This return is intentionally placed after the surface mesh updates so that this
		// code may be copied and re-used for CPU-based processing of surface data.
		CreateDeviceDependentResources(device);
		return;
	}

	context->UpdateSubresource(m_modelTransformBuffer.Get(), 0,	NULL, &m_constantBufferData, 0, 0);
}

// Does an indexed, instanced draw call after setting the IA stage to use the mesh's geometry, and
// after setting up the constant buffer for the surface mesh.
// The caller is responsible for the rest of the shader pipeline.
void SurfaceMesh::Draw(ID3D11Device* device, ID3D11DeviceContext* context, bool usingVprtShaders, bool isStereo) {
	if (!m_constantBufferCreated || !m_loadingComplete)	{
		// Resources are still being initialized.
		return;
	}

	if (!m_isActive) {
		// Mesh is not active this frame, and should not be drawn.
		return;
	}

	// The vertices are provided in {vertex, normal} format
	UINT strides[] = { m_meshProperties.vertexStride, m_meshProperties.normalStride };
	UINT offsets[] = { 0, 0 };
	ID3D11Buffer* buffers[] = { m_vertexPositions.Get(), m_vertexNormals.Get() };

	context->IASetVertexBuffers(0, ARRAYSIZE(buffers), buffers, strides, offsets);
	context->IASetIndexBuffer(m_triangleIndices.Get(), m_meshProperties.indexFormat, 0);
	context->VSSetConstantBuffers(0, 1,	m_modelTransformBuffer.GetAddressOf());

	if (!usingVprtShaders) {
		context->GSSetConstantBuffers(0, 1,	m_modelTransformBuffer.GetAddressOf());
	}

	context->PSSetConstantBuffers(0, 1, m_modelTransformBuffer.GetAddressOf());
	context->DrawIndexedInstanced(m_meshProperties.indexCount, isStereo ? 2 : 1, 0, 0, 0);
}

void SurfaceMesh::CreateDirectXBuffer(ID3D11Device* device,	D3D11_BIND_FLAG binding, IBuffer^ buffer, ID3D11Buffer** target) {
	auto length = buffer->Length;

	CD3D11_BUFFER_DESC bufferDescription(buffer->Length, binding);
	D3D11_SUBRESOURCE_DATA bufferBytes = { GetDataFromIBuffer(buffer), 0, 0 };
	device->CreateBuffer(&bufferDescription, &bufferBytes, target);
}

void SurfaceMesh::CreateVertexResources(ID3D11Device* device) {
	if (m_surfaceMesh == nullptr) {
		// Not yet ready.
		m_isActive = false;
		return;
	}

	if (m_surfaceMesh->TriangleIndices->ElementCount < 3) {
		// Not enough indices to draw a triangle.
		m_isActive = false;
		return;
	}

	// Surface mesh resources are created off-thread, so that they don't affect rendering latency.'
	auto taskOptions = Concurrency::task_options();
	auto task = concurrency::create_task([this, device]() {
		// Create new Direct3D device resources for the updated buffers. These will be set aside
		// for now, and then swapped into the active slot next time the render loop is ready to draw.

		// First, we acquire the raw data buffers.
		Windows::Storage::Streams::IBuffer^ positions = m_surfaceMesh->VertexPositions->Data;
		Windows::Storage::Streams::IBuffer^ normals = m_surfaceMesh->VertexNormals->Data;
		Windows::Storage::Streams::IBuffer^ indices = m_surfaceMesh->TriangleIndices->Data;

		// Then, we create Direct3D device buffers with the mesh data provided by HoloLens.
		Microsoft::WRL::ComPtr<ID3D11Buffer> updatedVertexPositions;
		Microsoft::WRL::ComPtr<ID3D11Buffer> updatedVertexNormals;
		Microsoft::WRL::ComPtr<ID3D11Buffer> updatedTriangleIndices;
		CreateDirectXBuffer(device, D3D11_BIND_VERTEX_BUFFER, positions, updatedVertexPositions.GetAddressOf());
		CreateDirectXBuffer(device, D3D11_BIND_VERTEX_BUFFER, normals, updatedVertexNormals.GetAddressOf());
		CreateDirectXBuffer(device, D3D11_BIND_INDEX_BUFFER, indices, updatedTriangleIndices.GetAddressOf());

		// Before updating the meshes, check to ensure that there wasn't a more recent update.
		{
			std::lock_guard<std::mutex> lock(m_meshResourcesMutex);

			auto meshUpdateTime = m_surfaceMesh->SurfaceInfo->UpdateTime;
			if (meshUpdateTime.UniversalTime > m_lastUpdateTime.UniversalTime) {

				// Prepare to swap in the new meshes.
				// Here, we use ComPtr.Swap() to avoid unnecessary overhead from ref counting.
				m_updatedVertexPositions.Swap(updatedVertexPositions);
				m_updatedVertexNormals.Swap(updatedVertexNormals);
				m_updatedTriangleIndices.Swap(updatedTriangleIndices);

				// Cache properties for the buffers we will now use.
				m_updatedMeshProperties.vertexStride = m_surfaceMesh->VertexPositions->Stride;
				m_updatedMeshProperties.normalStride = m_surfaceMesh->VertexNormals->Stride;
				m_updatedMeshProperties.indexCount = m_surfaceMesh->TriangleIndices->ElementCount;
				m_updatedMeshProperties.indexFormat = static_cast<DXGI_FORMAT>(m_surfaceMesh->TriangleIndices->Format);

				// Send a signal to the render loop indicating that new resources are available to use.
				m_updateReady = true;
				m_lastUpdateTime = meshUpdateTime;
				m_loadingComplete = true;
			}
		}
	});
}

void SurfaceMesh::CreateDeviceDependentResources(ID3D11Device* device) {
	CreateVertexResources(device);

	// Create a constant buffer to control mesh position.
	CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelNormalConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
	DX::ThrowIfFailed(device->CreateBuffer(&constantBufferDesc,	nullptr, &m_modelTransformBuffer));

	m_constantBufferCreated = true;
}

void SurfaceMesh::ReleaseVertexResources() {
	m_vertexPositions.Reset();
	m_vertexNormals.Reset();
	m_triangleIndices.Reset();
}

void SurfaceMesh::SwapVertexBuffers() {
	// Swap out the previous vertex position, normal, and index buffers, and replace
	// them with up-to-date buffers.
	m_vertexPositions = m_updatedVertexPositions;
	m_vertexNormals = m_updatedVertexNormals;
	m_triangleIndices = m_updatedTriangleIndices;

	// Swap out the metadata: index count, index format, .
	m_meshProperties = m_updatedMeshProperties;

	ZeroMemory(&m_updatedMeshProperties, sizeof(SurfaceMeshProperties));
	m_updatedVertexPositions.Reset();
	m_updatedVertexNormals.Reset();
	m_updatedTriangleIndices.Reset();
}

void SurfaceMesh::ReleaseDeviceDependentResources() {
	// Clear out any pending resources.
	SwapVertexBuffers();

	// Clear out active resources.
	ReleaseVertexResources();

	m_modelTransformBuffer.Reset();

	m_constantBufferCreated = false;
	m_loadingComplete = false;
}

// Deletes data stored in m_localMesh and recreates it.
void SurfaceMesh::UpdateLocalMesh() {
	ClearLocalMesh();
	m_localMesh = ConstructMeshData();
}

// Deletes m_localMesh.
void SurfaceMesh::ClearLocalMesh() {
	if (m_localMesh.verts) {
		delete m_localMesh.verts;
		m_localMesh.vertCount = 0;
	}

	if (m_localMesh.normals) {
		delete m_localMesh.normals;
	}

	if (m_localMesh.indices) {
		delete m_localMesh.indices;
		m_localMesh.indexCount = 0;
	}

	m_localMesh.transform = XMFloat4x4Identity;
}

// Return a MeshData object from the Raw data buffers.
MeshData SurfaceMesh::ConstructMeshData() {
	// we configured RealtimeSurfaceMeshRenderer to ensure that the data
	// we are receiving is in the correct format.
	// Vertex Positions: R16G16B16A16IntNormalized
	// Vertex Normals: R8G8B8A8IntNormalized
	// Indices: R16UInt (we'll convert it from here to R32Int. HoloLens Spatial Mapping doesn't appear to support this format directly.

	MeshData newMesh;
	newMesh.vertCount = m_surfaceMesh->VertexPositions->ElementCount;
	newMesh.verts = new XMFLOAT3[newMesh.vertCount];
	newMesh.normals = new XMFLOAT3[newMesh.vertCount];
	newMesh.indexCount = m_surfaceMesh->TriangleIndices->ElementCount;
	newMesh.indices = new INT32[newMesh.indexCount];

	XMSHORTN4* rawVertexData = (XMSHORTN4*)GetDataFromIBuffer(m_surfaceMesh->VertexPositions->Data);
	XMBYTEN4* rawNormalData = (XMBYTEN4*)GetDataFromIBuffer(m_surfaceMesh->VertexNormals->Data);
	UINT16* rawIndexData = (UINT16*)GetDataFromIBuffer(m_surfaceMesh->TriangleIndices->Data);
	float3 vertexScale = m_surfaceMesh->VertexPositionScale;
	
	for (int index = 0; index < newMesh.vertCount; ++index) {
		// read the current position as an XMSHORTN4.
		XMSHORTN4 currentPos = XMSHORTN4(rawVertexData[index]);
		XMFLOAT4 vals;

		// XMVECTOR knows how to convert XMSHORTN4 to actual floating point coordinates.
		XMVECTOR vec = XMLoadShortN4(&currentPos);

		// Store that into an XMFLOAT4 so we can read the values.
		XMStoreFloat4(&vals, vec);

		// Scale by the vertex scale.
		XMFLOAT4 scaledPos = XMFLOAT4(vals.x * vertexScale.x, vals.y * vertexScale.y, vals.z * vertexScale.z, vals.w);

		// Then we need to down scale the vector since it will be rescaled when rendering (ie divide by w).
		// do we?
		float4 downScaledPos = float4(scaledPos.x, scaledPos.y, scaledPos.z, scaledPos.w);

		newMesh.verts[index].x = downScaledPos.x;
		newMesh.verts[index].y = downScaledPos.y;
		newMesh.verts[index].z = downScaledPos.z;

		// now do the same for the normal.
		XMBYTEN4 currentNormal = XMBYTEN4(rawNormalData[index]);
		XMFLOAT4 norms;
		XMVECTOR norm = XMLoadByteN4(&currentNormal);
		XMStoreFloat4(&norms, norm);
		// No need to downscale. Does nothing.
		newMesh.normals[index].x = norms.x;
		newMesh.normals[index].y = norms.y;
		newMesh.normals[index].z = norms.z;
	}

	for (int index = 0; index < newMesh.indexCount; ++index) {
		newMesh.indices[index] = rawIndexData[index];
	}

	newMesh.transform = XMFloat4x4Identity;

	return newMesh;
}