#include "JumpFloodPassSceneViewExtension.h"
#include "JumpFloodPassSettings.h"
#include "JumpFloodSeedPass.h"
#include "JumpFloodFloodPass.h"
#include "JumpFloodCopyPass.h"

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

TAutoConsoleVariable<float> CVarJFARenderScale(
	TEXT("r.JFA.RenderScale"),
	1.0f,
	TEXT("Value to scale Jump Flooding render textures by. 1.0 scale is used when value is 0\n"),
	ECVF_RenderThreadSafe);

bool FJumpFloodPassSceneViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	return UJumpFloodPassSettings::IsEnabled() && RenderTarget;
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

	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.JFA.RenderScale"));
	float RenderScale = CVar->GetValueOnRenderThread() > 0.0f ? CVar->GetValueOnRenderThread() : 1.0f;

	const FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

	RDG_EVENT_SCOPE(GraphBuilder, "JFA");

	if (bShouldRecreatePooledRenderTarget)
	{
		CreatePooledRenderTarget_RenderThread();
	}

	FRDGTextureRef RenderTargetTexture = GraphBuilder.RegisterExternalTexture(PooledRenderTarget, TEXT("JFA Render Target"));
	const FIntRect RenderViewport = FIntRect(0, 0, RenderTargetTexture->Desc.Extent.X, RenderTargetTexture->Desc.Extent.Y);

	FRDGTextureDesc IntermediateTargetDesc = RenderTargetTexture->Desc;
	IntermediateTargetDesc.Extent =
		FIntPoint {
			(int32) ((float) IntermediateTargetDesc.Extent.X * RenderScale),
			(int32) ((float) IntermediateTargetDesc.Extent.Y * RenderScale) };

	const FIntRect IntermediateViewport = FIntRect(0, 0, IntermediateTargetDesc.Extent.X, IntermediateTargetDesc.Extent.Y);

	FRDGTextureRef JFATextures[] = {
		GraphBuilder.CreateTexture(IntermediateTargetDesc, TEXT("JFA Target A")),
		GraphBuilder.CreateTexture(IntermediateTargetDesc, TEXT("JFA Target B")),
	};

	int32 ReadIndex  = 0;
	int32 WriteIndex = 1;

	//  Init Pass
	{
		FJumpFloodSeedPassPS::FParameters* Parameters = GraphBuilder.AllocParameters<FJumpFloodSeedPassPS::FParameters>();
		Parameters->TextureSize = JFATextures[WriteIndex]->Desc.Extent;
		Parameters->TextureSizeInverse = FVector2f(1.0f, 1.0f) / Parameters->TextureSize;
		Parameters->ViewportSize = RenderViewport.Size();
		Parameters->View = InView.ViewUniformBuffer;
		Parameters->SceneTextures = CreateSceneTextureShaderParameters(GraphBuilder, ViewInfo.GetSceneTexturesChecked(), InView.GetFeatureLevel(), ESceneTextureSetupMode::All);

		// We're going to also clear the render target
		Parameters->RenderTargets[0] = FRenderTargetBinding(JFATextures[WriteIndex], ERenderTargetLoadAction::EClear);

		TShaderMapRef<FJumpFloodSeedPassPS> PixelShader(GlobalShaderMap);
		FPixelShaderUtils::AddFullscreenPass(GraphBuilder, GlobalShaderMap, FRDGEventName(TEXT("JFA: Init")), PixelShader, Parameters, IntermediateViewport);
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
			JFATextures[ReadIndex],
			JFATextures[WriteIndex],
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
				JFATextures[ReadIndex],
				JFATextures[WriteIndex],
				FloodExponent,
				LargestSideInverse);
		}
	}

	//  Final stretched copy pass
	{
		FJumpFloodCopyPassPS::FParameters* Parameters = GraphBuilder.AllocParameters<FJumpFloodCopyPassPS::FParameters>();
		Parameters->InSource = JFATextures[WriteIndex];
		Parameters->InSourceSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		Parameters->InDestinationResolution = RenderViewport.Size();

		Parameters->RenderTargets[0] = FRenderTargetBinding(RenderTargetTexture, ERenderTargetLoadAction::EClear);

		TShaderMapRef<FJumpFloodCopyPassPS> PixelShader(GlobalShaderMap);
		FPixelShaderUtils::AddFullscreenPass(GraphBuilder, GlobalShaderMap, FRDGEventName(TEXT("JFA: Final Copy")), PixelShader, Parameters, RenderViewport);
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
	Parameters->TextureSize = ReadTexture->Desc.Extent;
	Parameters->TextureSizeInverse = FVector2f(1.0f, 1.0f) / Parameters->TextureSize;
	Parameters->View = ViewInfo.ViewUniformBuffer;
	Parameters->SceneTextures = CreateSceneTextureShaderParameters(GraphBuilder, ViewInfo.GetSceneTexturesChecked(), ViewInfo.GetFeatureLevel(), ESceneTextureSetupMode::All);
	Parameters->JFATexture = ReadTexture;
	Parameters->JFASampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	Parameters->StepSize = ((float) (1 << FloodExponent)) * ExponentToUVScaler;

	Parameters->RenderTargets[0] = FRenderTargetBinding(WriteTexture, ERenderTargetLoadAction::ELoad);

	TShaderMapRef<FJumpFloodFloodPassPS> PixelShader(GlobalShaderMap);
	FPixelShaderUtils::AddFullscreenPass(GraphBuilder, GlobalShaderMap, FRDGEventName(TEXT("JFA: Flood %d"), (1 << FloodExponent)), PixelShader, Parameters, IntermediateViewport);
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