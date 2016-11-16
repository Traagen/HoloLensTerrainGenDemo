#pragma once

#include "..\Common\DeviceResources.h"
#include "..\Common\StepTimer.h"
#include "ShaderStructures.h"
#include "BSP Tree.h"
#include <random>

namespace HoloLensTerrainGenDemo {
	class Terrain {
	public:
		// provide h and w in centimeters.
		// res is number of triangle edges per centimeter.
		Terrain(const std::shared_ptr<DX::DeviceResources>& deviceResources, float h, float w, unsigned int res);
		~Terrain();
		void CreateDeviceDependentResources();
		void ReleaseDeviceDependentResources();
		void Update(const DX::StepTimer& timer);
		void Render();

		// Repositions the sample hologram.
		void PositionHologram(Windows::UI::Input::Spatial::SpatialPointerPose^ pointerPose);

		// Property accessors.
		void SetPosition(Windows::Foundation::Numerics::float3 pos) { m_position = pos; }
		Windows::Foundation::Numerics::float3 GetPosition() { return m_position; }

		// Reset the Height map to all zeros.
		void ResetHeightMap();

	private:
		// initializes the height map to the supplied dimensions.
		void InitializeHeightmap();
		void IterateFaultFormation(unsigned int treeDepth, float treeAmplitude);
		// Recursively generate a BSP Tree of specified depth for use in Fault Formation algorithm.
		// depth of 1 is a leaf node.
		void BuildBSPTree(BSPNode* current, unsigned int depth);
		void FIRFilter(float filter);

		// Cached pointer to device resources.
		std::shared_ptr<DX::DeviceResources>			    m_deviceResources;
		// Direct3D resources for cube geometry.
		Microsoft::WRL::ComPtr<ID3D11InputLayout>		    m_inputLayout;
		Microsoft::WRL::ComPtr<ID3D11Buffer>			    m_vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer>			    m_indexBuffer;
		Microsoft::WRL::ComPtr<ID3D11VertexShader>		    m_vertexShader;
		Microsoft::WRL::ComPtr<ID3D11GeometryShader>	    m_geometryShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader>		    m_pixelShader;
		Microsoft::WRL::ComPtr<ID3D11Buffer>			    m_modelConstantBuffer;
		// Direct3D resources for heightmap.	
		Microsoft::WRL::ComPtr<ID3D11Texture2D>				m_hmTexture;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	m_hmSRV;
		// System resources for cube geometry.
		ModelConstantBuffer									m_modelConstantBufferData;
		uint32											    m_indexCount = 0;
		// Variables used with the rendering loop.
		bool											    m_loadingComplete = false;
		Windows::Foundation::Numerics::float3			    m_position = { 0.f, -0.2f, -1.f };
		// If the current D3D Device supports VPRT, we can avoid using a geometry
		// shader just to set the render target array index.
		bool											    m_usingVprtShaders = false;
		float*											 	m_heightmap;
		// width of the heightmap texture.
		unsigned int										m_wHeightmap;
		// height of the heighmap texture.
		unsigned int										m_hHeightmap;
		// resolution as measurement of number of triangle edges per centimeter.
		// holoLens scales by 1 = 1m, so if width of surface is 0.1, that's
		// 10cm. For resolution 4, we'd have 10*4 + 1 vertices.
		unsigned int										m_resHeightmap;
		std::default_random_engine generator;
		// iterator for tracking iteration of terrain generator.
		unsigned int										m_iIter = 0;
	};
};
