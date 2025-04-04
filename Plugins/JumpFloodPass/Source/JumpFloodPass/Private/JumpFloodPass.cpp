// Copyright Epic Games, Inc. All Rights Reserved.

#include "JumpFloodPass.h"
#include "JumpFloodPassSettings.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "FJumpFloodPassModule"

void FJumpFloodPassModule::StartupModule()
{
	CreateJumpFloodPassSettings();
}

void FJumpFloodPassModule::ShutdownModule()
{

}

void FJumpFloodPassModule::CreateJumpFloodPassSettings()
{
	// JumpFloodPassSettings = NewObject<UJumpFloodPassSettings>(GetTransientPackage(), UJumpFloodPassSettings::StaticClass());
	// check(JumpFloodPassSettings);
	// JumpFloodPassSettings->AddToRoot();

	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("JumpFloodPass"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/JumpFloodPass"), PluginShaderDir);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FJumpFloodPassModule, JumpFloodPass)