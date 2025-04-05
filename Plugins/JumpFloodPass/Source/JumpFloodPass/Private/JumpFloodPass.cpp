// Copyright Epic Games, Inc. All Rights Reserved.

#include "JumpFloodPass.h"
#include "JumpFloodPassSettings.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "FJumpFloodPassModule"

void FJumpFloodPassModule::StartupModule()
{
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("JumpFloodPass"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/JumpFloodPass"), PluginShaderDir);
}

void FJumpFloodPassModule::ShutdownModule()
{

}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FJumpFloodPassModule, JumpFloodPass)