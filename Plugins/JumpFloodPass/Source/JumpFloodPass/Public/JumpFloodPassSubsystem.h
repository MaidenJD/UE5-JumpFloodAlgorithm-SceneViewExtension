// Copyright James Maiden. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "JumpFloodPassSubsystem.generated.h"

class FJumpFloodPassSceneViewExtension;

UCLASS()
class JUMPFLOODPASS_API UJumpFloodPassSubsystem final : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	//~ Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override final;
	virtual void Deinitialize() override final;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override final;
	//~ End USubsystem Interface

protected:

	//~ Begin UWorldSubsystem Interface
	bool DoesSupportWorldType(const EWorldType::Type WorldType) const override final;
	//~ End UWorldSubsystem Interface

private:

	TSharedPtr<FJumpFloodPassSceneViewExtension, ESPMode::ThreadSafe> SceneViewExtension;

};
