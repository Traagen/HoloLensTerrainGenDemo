#include "pch.h"
#include "HoloLensTerrainGenDemoMain.h"
#include "Common\DirectXHelper.h"

#include <windows.graphics.directx.direct3d11.interop.h>
#include <Collection.h>

using namespace HoloLensTerrainGenDemo;

using namespace concurrency;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Numerics;
using namespace Windows::Graphics::Holographic;
using namespace Windows::Perception::Spatial;
using namespace Windows::UI::Input::Spatial;
using namespace std::placeholders;
using namespace Windows::Foundation::Collections;
using namespace Windows::Perception::Spatial::Surfaces;

// Loads and initializes application assets when the application is loaded.
HoloLensTerrainGenDemoMain::HoloLensTerrainGenDemoMain(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
    m_deviceResources(deviceResources) {
    // Register to be notified if the device is lost or recreated.
    m_deviceResources->RegisterDeviceNotify(this);

	m_planeReadTimer.SetFixedTimeStep(true);
	m_planeReadTimer.SetTargetElapsedSeconds(5.0f);
}

void HoloLensTerrainGenDemoMain::SetHolographicSpace(HolographicSpace^ holographicSpace) {
	UnregisterHolographicEventHandlers();

	m_holographicSpace = holographicSpace;

	m_terrain = nullptr;
	m_surfaceObserver = nullptr;
	m_meshRenderer = std::make_unique<RealtimeSurfaceMeshRenderer>(m_deviceResources);
	m_planeRenderer = std::make_unique<SurfacePlaneRenderer>(m_deviceResources);

	// Initialize the GUI
	m_guiManager = std::make_unique<GUIManager>();

	// The interaction manager provides an event that informs the app when
	// spatial interactions are detected.
	m_interactionManager = SpatialInteractionManager::GetForCurrentView();

	m_interactionDetectedEventToken = m_interactionManager->InteractionDetected +=
		ref new TypedEventHandler<SpatialInteractionManager^, SpatialInteractionDetectedEventArgs^>(
			bind(&HoloLensTerrainGenDemoMain::OnInteractionDetected, this, _1, _2)
			);

	// Use the default SpatialLocator to track the motion of the device.
	m_locator = SpatialLocator::GetDefault();

	// This sample responds to changes in the positional tracking state by cancelling deactivation 
	// of positional tracking.
	m_positionalTrackingDeactivatingToken =
		m_locator->PositionalTrackingDeactivating +=
		ref new Windows::Foundation::TypedEventHandler<SpatialLocator^, SpatialLocatorPositionalTrackingDeactivatingEventArgs^>(
			std::bind(&HoloLensTerrainGenDemoMain::OnPositionalTrackingDeactivating, this, _1, _2)
			);

	// Respond to camera added events by creating any resources that are specific
	// to that camera, such as the back buffer render target view.
	// When we add an event handler for CameraAdded, the API layer will avoid putting
	// the new camera in new HolographicFrames until we complete the deferral we created
	// for that handler, or return from the handler without creating a deferral. This
	// allows the app to take more than one frame to finish creating resources and
	// loading assets for the new holographic camera.
	// This function should be registered before the app creates any HolographicFrames.
	m_cameraAddedToken =
		m_holographicSpace->CameraAdded +=
		ref new Windows::Foundation::TypedEventHandler<HolographicSpace^, HolographicSpaceCameraAddedEventArgs^>(
			std::bind(&HoloLensTerrainGenDemoMain::OnCameraAdded, this, _1, _2)
			);

	// Respond to camera removed events by releasing resources that were created for that
	// camera.
	// When the app receives a CameraRemoved event, it releases all references to the back
	// buffer right away. This includes render target views, Direct2D target bitmaps, and so on.
	// The app must also ensure that the back buffer is not attached as a render target, as
	// shown in DeviceResources::ReleaseResourcesForBackBuffer.
	m_cameraRemovedToken =
		m_holographicSpace->CameraRemoved +=
		ref new Windows::Foundation::TypedEventHandler<HolographicSpace^, HolographicSpaceCameraRemovedEventArgs^>(
			std::bind(&HoloLensTerrainGenDemoMain::OnCameraRemoved, this, _1, _2)
			);

	m_locatabilityChangedToken =
		m_locator->LocatabilityChanged +=
		ref new Windows::Foundation::TypedEventHandler<SpatialLocator^, Object^>(
			std::bind(&HoloLensTerrainGenDemoMain::OnLocatabilityChanged, this, _1, _2)
			);

	// The simplest way to render world-locked holograms is to create a stationary reference frame
	// when the app is launched. This is roughly analogous to creating a "world" coordinate system
	// with the origin placed at the device's position as the app is launched.
	m_referenceFrame = m_locator->CreateAttachedFrameOfReferenceAtCurrentHeading();

	// The spatial mapping API reads information about the user's environment. The user must
	// grant permission to the app to use this capability of the Windows Holographic device.
	auto requestPerceptionAccessTask = create_task(SpatialSurfaceObserver::RequestAccessAsync());
	requestPerceptionAccessTask.then([this](Windows::Perception::Spatial::SpatialPerceptionAccessStatus status) {
		if (status == SpatialPerceptionAccessStatus::Allowed) {
			// Create an initial stationary reference frame to get a coordinate system from to calculate our initial
			// bounding volume for the surface observer.
			SpatialStationaryFrameOfReference^ baseReference = m_locator->CreateStationaryFrameOfReferenceAtCurrentLocation();
				
			SpatialBoundingBox aabb = {
				{ 0.f,  0.f, 0.f },
				{ 20.f, 20.f, 5.f },
			};
			SpatialBoundingVolume^ bounds = SpatialBoundingVolume::FromBox(baseReference->CoordinateSystem, aabb);

			// Create the observer.
			m_surfaceObserver = ref new SpatialSurfaceObserver();
			if (m_surfaceObserver) {
				m_surfaceObserver->SetBoundingVolume(bounds);

				// If the surface observer was successfully created, we can initialize our
				// collection by pulling the current data set.
				auto mapContainingSurfaceCollection = m_surfaceObserver->GetObservedSurfaces();
				for (auto const& pair : mapContainingSurfaceCollection) {
					auto const& id = pair->Key;
					auto const& surfaceInfo = pair->Value;
					m_meshRenderer->AddSurface(id, surfaceInfo);
				}

				// We then subscribe to an event to receive up-to-date data.
				m_surfacesChangedToken = m_surfaceObserver->ObservedSurfacesChanged +=
					ref new TypedEventHandler<SpatialSurfaceObserver^, Platform::Object^>(
						bind(&HoloLensTerrainGenDemoMain::OnSurfacesChanged, this, _1, _2)
						);
			}
		}
	});
}

void HoloLensTerrainGenDemoMain::UnregisterHolographicEventHandlers() {
    if (m_holographicSpace != nullptr) {
        // Clear previous event registrations.

        if (m_cameraAddedToken.Value != 0) {
            m_holographicSpace->CameraAdded -= m_cameraAddedToken;
            m_cameraAddedToken.Value = 0;
        }

        if (m_cameraRemovedToken.Value != 0) {
            m_holographicSpace->CameraRemoved -= m_cameraRemovedToken;
            m_cameraRemovedToken.Value = 0;
        }
    }

    if (m_locator != nullptr) {
		m_locator->PositionalTrackingDeactivating -= m_positionalTrackingDeactivatingToken;
		m_locator->LocatabilityChanged -= m_locatabilityChangedToken;
    }

	if (m_surfaceObserver != nullptr)
	{
		m_surfaceObserver->ObservedSurfacesChanged -= m_surfacesChangedToken;
	}

	// Unregister our handler for the InteractionDetected event.
	if (m_interactionManager) {
		m_interactionManager->InteractionDetected -= m_interactionDetectedEventToken;
	}
}

HoloLensTerrainGenDemoMain::~HoloLensTerrainGenDemoMain() {
    // Deregister device notification.
    m_deviceResources->RegisterDeviceNotify(nullptr);
	
    UnregisterHolographicEventHandlers();
}

// Updates the application state once per frame.
HolographicFrame^ HoloLensTerrainGenDemoMain::Update() {
	// Before doing the timer update, there is some work to do per-frame
	// to maintain holographic rendering. First, we will get information
	// about the current frame.

	// The HolographicFrame has information that the app needs in order
	// to update and render the current frame. The app begins each new
	// frame by calling CreateNextFrame.
	HolographicFrame^ holographicFrame = m_holographicSpace->CreateNextFrame();

	// Get a prediction of where holographic cameras will be when this frame
	// is presented.
	HolographicFramePrediction^ prediction = holographicFrame->CurrentPrediction;

	// Back buffers can change from frame to frame. Validate each buffer, and recreate
	// resource views and depth buffers as needed.
	m_deviceResources->EnsureCameraResources(holographicFrame, prediction);

	// Next, we get a coordinate system from the attached frame of reference that is
	// associated with the current frame. Later, this coordinate system is used for
	// for creating the stereo view matrices when rendering the sample content.
	SpatialCoordinateSystem^ currentCoordinateSystem = m_referenceFrame->GetStationaryCoordinateSystemAtTimestamp(prediction->Timestamp);
	
	if (m_surfaceObserver)	{
		SpatialBoundingBox aabb =	{
			{ 0.f,  0.f, 0.f },
			{ 20.f, 20.f, 5.f },
		};
		SpatialBoundingVolume^ bounds = SpatialBoundingVolume::FromBox(currentCoordinateSystem, aabb);

		// Keep the surface observer positioned at the device's location.
		m_surfaceObserver->SetBoundingVolume(bounds);
	}

    m_timer.Tick([&] () {
        // Put time-based updates here. By default this code will run once per frame,
        // but if you change the StepTimer to use a fixed time step this code will
        // run as many times as needed to get to the current step.
		if (m_terrain) {
			m_terrain->Update(m_timer, currentCoordinateSystem);
		}

		m_meshRenderer->Update(m_timer, currentCoordinateSystem);
		m_planeRenderer->Update(currentCoordinateSystem);
    });

	
	// if we haven't generated a terrain yet, check if we have recently
	// tapped on a surface plane;
	if (!m_terrain && m_planeRenderer->WasTappedRecently()) {
		auto anchor = m_planeRenderer->GetAnchor();
		auto dimensions = m_planeRenderer->GetDimensions();
		m_terrain = std::make_unique<Terrain>(m_deviceResources, 0.6f, 0.6f, 4, anchor);
	}

	// We complete the frame update by using information about our content positioning
    // to set the focus point.
	for (auto cameraPose : prediction->CameraPoses) {
        // The HolographicCameraRenderingParameters class provides access to set
        // the image stabilization parameters.
        HolographicCameraRenderingParameters^ renderingParameters = holographicFrame->GetRenderingParameters(cameraPose);

        // SetFocusPoint informs the system about a specific point in your scene to
        // prioritize for image stabilization. The focus point is set independently
        // for each holographic camera.
        // You should set the focus point near the content that the user is looking at.
        // In this example, we put the focus point at the center of the sample hologram,
        // since that is the only hologram available for the user to focus on.
        // You can also set the relative velocity and facing of that content; the sample
        // hologram is at a fixed point so we only need to indicate its position.
		if (m_terrain) {
			renderingParameters->SetFocusPoint(currentCoordinateSystem, m_terrain->GetPosition());
		}
    }

    // The holographic frame will be used to get up-to-date view and projection matrices and
    // to present the swap chain.
    return holographicFrame;
}

// Renders the current frame to each holographic camera, according to the
// current application and spatial positioning state. Returns true if the
// frame was rendered to at least one camera.
bool HoloLensTerrainGenDemoMain::Render(HolographicFrame^ holographicFrame) {
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return false;
    }

    //
    // TODO: Add code for pre-pass rendering here.
    //
    // Take care of any tasks that are not specific to an individual holographic
    // camera. This includes anything that doesn't need the final view or projection
    // matrix, such as lighting maps.
    //

    // Lock the set of holographic camera resources, then draw to each camera
    // in this frame.
    return m_deviceResources->UseHolographicCameraResources<bool>(
        [this, holographicFrame](std::map<UINT32, std::unique_ptr<DX::CameraResources>>& cameraResourceMap)
    {
        // Up-to-date frame predictions enhance the effectiveness of image stablization and
        // allow more accurate positioning of holograms.
        holographicFrame->UpdateCurrentPrediction();
        HolographicFramePrediction^ prediction = holographicFrame->CurrentPrediction;

        bool atLeastOneCameraRendered = false;
        for (auto cameraPose : prediction->CameraPoses)
        {
            // This represents the device-based resources for a HolographicCamera.
            DX::CameraResources* pCameraResources = cameraResourceMap[cameraPose->HolographicCamera->Id].get();

            // Get the device context.
            const auto context = m_deviceResources->GetD3DDeviceContext();
            const auto depthStencilView = pCameraResources->GetDepthStencilView();

            // Set render targets to the current holographic camera.
            ID3D11RenderTargetView *const targets[1] = { pCameraResources->GetBackBufferRenderTargetView() };
            context->OMSetRenderTargets(1, targets, depthStencilView);

            // Clear the back buffer and depth stencil view.
            context->ClearRenderTargetView(targets[0], DirectX::Colors::Transparent);
            context->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

            //
            // TODO: Replace the sample content with your own content.
            //
            // Notes regarding holographic content:
            //    * For drawing, remember that you have the potential to fill twice as many pixels
            //      in a stereoscopic render target as compared to a non-stereoscopic render target
            //      of the same resolution. Avoid unnecessary or repeated writes to the same pixel,
            //      and only draw holograms that the user can see.
            //    * To help occlude hologram geometry, you can create a depth map using geometry
            //      data obtained via the surface mapping APIs. You can use this depth map to avoid
            //      rendering holograms that are intended to be hidden behind tables, walls,
            //      monitors, and so on.
            //    * Black pixels will appear transparent to the user wearing the device, but you
            //      should still use alpha blending to draw semitransparent holograms. You should
            //      also clear the screen to Transparent as shown above.
            //


            // The view and projection matrices for each holographic camera will change
            // every frame. This function refreshes the data in the constant buffer for
            // the holographic camera indicated by cameraPose.
            pCameraResources->UpdateViewProjectionBuffer(m_deviceResources, cameraPose, m_referenceFrame->GetStationaryCoordinateSystemAtTimestamp(prediction->Timestamp));
			
            // Attach the view/projection constant buffer for this camera to the graphics pipeline.
            bool cameraActive = pCameraResources->AttachViewProjectionBuffer(m_deviceResources);
			
			// Only render world-locked content when positional tracking is active.
			if (cameraActive) {
				m_meshRenderer->Render(pCameraResources->IsRenderingStereoscopic(), m_guiManager->GetRenderWireframe(), !m_guiManager->GetRenderSurfaces());
				
				// Draw the sample hologram.
				if (m_terrain) {
					m_terrain->Render();
				}
				else {
					m_planeRenderer->Render();
				}
			}

            atLeastOneCameraRendered = true;
        }

        return atLeastOneCameraRendered;
    });
}

