// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameterStruct.h"
#include "SceneTextureParameters.h"

BEGIN_SHADER_PARAMETER_STRUCT(FJumpFloodSeedPassParams,)
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)
	SHADER_PARAMETER(FVector2f, TextureSize)
	SHADER_PARAMETER(FVector2f, TextureSizeInverse)
	SHADER_PARAMETER(FVector2f, ViewportSize)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FJumpFloodSeedPassPS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FJumpFloodSeedPassPS, Global, );
	using FParameters = FJumpFloodSeedPassParams;
	SHADER_USE_PARAMETER_STRUCT(FJumpFloodSeedPassPS, FGlobalShader);
};