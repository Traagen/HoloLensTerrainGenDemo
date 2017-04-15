#pragma once
#include "Common\DeviceResources.h"
#include "ShaderStructures.h"
#include "Common\PlaneFinding\PlaneFinding.h"

namespace HoloLensTerrainGenDemo {
	class SurfacePlaneRenderer {
	public:
		SurfacePlaneRenderer();
		~SurfacePlaneRenderer();

		void CreateVertexResources(ID3D11Device* device);
		void CreateDeviceDependentResources(ID3D11Device* device);
		void ReleaseDeviceDependentResources();

	private:
		std::vector<PlaneFinding::BoundedPlane>	m_planeList;

		Microsoft::WRL::ComPtr<ID3D11Buffer> m_constantBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;

		ModelConstantBuffer m_constantBufferData;

		bool m_createdConstantBuffer = false;
	};
};
