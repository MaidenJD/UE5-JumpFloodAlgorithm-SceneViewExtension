// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "JFA_GameInstance.generated.h"

class FJFA_SceneViewExtension;

UCLASS()
class JUMPFLOODALGORITHM_API UJFA_GameInstance final : public UGameInstance
{
	GENERATED_BODY()

public:

	void Init() override;

private:

	TSharedPtr<FJFA_SceneViewExtension, ESPMode::ThreadSafe> SceneViewExtension;

};
