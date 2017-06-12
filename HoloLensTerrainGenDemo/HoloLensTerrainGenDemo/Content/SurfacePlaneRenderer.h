#pragma once
#include "Common\DeviceResources.h"
#include "ShaderStructures.h"
#include "Common\PlaneFinding\PlaneFinding.h"

namespace HoloLensTerrainGenDemo {
	class SurfacePlaneRenderer {
	public:
		SurfacePlaneRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		~SurfacePlaneRenderer();

		void UpdatePlanes(std::vector<PlaneFinding::BoundedPlane> newList, Windows::Perception::Spatial::SpatialCoordinateSystem^ cs);
		void Update(Windows::Perception::Spatial::SpatialCoordinateSystem^ baseCoordinateSystem);
		void Render();

		void CreateVertexResources();
		void CreateDeviceDependentResources();
		void ReleaseDeviceDependentResources();

		bool CaptureInteraction(Windows::UI::Input::Spatial::SpatialInteraction^ interaction);

		Windows::Perception::Spatial::SpatialAnchor^ GetAnchor();
		Windows::Foundation::Numerics::float2 GetDimensions();
		DirectX::XMFLOAT4X4 GetOrientation();
		bool WasTappedRecently() { return m_WasTapped;  }

	private:
		void CreateShaders();

		// Event handler for gesture recognition.
		void OnTap(Windows::UI::Input::Spatial::SpatialGestureRecognizer^ sender,
			Windows::UI::Input::Spatial::SpatialTappedEventArgs^ args);

		// Cached pointer to device resources.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		// If the current D3D Device supports VPRT, we can avoid using a geometry
		// shader just to set the render target array index.
		bool m_usingVprtShaders = false;

		// Direct3D resources for rendering pipeline.
		Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
		Microsoft::WRL::ComPtr<ID3D11GeometryShader> m_geometryShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;

		// Rasterizer states
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_defaultRasterizerState;

		// A way to lock map access.
		std::mutex m_planeListLock;

		std::vector<PlaneFinding::BoundedPlane>	m_planeList;

		Microsoft::WRL::ComPtr<ID3D11Buffer> m_constantBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;

		ModelConstantBuffer m_constantBufferData;
		DirectX::XMFLOAT4X4 m_modelToWorld;

		Windows::Perception::Spatial::SpatialCoordinateSystem^ m_coordinateSystem;

		bool m_shadersReady = false;

		// Recognizes valid gestures passed to the Terrain object.
		Windows::UI::Input::Spatial::SpatialGestureRecognizer^ m_gestureRecognizer;

		// event token
		Windows::Foundation::EventRegistrationToken			m_tapGestureEventToken;

		// defines the last intersected plane
		int m_intersectedPlane = -1;
		bool m_WasTapped = false;
	};
};
