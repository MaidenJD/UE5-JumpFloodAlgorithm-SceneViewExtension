// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameterStruct.h"
#include "SceneTextureParameters.h"

BEGIN_SHADER_PARAMETER_STRUCT(FJumpFloodCopyPassParams, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, InSource)
	SHADER_PARAMETER_SAMPLER(SamplerState, InSourceSampler)
	SHADER_PARAMETER(FVector2f, InDestinationResolution)
	RENDER_TARGET_BINDING_SLOTS() // Holds our output
END_SHADER_PARAMETER_STRUCT()

class FJumpFloodCopyPassPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FJumpFloodCopyPassPS);
	using FParameters = FJumpFloodCopyPassParams;
	SHADER_USE_PARAMETER_STRUCT(FJumpFloodCopyPassPS, FGlobalShader);
};