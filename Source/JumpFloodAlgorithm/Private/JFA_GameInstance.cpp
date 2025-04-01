// Fill out your copyright notice in the Description page of Project Settings.


#include "JFA_GameInstance.h"
#include "JFA_SceneViewExtension.h"
#include "JFA_DeveloperSettings.h"

void UJFA_GameInstance::Init()
{
	Super::Init();

	UTextureRenderTarget2D* RenderTarget = UJFA_DeveloperSettings::GetRenderTarget().LoadSynchronous();
	SceneViewExtension = FSceneViewExtensions::NewExtension<FJFA_SceneViewExtension>(RenderTarget);
}