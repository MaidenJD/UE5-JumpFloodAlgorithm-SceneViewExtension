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
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrimaryTexture)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SecondaryTexture)

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
	return UJumpFloodPassSettings::IsEnabled()
		&& IsValid(PrimaryRenderTarget)
		&& IsValid(SecondaryRenderTarget);
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

		if (PrimaryRenderTarget->GetSurfaceWidth() != ViewSize.X || PrimaryRenderTarget->GetSurfaceHeight() != ViewSize.Y)
		{
			PrimaryRenderTarget->InitAutoFormat(ViewSize.X, ViewSize.Y);
			SecondaryRenderTarget->InitAutoFormat(ViewSize.X, ViewSize.Y);

			bShouldRecreatePooledRenderTargets = true;
		}
	}
}

void FJumpFloodPassSceneViewExtension::PostRenderBasePassDeferred_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView, const FRenderTargetBindingSlots& RenderTargets, TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures)
{
	checkSlow(InView.bIsViewInfo);
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(InView);

	FSceneViewExtensionBase::PostRenderBasePassDeferred_RenderThread(GraphBuilder, InView, RenderTargets, SceneTextures);

	if (!IsActiveThisFrame_Internal(FSceneViewExtensionContext{}))
	{
		return;
	}

	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.JumpFloodPass.RenderScale"));
	float RenderScale = CVar->GetValueOnRenderThread() > 0.0f ? CVar->GetValueOnRenderThread() : 1.0f;

	const FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

	RDG_EVENT_SCOPE(GraphBuilder, "JumpFlood");

	if (bShouldRecreatePooledRenderTargets)
	{
		CreatePooledRenderTargets_RenderThread();
	}

	FRDGTextureRef PrimaryRenderTargetTexture = GraphBuilder.RegisterExternalTexture(PooledPrimaryRenderTarget, TEXT("JumpFloodTarget_0"));
	FRDGTextureRef SecondaryRenderTargetTexture = GraphBuilder.RegisterExternalTexture(PooledSecondaryRenderTarget, TEXT("JumpFloodTarget_1"));
	const FIntRect RenderViewport = FIntRect(0, 0, PrimaryRenderTargetTexture->Desc.Extent.X, PrimaryRenderTargetTexture->Desc.Extent.Y);

	FRDGTextureDesc IntermediateTargetDesc = PrimaryRenderTargetTexture->Desc;
	IntermediateTargetDesc.ClearValue = FClearValueBinding::Transparent;
	IntermediateTargetDesc.Extent =
		FIntPoint {
			(int32) ((float) IntermediateTargetDesc.Extent.X * RenderScale),
			(int32) ((float) IntermediateTargetDesc.Extent.Y * RenderScale) };

	const FIntRect IntermediateViewport = FIntRect(0, 0, IntermediateTargetDesc.Extent.X, IntermediateTargetDesc.Extent.Y);

	FRDGTextureRef PrimaryTextures[] = {
		GraphBuilder.CreateTexture(IntermediateTargetDesc, TEXT("JumpFloodIntermediateTarget_0_0")),
		GraphBuilder.CreateTexture(IntermediateTargetDesc, TEXT("JumpFloodIntermediateTarget_0_1")),
	};

	FRDGTextureRef SecondaryTextures[] = {
		GraphBuilder.CreateTexture(IntermediateTargetDesc, TEXT("JumpFloodIntermediateTarget_1_0")),
		GraphBuilder.CreateTexture(IntermediateTargetDesc, TEXT("JumpFloodIntermediateTarget_1_1")),
	};

	int32 ReadIndex  = 0;
	int32 WriteIndex = 1;

	//  Init Pass
	{
		FJumpFloodSeedPassPS::FParameters* Parameters = GraphBuilder.AllocParameters<FJumpFloodSeedPassPS::FParameters>();
		Parameters->TextureSize = PrimaryTextures[WriteIndex]->Desc.Extent;
		Parameters->TextureSizeInverse = FVector2f(1.0f, 1.0f) / Parameters->TextureSize;
		Parameters->ViewportSize = RenderViewport.Size();
		Parameters->View = InView.ViewUniformBuffer;
		Parameters->SceneTextures = CreateSceneTextureShaderParameters(GraphBuilder, ViewInfo.GetSceneTexturesChecked(), InView.GetFeatureLevel(), ESceneTextureSetupMode::All);

		// We're going to also clear the render target
		Parameters->RenderTargets[0] = FRenderTargetBinding(PrimaryTextures[WriteIndex], ERenderTargetLoadAction::EClear);
		Parameters->RenderTargets[1] = FRenderTargetBinding(SecondaryTextures[WriteIndex], ERenderTargetLoadAction::EClear);

		TShaderMapRef<FJumpFloodSeedPassPS> PixelShader(GlobalShaderMap);
		FPixelShaderUtils::AddFullscreenPass(GraphBuilder, GlobalShaderMap, FRDGEventName(TEXT("JumpFlood - Seed")), PixelShader, Parameters, IntermediateViewport);
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
			PrimaryTextures[ReadIndex],
			PrimaryTextures[WriteIndex],
			SecondaryTextures[ReadIndex],
			SecondaryTextures[WriteIndex],
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
				PrimaryTextures[ReadIndex],
				PrimaryTextures[WriteIndex],
				SecondaryTextures[ReadIndex],
				SecondaryTextures[WriteIndex],
				FloodExponent,
				LargestSideInverse);
		}
	}

	//  Final stretched copy pass
	{
		FJumpFloodCopyPassPS::FParameters* Parameters = GraphBuilder.AllocParameters<FJumpFloodCopyPassPS::FParameters>();
		Parameters->PrimaryTexture = PrimaryTextures[WriteIndex];
		Parameters->SecondaryTexture = SecondaryTextures[WriteIndex];
		Parameters->CopyDestinationResolution = RenderViewport.Size();
		Parameters->View = InView.ViewUniformBuffer;
		Parameters->SceneTextures = CreateSceneTextureShaderParameters(GraphBuilder, ViewInfo.GetSceneTexturesChecked(), InView.GetFeatureLevel(), ESceneTextureSetupMode::All);

		Parameters->RenderTargets[0] = FRenderTargetBinding(PrimaryRenderTargetTexture, ERenderTargetLoadAction::EClear);
		Parameters->RenderTargets[1] = FRenderTargetBinding(SecondaryRenderTargetTexture, ERenderTargetLoadAction::EClear);

		TShaderMapRef<FJumpFloodCopyPassPS> PixelShader(GlobalShaderMap);
		FPixelShaderUtils::AddFullscreenPass(GraphBuilder, GlobalShaderMap, FRDGEventName(TEXT("JumpFlood - Resolve")), PixelShader, Parameters, RenderViewport);
	}
}

