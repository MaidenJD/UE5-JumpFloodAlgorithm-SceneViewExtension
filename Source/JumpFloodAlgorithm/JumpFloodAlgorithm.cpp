// Copyright Epic Games, Inc. All Rights Reserved.

#include "JumpFloodAlgorithm.h"
#include "Modules/ModuleManager.h"

class FJumpFloodAlgorithmGameModule final : public IModuleInterface
{

public:

	void StartupModule() override
	{
		const FString ProjectShaderDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Shaders"));
		if(!AllShaderSourceDirectoryMappings().Contains(TEXT("/JFA_Shaders")))
		{
			AddShaderSourceDirectoryMapping(TEXT("/JFA_Shaders"), ProjectShaderDir);
		}
	}
};


IMPLEMENT_PRIMARY_GAME_MODULE(FJumpFloodAlgorithmGameModule, JumpFloodAlgorithm, "JumpFloodAlgorithm");