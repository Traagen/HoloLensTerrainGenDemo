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
#include "SpatialInputHandler.h"
#include <functional>

using namespace HoloLensTerrainGenDemo;

using namespace Windows::Foundation;
using namespace Windows::UI::Input::Spatial;
using namespace std::placeholders;

// Creates and initializes a GestureRecognizer that listens to a Person.
SpatialInputHandler::SpatialInputHandler()
{
	// The interaction manager provides an event that informs the app when
	// spatial interactions are detected.
	m_interactionManager = SpatialInteractionManager::GetForCurrentView();

	// Bind a handler to the SourcePressed event.
	m_sourcePressedEventToken =
		m_interactionManager->SourcePressed +=
		ref new TypedEventHandler<SpatialInteractionManager^, SpatialInteractionSourceEventArgs^>(
			bind(&SpatialInputHandler::OnSourcePressed, this, _1, _2)
			);

	m_interactionDetectedEventToken = m_interactionManager->InteractionDetected +=
		ref new TypedEventHandler<SpatialInteractionManager^, SpatialInteractionDetectedEventArgs^>(
			bind(&SpatialInputHandler::OnInteractionDetected, this, _1, _2)
			);

	// Set up a general gesture recognizer for input.
	m_gestureRecognizer = ref new SpatialGestureRecognizer(SpatialGestureSettings::DoubleTap | SpatialGestureSettings::Tap | SpatialGestureSettings::Hold);

	m_tapGestureEventToken =
		m_gestureRecognizer->Tapped +=
		ref new Windows::Foundation::TypedEventHandler<SpatialGestureRecognizer^, SpatialTappedEventArgs^>(
			std::bind(&SpatialInputHandler::OnTap, this, _1, _2)
			);

	m_holdGestureCompletedEventToken = 
		m_gestureRecognizer->HoldCompleted += 
		ref new Windows::Foundation::TypedEventHandler<SpatialGestureRecognizer^, SpatialHoldCompletedEventArgs^>(
			std::bind(&SpatialInputHandler::OnHoldCompleted, this, _1, _2)
		);
}

SpatialInputHandler::~SpatialInputHandler()
{
	// Unregister our handler for the OnSourcePressed event.
	m_interactionManager->SourcePressed -= m_sourcePressedEventToken;
	m_interactionManager->InteractionDetected -= m_interactionDetectedEventToken;

	if (m_gestureRecognizer) {
		m_gestureRecognizer->Tapped -= m_tapGestureEventToken;
		m_gestureRecognizer->HoldCompleted -= m_holdGestureCompletedEventToken;
	}
}

// Checks if the user performed an input gesture since the last call to this method.
// Allows the main update loop to check for asynchronous changes to the user
// input state.
SpatialInteractionSourceState^ SpatialInputHandler::CheckForInput()
{
	SpatialInteractionSourceState^ sourceState = m_sourceState;
	m_sourceState = nullptr;
	return sourceState;
}

unsigned short SpatialInputHandler::CheckForTap() {
	unsigned short numTaps = m_numTaps;
	m_numTaps = 0;

	return numTaps;
}

bool SpatialInputHandler::CheckForHold() {
	bool hold = m_holdCompleted;
	m_holdCompleted = false;
	return hold;
}

void SpatialInputHandler::OnSourcePressed(SpatialInteractionManager^ sender, SpatialInteractionSourceEventArgs^ args)
{
	m_sourceState = args->State;
}

void SpatialInputHandler::OnInteractionDetected(Windows::UI::Input::Spatial::SpatialInteractionManager^ sender, 
	Windows::UI::Input::Spatial::SpatialInteractionDetectedEventArgs^ args) {
	m_gestureRecognizer->CaptureInteraction(args->Interaction);
}

void SpatialInputHandler::OnTap(SpatialGestureRecognizer^ sender, SpatialTappedEventArgs^ args) {
	m_numTaps = args->TapCount;
}

void SpatialInputHandler::OnHoldCompleted(Windows::UI::Input::Spatial::SpatialGestureRecognizer^ sender,
	Windows::UI::Input::Spatial::SpatialHoldCompletedEventArgs^ args) {
	m_holdCompleted = true;
}