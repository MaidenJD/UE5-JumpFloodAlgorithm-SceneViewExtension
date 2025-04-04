// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class UJumpFloodPassSettings;

class FJumpFloodPassModule : public IModuleInterface
{

public:

	//~ Begin IModuleInterface */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface */

private:

	void CreateJumpFloodPassSettings();

private:

	TObjectPtr<UJumpFloodPassSettings> JumpFloodPassSettings = NULL;

};
