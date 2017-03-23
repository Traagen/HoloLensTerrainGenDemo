#pragma once

namespace HoloLensTerrainGenDemo
{
    // Sample gesture handler.
    // Hooks up events to recognize a tap gesture, and keeps track of input using a boolean value.
    class SpatialInputHandler
    {
    public:
        SpatialInputHandler();
        ~SpatialInputHandler();

        Windows::UI::Input::Spatial::SpatialInteractionSourceState^ CheckForInput();
		unsigned short CheckForTap();
		bool CheckForHold();

    private:
        // Source pressed event handler.
        void OnSourcePressed(
            Windows::UI::Input::Spatial::SpatialInteractionManager^ sender,
            Windows::UI::Input::Spatial::SpatialInteractionSourceEventArgs^ args);

		// Interaction event handler.
		void OnInteractionDetected(
			Windows::UI::Input::Spatial::SpatialInteractionManager^ sender,
			Windows::UI::Input::Spatial::SpatialInteractionDetectedEventArgs^ args);

		void OnTap(Windows::UI::Input::Spatial::SpatialGestureRecognizer^ sender,
			Windows::UI::Input::Spatial::SpatialTappedEventArgs^ args);

		void OnHoldCompleted(Windows::UI::Input::Spatial::SpatialGestureRecognizer^ sender, 
			Windows::UI::Input::Spatial::SpatialHoldCompletedEventArgs^ args);

        // API objects used to process gesture input, and generate gesture events.
        Windows::UI::Input::Spatial::SpatialInteractionManager^     m_interactionManager;

        // Event registration token.
        Windows::Foundation::EventRegistrationToken                 m_sourcePressedEventToken;
		Windows::Foundation::EventRegistrationToken                 m_interactionDetectedEventToken;
		Windows::Foundation::EventRegistrationToken					m_tapGestureEventToken;
		Windows::Foundation::EventRegistrationToken					m_holdGestureCompletedEventToken;

        // Used to indicate that a Pressed input event was received this frame.
        Windows::UI::Input::Spatial::SpatialInteractionSourceState^ m_sourceState = nullptr;

		// Recognizes valid gestures passed to the Terrain object.
		Windows::UI::Input::Spatial::SpatialGestureRecognizer^		m_gestureRecognizer;

		unsigned short m_numTaps = 0;
		bool m_holdCompleted = false;
    };
}
