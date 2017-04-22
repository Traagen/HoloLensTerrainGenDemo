#pragma once

#include "Common\DeviceResources.h"
#include "Common\StepTimer.h"
#include "Content\GUIManager.h"
#include "Content\Terrain.h"
#include "Content\RealtimeSurfaceMeshRenderer.h"
#include "Content\SurfacePlaneRenderer.h"

// Updates, renders, and presents holographic content using Direct3D.
namespace HoloLensTerrainGenDemo {
    class HoloLensTerrainGenDemoMain : public DX::IDeviceNotify {
    public:
        HoloLensTerrainGenDemoMain(const std::shared_ptr<DX::DeviceResources>& deviceResources);
        ~HoloLensTerrainGenDemoMain();

        // Sets the holographic space. This is our closest analogue to setting a new window
        // for the app.
        void SetHolographicSpace(Windows::Graphics::Holographic::HolographicSpace^ holographicSpace);

        // Starts the holographic frame and updates the content.
        Windows::Graphics::Holographic::HolographicFrame^ Update();

        // Renders holograms, including world-locked content.
        bool Render(Windows::Graphics::Holographic::HolographicFrame^ holographicFrame);

        // Handle saving and loading of app state owned by AppMain.
        void SaveAppState();
        void LoadAppState();

        // IDeviceNotify
        virtual void OnDeviceLost();
        virtual void OnDeviceRestored();

		// Handle surface change events.
		void OnSurfacesChanged(Windows::Perception::Spatial::Surfaces::SpatialSurfaceObserver^ sender, Platform::Object^ args);

    private:
        // Asynchronously creates resources for new holographic cameras.
        void OnCameraAdded(
            Windows::Graphics::Holographic::HolographicSpace^ sender,
            Windows::Graphics::Holographic::HolographicSpaceCameraAddedEventArgs^ args);

        // Synchronously releases resources for holographic cameras that are no longer
        // attached to the system.
        void OnCameraRemoved(
            Windows::Graphics::Holographic::HolographicSpace^ sender,
            Windows::Graphics::Holographic::HolographicSpaceCameraRemovedEventArgs^ args);

        // Used to notify the app when the positional tracking state changes.
        void OnLocatabilityChanged(
            Windows::Perception::Spatial::SpatialLocator^ sender,
            Platform::Object^ args);

		// Used to prevent the device from deactivating positional tracking, which is 
		// necessary to continue to receive spatial mapping data.
		void OnPositionalTrackingDeactivating(
			Windows::Perception::Spatial::SpatialLocator^ sender,
			Windows::Perception::Spatial::SpatialLocatorPositionalTrackingDeactivatingEventArgs^ args);

		// Interaction event handler.
		void OnInteractionDetected(
			Windows::UI::Input::Spatial::SpatialInteractionManager^ sender,
			Windows::UI::Input::Spatial::SpatialInteractionDetectedEventArgs^ args);

        // Clears event registration state. Used when changing to a new HolographicSpace
        // and when tearing down AppMain.
        void UnregisterHolographicEventHandlers();

        // Manages GUI elements.
        std::shared_ptr<GUIManager>											m_guiManager;

		// API objects used to process gesture input, and generate gesture events.
		Windows::UI::Input::Spatial::SpatialInteractionManager^			   m_interactionManager;
	
		// Our terrain
		std::unique_ptr<Terrain>											m_terrain;
        
		// Cached pointer to device resources.
        std::shared_ptr<DX::DeviceResources>								m_deviceResources;
        
		// Render loop timer.
        DX::StepTimer													    m_timer;
		// Slow timer used to update plane finding.
		DX::StepTimer														m_planeReadTimer;
        
		// Represents the holographic space around the user.
        Windows::Graphics::Holographic::HolographicSpace^			        m_holographicSpace;
        
		// SpatialLocator that is attached to the primary camera.
        Windows::Perception::Spatial::SpatialLocator^						m_locator;
        
		// A reference frame attached to the holographic camera.
        Windows::Perception::Spatial::SpatialLocatorAttachedFrameOfReference^ m_referenceFrame;

		// Event registration tokens.
        Windows::Foundation::EventRegistrationToken							m_cameraAddedToken;
        Windows::Foundation::EventRegistrationToken							m_cameraRemovedToken;
        Windows::Foundation::EventRegistrationToken							m_locatabilityChangedToken;
		Windows::Foundation::EventRegistrationToken                         m_positionalTrackingDeactivatingToken;
		Windows::Foundation::EventRegistrationToken                         m_surfacesChangedToken;
		Windows::Foundation::EventRegistrationToken						    m_interactionDetectedEventToken;

		// Obtains surface mapping data from the device in real time.
		Windows::Perception::Spatial::Surfaces::SpatialSurfaceObserver^     m_surfaceObserver;

		// A data handler for surface meshes.
		std::unique_ptr<RealtimeSurfaceMeshRenderer> m_meshRenderer;

		// A data handler for surface planes.
		std::unique_ptr<SurfacePlaneRenderer> m_planeRenderer;
    };
}
