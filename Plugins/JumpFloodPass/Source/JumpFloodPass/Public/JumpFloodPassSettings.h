// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/TextureRenderTarget2D.h"
#include "JumpFloodPassSettings.generated.h"


/**
 * 
 */
UCLASS(Config = JumpFloodPass, DefaultConfig, DisplayName = "Jump Flood Pass")
class JUMPFLOODPASS_API UJumpFloodPassSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	static bool IsEnabled() { return GetDefault<ThisClass>()->bEnabled; }
	static TSoftObjectPtr<UTextureRenderTarget2D> GetRenderTarget() { return GetDefault<ThisClass>()->RenderTarget; }

	//~ Begin UDeveloperSettings Interface
	virtual FName GetContainerName() const override final { return FName("Project"); }
	virtual FName GetCategoryName() const override final { return FName("Plugins"); }
	virtual FName GetSectionName() const override final { return FName("JumpFloodPass"); }
	//~ End UDeveloperSettings Interface

private:

	UPROPERTY(Config, EditAnywhere)
	bool bEnabled = true;

	UPROPERTY(Config, EditAnywhere)
	TSoftObjectPtr<UTextureRenderTarget2D> RenderTarget;

};
