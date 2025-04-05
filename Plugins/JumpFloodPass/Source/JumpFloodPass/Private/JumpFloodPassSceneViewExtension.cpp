#include "JumpFloodPassSceneViewExtension.h"
#include "JumpFloodPassSettings.h"

#include "DynamicResolutionState.h"
#include "PixelShaderUtils.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/PostProcessMaterial.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphEvent.h"
#include "RenderTargetPool.h"
#include "RHI.h"
#include "SceneRendering.h"
#include "SceneTexturesConfig.h"
#include "SceneTextureParameters.h"
#include "SceneView.h"
#include "ScreenPass.h"

TAutoConsoleVariable<float> CVarJumpFloodRenderScale(
	TEXT("r.JumpFloodPass.RenderScale"),
	1.0f,
	TEXT("Value to scale Jump Flooding render textures by. 1.0 scale is used when value is <= 0\n"),
	ECVF_RenderThreadSafe);

BEGIN_SHADER_PARAMETER_STRUCT(FJumpFloodPassParams,)
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)

	SHADER_PARAMETER(FVector2f, ViewportSize)
	SHADER_PARAMETER(FVector2f, TextureSize)
	SHADER_PARAMETER(FVector2f, TextureSizeInverse)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SourceTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, SourceSampler)

	SHADER_PARAMETER(float, FloodStepSize)

	SHADER_PARAMETER(FVector2f, CopyDestinationResolution)

	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FJumpFloodSeedPassPS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FJumpFloodSeedPassPS, Global, );
	using FParameters = FJumpFloodPassParams;
	SHADER_USE_PARAMETER_STRUCT(FJumpFloodSeedPassPS, FGlobalShader);
};

IMPLEMENT_SHADER_TYPE(, FJumpFloodSeedPassPS, TEXT("/Plugin/JumpFloodPass/Private/JumpFloodPass.usf"), TEXT("SeedPS"), SF_Pixel);

class FJumpFloodFloodPassPS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FJumpFloodFloodPassPS, Global, );
	using FParameters = FJumpFloodPassParams;
	SHADER_USE_PARAMETER_STRUCT(FJumpFloodFloodPassPS, FGlobalShader);
};

IMPLEMENT_SHADER_TYPE(, FJumpFloodFloodPassPS, TEXT("/Plugin/JumpFloodPass/Private/JumpFloodPass.usf"), TEXT("FloodPS"), SF_Pixel);

class FJumpFloodCopyPassPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FJumpFloodCopyPassPS);
	using FParameters = FJumpFloodPassParams;
	SHADER_USE_PARAMETER_STRUCT(FJumpFloodCopyPassPS, FGlobalShader);
};

IMPLEMENT_SHADER_TYPE(, FJumpFloodCopyPassPS, TEXT("/Plugin/JumpFloodPass/Private/JumpFloodPass.usf"), TEXT("CopyPS"), SF_Pixel);

bool FJumpFloodPassSceneViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	return UJumpFloodPassSettings::IsEnabled() && IsValid(RenderTarget);
}

void FJumpFloodPassSceneViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	FIntRect ViewRect = InView.UnconstrainedViewRect;

	const ISceneViewFamilyScreenPercentage* ScreenPercentageInterface = InViewFamily.GetScreenPercentageInterface();
	if (ScreenPercentageInterface)
	{
		DynamicRenderScaling::TMap<float> UpperBounds = ScreenPercentageInterface->GetResolutionFractionsUpperBound();
		const float ScreenPercentage = UpperBounds[GDynamicPrimaryResolutionFraction] * InViewFamily.SecondaryViewFraction;

		ViewRect = ViewRect.Scale(ScreenPercentage);
		FIntPoint ViewSize { ViewRect.Width(), ViewRect.Height() };

		if (RenderTarget->GetSurfaceWidth() != ViewSize.X || RenderTarget->GetSurfaceHeight() != ViewSize.Y)
		{
			RenderTarget->InitAutoFormat(ViewSize.X, ViewSize.Y);
			bShouldRecreatePooledRenderTarget = true;
		}
	}
}