void HoloLensTerrainGenDemoMain::SaveAppState() {
    //
    // TODO: Insert code here to save your app state.
    //       This method is called when the app is about to suspend.
    //
    //       For example, store information in the SpatialAnchorStore.
    //
}

void HoloLensTerrainGenDemoMain::LoadAppState() {
    //
    // TODO: Insert code here to load your app state.
    //       This method is called when the app resumes.
    //
    //       For example, load information from the SpatialAnchorStore.
    //
}

// Notifies classes that use Direct3D device resources that the device resources
// need to be released before this method returns.
void HoloLensTerrainGenDemoMain::OnDeviceLost() {
	if (m_terrain) {
		m_terrain->ReleaseDeviceDependentResources();
	}

	m_meshRenderer->ReleaseDeviceDependentResources();
	m_planeRenderer->ReleaseDeviceDependentResources();
}

// Notifies classes that use Direct3D device resources that the device resources
// may now be recreated.
void HoloLensTerrainGenDemoMain::OnDeviceRestored() {
	if (m_terrain) {
		m_terrain->CreateDeviceDependentResources();
	}

	m_meshRenderer->CreateDeviceDependentResources();
	m_planeRenderer->CreateDeviceDependentResources();
}

void HoloLensTerrainGenDemoMain::OnLocatabilityChanged(SpatialLocator^ sender, Object^ args) {
    switch (sender->Locatability)
    {
    case SpatialLocatability::Unavailable:
        // Holograms cannot be rendered.
        {
            String^ message = L"Warning! Positional tracking is " +
                                        sender->Locatability.ToString() + L".\n";
            OutputDebugStringW(message->Data());
        }
        break;

    // In the following three cases, it is still possible to place holograms using a
    // SpatialLocatorAttachedFrameOfReference.
    case SpatialLocatability::PositionalTrackingActivating:
        // The system is preparing to use positional tracking.

    case SpatialLocatability::OrientationOnly:
        // Positional tracking has not been activated.

    case SpatialLocatability::PositionalTrackingInhibited:
        // Positional tracking is temporarily inhibited. User action may be required
        // in order to restore positional tracking.
        break;

    case SpatialLocatability::PositionalTrackingActive:
        // Positional tracking is active. World-locked content can be rendered.
        break;
    }
}

