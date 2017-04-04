#pragma once

namespace HoloLensTerrainGenDemo
{
    // Sample gesture handler.
    // Hooks up events to recognize a tap gesture, and keeps track of input using a boolean value.
	class GUIManager
	{
	public:
		GUIManager();
		~GUIManager();
		void CaptureInteraction(Windows::UI::Input::Spatial::SpatialInteraction^ interaction);
		
		bool GetRenderSurfaces() { return renderSurfaces; }
		bool GetRenderWireframe() { return renderWireframe; }

    private:
		void OnTap(Windows::UI::Input::Spatial::SpatialGestureRecognizer^ sender,
			Windows::UI::Input::Spatial::SpatialTappedEventArgs^ args);

		void OnHoldCompleted(Windows::UI::Input::Spatial::SpatialGestureRecognizer^ sender, 
			Windows::UI::Input::Spatial::SpatialHoldCompletedEventArgs^ args);

        // Event registration token.
		Windows::Foundation::EventRegistrationToken					m_tapGestureEventToken;
		Windows::Foundation::EventRegistrationToken					m_holdGestureCompletedEventToken;

		// Recognizes valid gestures passed to the Terrain object.
		Windows::UI::Input::Spatial::SpatialGestureRecognizer^		m_gestureRecognizer;

		bool renderSurfaces = true;
		bool renderWireframe = false;
    };
}