void FJumpFloodPassSceneViewExtension::PostRenderBasePassDeferred_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView, const FRenderTargetBindingSlots& RenderTargets, TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures)
{
	checkSlow(InView.bIsViewInfo);
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(InView);

	FSceneViewExtensionBase::PostRenderBasePassDeferred_RenderThread(GraphBuilder, InView, RenderTargets, SceneTextures);

	if(!IsValid(RenderTarget))
	{
		return;
	}

	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.JumpFloodPass.RenderScale"));
	float RenderScale = CVar->GetValueOnRenderThread() > 0.0f ? CVar->GetValueOnRenderThread() : 1.0f;

	const FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

	RDG_EVENT_SCOPE(GraphBuilder, "Jump Flood");

	if (bShouldRecreatePooledRenderTarget)
	{
		CreatePooledRenderTarget_RenderThread();
	}

	FRDGTextureRef RenderTargetTexture = GraphBuilder.RegisterExternalTexture(PooledRenderTarget, TEXT("Jump Flood Render Target"));
	const FIntRect RenderViewport = FIntRect(0, 0, RenderTargetTexture->Desc.Extent.X, RenderTargetTexture->Desc.Extent.Y);

	FRDGTextureDesc IntermediateTargetDesc = RenderTargetTexture->Desc;
	IntermediateTargetDesc.Extent =
		FIntPoint {
			(int32) ((float) IntermediateTargetDesc.Extent.X * RenderScale),
			(int32) ((float) IntermediateTargetDesc.Extent.Y * RenderScale) };

	const FIntRect IntermediateViewport = FIntRect(0, 0, IntermediateTargetDesc.Extent.X, IntermediateTargetDesc.Extent.Y);

	FRDGTextureRef JumpFloodTextures[] = {
		GraphBuilder.CreateTexture(IntermediateTargetDesc, TEXT("Jump Flood Intermediate Target A")),
		GraphBuilder.CreateTexture(IntermediateTargetDesc, TEXT("Jump Flood Intermediate Target B")),
	};

	int32 ReadIndex  = 0;
	int32 WriteIndex = 1;

	//  Init Pass
	{
		FJumpFloodSeedPassPS::FParameters* Parameters = GraphBuilder.AllocParameters<FJumpFloodSeedPassPS::FParameters>();
		Parameters->TextureSize = JumpFloodTextures[WriteIndex]->Desc.Extent;
		Parameters->TextureSizeInverse = FVector2f(1.0f, 1.0f) / Parameters->TextureSize;
		Parameters->ViewportSize = RenderViewport.Size();
		Parameters->View = InView.ViewUniformBuffer;
		Parameters->SceneTextures = CreateSceneTextureShaderParameters(GraphBuilder, ViewInfo.GetSceneTexturesChecked(), InView.GetFeatureLevel(), ESceneTextureSetupMode::All);

		// We're going to also clear the render target
		Parameters->RenderTargets[0] = FRenderTargetBinding(JumpFloodTextures[WriteIndex], ERenderTargetLoadAction::EClear);

		TShaderMapRef<FJumpFloodSeedPassPS> PixelShader(GlobalShaderMap);
		FPixelShaderUtils::AddFullscreenPass(GraphBuilder, GlobalShaderMap, FRDGEventName(TEXT("Jump Flood: Seed")), PixelShader, Parameters, IntermediateViewport);
	}

	//  Flood Passes
	{
		float LargestSide = FMath::Max(IntermediateViewport.Width(), IntermediateViewport.Height());
		float LargestSideInverse = 1.0f / LargestSide;

		//Adding a 1-step pass before full flood Reduces error rate
		Swap(ReadIndex, WriteIndex);
		AddFloodPass_RenderThread(
			GraphBuilder,
			GlobalShaderMap,
			ViewInfo,
			IntermediateViewport,
			JumpFloodTextures[ReadIndex],
			JumpFloodTextures[WriteIndex],
			0,
			LargestSideInverse);
			
		int FloodPassCount = FMath::Log2(LargestSide);
		for (int FloodExponent = FloodPassCount; FloodExponent > -1 ; FloodExponent -= 1)
		{
			Swap(ReadIndex, WriteIndex);
			AddFloodPass_RenderThread(
				GraphBuilder,
				GlobalShaderMap,
				ViewInfo,
				IntermediateViewport,
				JumpFloodTextures[ReadIndex],
				JumpFloodTextures[WriteIndex],
				FloodExponent,
				LargestSideInverse);
		}
	}

	//  Final stretched copy pass
	{
		FJumpFloodCopyPassPS::FParameters* Parameters = GraphBuilder.AllocParameters<FJumpFloodCopyPassPS::FParameters>();
		Parameters->SourceTexture = JumpFloodTextures[WriteIndex];
		Parameters->SourceSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		Parameters->CopyDestinationResolution = RenderViewport.Size();

		Parameters->RenderTargets[0] = FRenderTargetBinding(RenderTargetTexture, ERenderTargetLoadAction::EClear);

		TShaderMapRef<FJumpFloodCopyPassPS> PixelShader(GlobalShaderMap);
		FPixelShaderUtils::AddFullscreenPass(GraphBuilder, GlobalShaderMap, FRDGEventName(TEXT("Jump Flood: Copy")), PixelShader, Parameters, RenderViewport);
	}
}

