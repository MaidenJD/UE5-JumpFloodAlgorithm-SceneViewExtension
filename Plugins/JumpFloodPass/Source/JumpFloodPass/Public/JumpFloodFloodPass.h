// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameterStruct.h"
#include "SceneTextureParameters.h"

BEGIN_SHADER_PARAMETER_STRUCT(FJumpFloodFloodPassParams,)
	SHADER_PARAMETER(FVector2f, TextureSize)
	SHADER_PARAMETER(FVector2f, TextureSizeInverse)
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)
	SHADER_PARAMETER_SAMPLER(SamplerState, JFASampler)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, JFATexture)
	SHADER_PARAMETER(float, StepSize)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FJumpFloodFloodPassPS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FJumpFloodFloodPassPS, Global, );
	using FParameters = FJumpFloodFloodPassParams;
	SHADER_USE_PARAMETER_STRUCT(FJumpFloodFloodPassPS, FGlobalShader);
};