void FJumpFloodPassSceneViewExtension::AddFloodPass_RenderThread(
	FRDGBuilder& GraphBuilder,
	const FGlobalShaderMap* GlobalShaderMap,
	const FViewInfo& ViewInfo,
	const FIntRect& IntermediateViewport,
	const FRDGTextureRef& PrimaryReadTexture,
	const FRDGTextureRef& PrimaryWriteTexture,
	const FRDGTextureRef& SecondaryReadTexture,
	const FRDGTextureRef& SecondaryWriteTexture,
	int32 FloodExponent,
	float ExponentToUVScaler)
{
	FJumpFloodFloodPassPS::FParameters* Parameters = GraphBuilder.AllocParameters<FJumpFloodFloodPassPS::FParameters>();
	Parameters->View = ViewInfo.ViewUniformBuffer;
	Parameters->SceneTextures = CreateSceneTextureShaderParameters(GraphBuilder, ViewInfo.GetSceneTexturesChecked(), ViewInfo.GetFeatureLevel(), ESceneTextureSetupMode::All);
	Parameters->TextureSize = PrimaryReadTexture->Desc.Extent;
	Parameters->TextureSizeInverse = FVector2f(1.0f, 1.0f) / Parameters->TextureSize;
	Parameters->PrimaryTexture = PrimaryReadTexture;
	Parameters->SecondaryTexture = SecondaryReadTexture;
	Parameters->FloodStepSize = ((float) (1 << FloodExponent));

	Parameters->RenderTargets[0] = FRenderTargetBinding(PrimaryWriteTexture, ERenderTargetLoadAction::ELoad);
	Parameters->RenderTargets[1] = FRenderTargetBinding(SecondaryWriteTexture, ERenderTargetLoadAction::ELoad);

	TShaderMapRef<FJumpFloodFloodPassPS> PixelShader(GlobalShaderMap);
	FPixelShaderUtils::AddFullscreenPass(GraphBuilder, GlobalShaderMap, FRDGEventName(TEXT("JumpFlood - Flood (%d)"), (1 << FloodExponent)), PixelShader, Parameters, IntermediateViewport);
}

void FJumpFloodPassSceneViewExtension::CreatePooledRenderTargets_RenderThread()
{
	checkf(IsInRenderingThread() || IsInRHIThread(), TEXT("Cannot create from outside the rendering thread"));

	{
		// Render target resources require the render thread
		const FTextureRenderTargetResource* RenderTargetResource = PrimaryRenderTarget->GetRenderTargetResource();
		const FTexture2DRHIRef RenderTargetRHI = RenderTargetResource->GetRenderTargetTexture();

		FSceneRenderTargetItem RenderTargetItem;
		RenderTargetItem.TargetableTexture = RenderTargetRHI;
		RenderTargetItem.ShaderResourceTexture = RenderTargetRHI;

		// Flags allow it to be used as a render target, shader resource, UAV
		FPooledRenderTargetDesc RenderTargetDesc = FPooledRenderTargetDesc::Create2DDesc(RenderTargetResource->GetSizeXY(), RenderTargetRHI->GetDesc().Format, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_UAV, TexCreate_None, false);

		GRenderTargetPool.CreateUntrackedElement(RenderTargetDesc, PooledPrimaryRenderTarget, RenderTargetItem);
	}

	{
		// Render target resources require the render thread
		const FTextureRenderTargetResource* RenderTargetResource = SecondaryRenderTarget->GetRenderTargetResource();
		const FTexture2DRHIRef RenderTargetRHI = RenderTargetResource->GetRenderTargetTexture();

		FSceneRenderTargetItem RenderTargetItem;
		RenderTargetItem.TargetableTexture = RenderTargetRHI;
		RenderTargetItem.ShaderResourceTexture = RenderTargetRHI;

		// Flags allow it to be used as a render target, shader resource, UAV
		FPooledRenderTargetDesc RenderTargetDesc = FPooledRenderTargetDesc::Create2DDesc(RenderTargetResource->GetSizeXY(), RenderTargetRHI->GetDesc().Format, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_UAV, TexCreate_None, false);

		GRenderTargetPool.CreateUntrackedElement(RenderTargetDesc, PooledSecondaryRenderTarget, RenderTargetItem);
	}
}