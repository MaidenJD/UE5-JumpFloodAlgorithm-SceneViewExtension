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

	UTextureRenderTarget2D* PrimaryRenderTarget = UJumpFloodPassSettings::GetPrimaryRenderTarget().LoadSynchronous();
	UTextureRenderTarget2D* SecondaryRenderTarget = UJumpFloodPassSettings::GetSecondaryRenderTarget().LoadSynchronous();
	SceneViewExtension = FSceneViewExtensions::NewExtension<FJumpFloodPassSceneViewExtension>(PrimaryRenderTarget, SecondaryRenderTarget);
}

void UJumpFloodPassSubsystem::Deinitialize()
{
	Super::Deinitialize();

	UTextureRenderTarget2D* PrimaryRenderTarget = UJumpFloodPassSettings::GetPrimaryRenderTarget().LoadSynchronous();
	if (PrimaryRenderTarget)
	{
		UKismetRenderingLibrary::ClearRenderTarget2D(this, PrimaryRenderTarget, FLinearColor::Transparent);
	}

	UTextureRenderTarget2D* SecondaryRenderTarget = UJumpFloodPassSettings::GetSecondaryRenderTarget().LoadSynchronous();
	if (SecondaryRenderTarget)
	{
		UKismetRenderingLibrary::ClearRenderTarget2D(this, SecondaryRenderTarget, FLinearColor::Transparent);
	}
}
