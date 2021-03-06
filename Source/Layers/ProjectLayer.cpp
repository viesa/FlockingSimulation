﻿#include "ProjectLayer.h"

namespace Se
{
void ProjectLayer::OnAttach(Shared<Batch> &loader)
{
	BaseLayer::OnAttach(loader);

	loader->Submit([this]
				   {
					   _boidManager = CreateUnique<BoidManager>(_camera);
				   }, "Creating Boid Manager");

	_controllableRenderTexture.SetClearColor({ 10, 10, 10 });
}

void ProjectLayer::OnDetach()
{
	BaseLayer::OnDetach();
}

void ProjectLayer::OnUpdate()
{
	BaseLayer::OnUpdate();

	_boidManager->OnUpdate();
	_boidManager->OnRender(_scene);
}

void ProjectLayer::OnGuiRender()
{
	BaseLayer::OnGuiRender();

	if ( ImGui::Begin("Project") )
	{
		_boidManager->OnGuiRender();
	}
	ImGui::End();
}

void ProjectLayer::OnRenderTargetResize(const sf::Vector2f &newSize)
{
	BaseLayer::OnRenderTargetResize(newSize);
	_scene.OnRenderTargetResize(newSize);
	if ( _firstRenderTargetResize )
	{
		_boidManager->OnFirstRenderTargetResize(newSize);
		_firstRenderTargetResize = false;
	}
}
}
