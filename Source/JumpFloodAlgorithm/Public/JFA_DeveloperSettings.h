// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/TextureRenderTarget2D.h"
#include "JFA_DeveloperSettings.generated.h"


/**
 * 
 */
UCLASS(Config = Game, DefaultConfig, DisplayName = "Jump Flood Algorithm")
class JUMPFLOODALGORITHM_API UJFA_DeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	static bool IsEnabled() { return GetDefault<ThisClass>()->bEnabled; }

	static TSoftObjectPtr<UTextureRenderTarget2D> GetRenderTarget() { return GetDefault<ThisClass>()->RenderTarget; }

private:

	UPROPERTY(Config, EditAnywhere)
	bool bEnabled = true;

	UPROPERTY(Config, EditAnywhere)
	TSoftObjectPtr<UTextureRenderTarget2D> RenderTarget;

};
