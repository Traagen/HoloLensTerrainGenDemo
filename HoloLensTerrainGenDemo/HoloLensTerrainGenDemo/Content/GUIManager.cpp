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
#include "GUIManager.h"
#include <functional>

using namespace HoloLensTerrainGenDemo;

using namespace Windows::Foundation;
using namespace Windows::UI::Input::Spatial;
using namespace std::placeholders;

// Creates and initializes a GestureRecognizer that listens to a Person.
GUIManager::GUIManager() {
	// Set up a general gesture recognizer for input.
	m_gestureRecognizer = ref new SpatialGestureRecognizer(SpatialGestureSettings::DoubleTap | SpatialGestureSettings::Tap | SpatialGestureSettings::Hold);

	m_tapGestureEventToken =
		m_gestureRecognizer->Tapped +=
		ref new Windows::Foundation::TypedEventHandler<SpatialGestureRecognizer^, SpatialTappedEventArgs^>(
			std::bind(&GUIManager::OnTap, this, _1, _2)
			);

	m_holdGestureCompletedEventToken = 
		m_gestureRecognizer->HoldCompleted += 
		ref new Windows::Foundation::TypedEventHandler<SpatialGestureRecognizer^, SpatialHoldCompletedEventArgs^>(
			std::bind(&GUIManager::OnHoldCompleted, this, _1, _2)
		);
}

GUIManager::~GUIManager() {
	if (m_gestureRecognizer) {
		m_gestureRecognizer->Tapped -= m_tapGestureEventToken;
		m_gestureRecognizer->HoldCompleted -= m_holdGestureCompletedEventToken;
	}
}

void GUIManager::OnTap(SpatialGestureRecognizer^ sender, SpatialTappedEventArgs^ args) {
	switch (args->TapCount) {
	case 1:
		renderWireframe = !renderWireframe;
		break;
	case 2:
		renderSurfaces = !renderSurfaces;
		break;
	}
}

void GUIManager::OnHoldCompleted(SpatialGestureRecognizer^ sender,	SpatialHoldCompletedEventArgs^ args) {
	// This will later be related to bringing up the GUI. Either on Hold Started or on Hold Completed.
}

void GUIManager::CaptureInteraction(SpatialInteraction^ interaction) {
	m_gestureRecognizer->CaptureInteraction(interaction);
}