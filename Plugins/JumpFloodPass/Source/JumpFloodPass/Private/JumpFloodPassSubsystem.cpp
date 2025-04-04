// Copyright James Maiden. All Rights Reserved.
#include "JumpFloodPassSubsystem.h"
#include "JumpFloodPassSettings.h"
#include "JumpFloodPassSceneViewExtension.h"

#include "Kismet/KismetRenderingLibrary.h"

bool UJumpFloodPassSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	return UJumpFloodPassSettings::IsEnabled();
}

bool UJumpFloodPassSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UJumpFloodPassSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UTextureRenderTarget2D* RenderTarget = UJumpFloodPassSettings::GetRenderTarget().LoadSynchronous();
	SceneViewExtension = FSceneViewExtensions::NewExtension<FJumpFloodPassSceneViewExtension>(RenderTarget);
}

void UJumpFloodPassSubsystem::Deinitialize()
{
	Super::Deinitialize();

	UTextureRenderTarget2D* RenderTarget = UJumpFloodPassSettings::GetRenderTarget().Get();
	if (RenderTarget)
	{
		UKismetRenderingLibrary::ClearRenderTarget2D(this, RenderTarget, FLinearColor::Transparent);
	}
}