void FJumpFloodPassSceneViewExtension::AddFloodPass_RenderThread(
	FRDGBuilder& GraphBuilder,
	const FGlobalShaderMap* GlobalShaderMap,
	const FViewInfo& ViewInfo,
	const FIntRect& IntermediateViewport,
	const FRDGTextureRef& ReadTexture,
	const FRDGTextureRef& WriteTexture,
	int32 FloodExponent,
	float ExponentToUVScaler)
{
	FJumpFloodFloodPassPS::FParameters* Parameters = GraphBuilder.AllocParameters<FJumpFloodFloodPassPS::FParameters>();
	Parameters->View = ViewInfo.ViewUniformBuffer;
	Parameters->SceneTextures = CreateSceneTextureShaderParameters(GraphBuilder, ViewInfo.GetSceneTexturesChecked(), ViewInfo.GetFeatureLevel(), ESceneTextureSetupMode::All);
	Parameters->TextureSize = ReadTexture->Desc.Extent;
	Parameters->TextureSizeInverse = FVector2f(1.0f, 1.0f) / Parameters->TextureSize;
	Parameters->SourceTexture = ReadTexture;
	Parameters->SourceSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	Parameters->FloodStepSize = ((float) (1 << FloodExponent)) * ExponentToUVScaler;

	Parameters->RenderTargets[0] = FRenderTargetBinding(WriteTexture, ERenderTargetLoadAction::ELoad);

	TShaderMapRef<FJumpFloodFloodPassPS> PixelShader(GlobalShaderMap);
	FPixelShaderUtils::AddFullscreenPass(GraphBuilder, GlobalShaderMap, FRDGEventName(TEXT("Jump Flood: Flood %d"), (1 << FloodExponent)), PixelShader, Parameters, IntermediateViewport);
}

void FJumpFloodPassSceneViewExtension::CreatePooledRenderTarget_RenderThread()
{
	checkf(IsInRenderingThread() || IsInRHIThread(), TEXT("Cannot create from outside the rendering thread"));

	// Render target resources require the render thread
	const FTextureRenderTargetResource* RenderTargetResource = RenderTarget->GetRenderTargetResource();
	const FTexture2DRHIRef RenderTargetRHI = RenderTargetResource->GetRenderTargetTexture();

	FSceneRenderTargetItem RenderTargetItem;
	RenderTargetItem.TargetableTexture = RenderTargetRHI;
	RenderTargetItem.ShaderResourceTexture = RenderTargetRHI;

	// Flags allow it to be used as a render target, shader resource, UAV
	FPooledRenderTargetDesc RenderTargetDesc = FPooledRenderTargetDesc::Create2DDesc(RenderTargetResource->GetSizeXY(), RenderTargetRHI->GetDesc().Format, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_UAV, TexCreate_None, false);

	GRenderTargetPool.CreateUntrackedElement(RenderTargetDesc, PooledRenderTarget, RenderTargetItem);
}