void HoloLensTerrainGenDemoMain::OnCameraAdded(HolographicSpace^ sender, HolographicSpaceCameraAddedEventArgs^ args) {
    Deferral^ deferral = args->GetDeferral();
    HolographicCamera^ holographicCamera = args->Camera;
    create_task([this, deferral, holographicCamera] () {
        //
        // TODO: Allocate resources for the new camera and load any content specific to
        //       that camera. Note that the render target size (in pixels) is a property
        //       of the HolographicCamera object, and can be used to create off-screen
        //       render targets that match the resolution of the HolographicCamera.
        //

        // Create device-based resources for the holographic camera and add it to the list of
        // cameras used for updates and rendering. Notes:
        //   * Since this function may be called at any time, the AddHolographicCamera function
        //     waits until it can get a lock on the set of holographic camera resources before
        //     adding the new camera. At 60 frames per second this wait should not take long.
        //   * A subsequent Update will take the back buffer from the RenderingParameters of this
        //     camera's CameraPose and use it to create the ID3D11RenderTargetView for this camera.
        //     Content can then be rendered for the HolographicCamera.
        m_deviceResources->AddHolographicCamera(holographicCamera);

        // Holographic frame predictions will not include any information about this camera until
        // the deferral is completed.
        deferral->Complete();
    });
}

void HoloLensTerrainGenDemoMain::OnCameraRemoved(HolographicSpace^ sender, HolographicSpaceCameraRemovedEventArgs^ args) {
    create_task([this]() {
        //
        // TODO: Asynchronously unload or deactivate content resources (not back buffer 
        //       resources) that are specific only to the camera that was removed.
        //
    });

    // Before letting this callback return, ensure that all references to the back buffer 
    // are released.
    // Since this function may be called at any time, the RemoveHolographicCamera function
    // waits until it can get a lock on the set of holographic camera resources before
    // deallocating resources for this camera. At 60 frames per second this wait should
    // not take long.
    m_deviceResources->RemoveHolographicCamera(args->Camera);
}

void HoloLensTerrainGenDemoMain::OnPositionalTrackingDeactivating(SpatialLocator^ sender, SpatialLocatorPositionalTrackingDeactivatingEventArgs^ args) {
	// Without positional tracking, spatial meshes will not be locatable.
	args->Canceled = true;
}

void HoloLensTerrainGenDemoMain::OnSurfacesChanged(SpatialSurfaceObserver^ sender, Object^ args) {
	IMapView<Guid, SpatialSurfaceInfo^>^ const& surfaceCollection = sender->GetObservedSurfaces();

	// Process surface adds and updates.
	for (const auto& pair : surfaceCollection) {
		auto id = pair->Key;
		auto surfaceInfo = pair->Value;

		// Choose whether to add, or update the surface.
		// In this example, new surfaces are treated differently by highlighting them in a different
		// color. This allows you to observe changes in the spatial map that are due to new meshes,
		// as opposed to mesh updates.
		// In your app, you might choose to process added surfaces differently than updated
		// surfaces. For example, you might prioritize processing of added surfaces, and
		// defer processing of updates to existing surfaces.
		if (m_meshRenderer->HasSurface(id))	{
			if (m_meshRenderer->GetLastUpdateTime(id).UniversalTime < surfaceInfo->UpdateTime.UniversalTime) {
				// Update existing surface.
				m_meshRenderer->UpdateSurface(id, surfaceInfo);
			}
		} else {
			// New surface.
			m_meshRenderer->AddSurface(id, surfaceInfo);
		}
	}

	// Sometimes, a mesh will fall outside the area that is currently visible to
	// the surface observer. In this code sample, we "sleep" any meshes that are
	// not included in the surface collection to avoid rendering them.
	// The system can including them in the collection again later, in which case
	// they will no longer be hidden.
	m_meshRenderer->HideInactiveMeshes(surfaceCollection);

	// The HolographicFrame has information that the app needs in order
	// to update and render the current frame. The app begins each new
	// frame by calling CreateNextFrame.
	HolographicFrame^ holographicFrame = m_holographicSpace->CreateNextFrame();

	// Get a prediction of where holographic cameras will be when this frame
	// is presented.
	HolographicFramePrediction^ prediction = holographicFrame->CurrentPrediction;
	SpatialCoordinateSystem^ currentCoordinateSystem = m_referenceFrame->GetStationaryCoordinateSystemAtTimestamp(prediction->Timestamp);
	// use it to find all surface planes and pass them to the planeRenderer.
	// this is quite slow, so it is done asynchronously.
	auto getPlanesTask = create_task([this, currentCoordinateSystem] {
		auto planes = m_meshRenderer->GetPlanes(currentCoordinateSystem);
		m_planeRenderer->UpdatePlanes(planes, currentCoordinateSystem);
	});
}

void HoloLensTerrainGenDemoMain::OnInteractionDetected(SpatialInteractionManager^ sender, SpatialInteractionDetectedEventArgs^ args) {
	// if the terrain does not exist, check whether the interaction was with a surface plane.
	// if the terrain does exist, check whether the interaction was with the terrain.
	// in either case, if the interaction was not with the object, check it with the guiManager.
	if (!m_terrain) {
		if (!m_planeRenderer->CaptureInteraction(args->Interaction)) {
			m_guiManager->CaptureInteraction(args->Interaction);
		}
	} else {
		if (!m_terrain->CaptureInteraction(args->Interaction)) {
			m_guiManager->CaptureInteraction(args->Interaction);
		}
	}
}