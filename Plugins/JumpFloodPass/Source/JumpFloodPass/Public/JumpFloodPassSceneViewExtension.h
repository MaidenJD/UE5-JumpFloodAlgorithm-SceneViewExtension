#pragma once

#include "SceneViewExtension.h"

class UTextureRenderTarget2D;

class FJumpFloodPassSceneViewExtension final : public FSceneViewExtensionBase
{

public:

	FJumpFloodPassSceneViewExtension(const FAutoRegister &AutoRegister, UTextureRenderTarget2D* PrimaryRenderTarget, UTextureRenderTarget2D* SecondaryRenderTarget)
		: FSceneViewExtensionBase(AutoRegister)
		, PrimaryRenderTarget(PrimaryRenderTarget)
		, SecondaryRenderTarget(SecondaryRenderTarget)
	{
	}

public:

	void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}
	void PostRenderBasePassDeferred_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView, const FRenderTargetBindingSlots& RenderTargets, TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures) override;

protected:

	bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

private:

	void CreatePooledRenderTargets_RenderThread();

	void AddFloodPass_RenderThread(
		FRDGBuilder& GraphBuilder,
		const FGlobalShaderMap* GlobalShaderMap,
		const FViewInfo& ViewInfo,
		const FIntRect& IntermediateViewport,
		const FRDGTextureRef& PrimaryReadTexture,
		const FRDGTextureRef& PrimaryWriteTexture,
		const FRDGTextureRef& SecondaryReadTexture,
		const FRDGTextureRef& SecondaryWriteTexture,
		int32 FloodExponent,
		float ExponentToUVScaler);

private:

	TObjectPtr<UTextureRenderTarget2D> PrimaryRenderTarget;
	TObjectPtr<UTextureRenderTarget2D> SecondaryRenderTarget;

	TRefCountPtr<IPooledRenderTarget> PooledPrimaryRenderTarget;
	TRefCountPtr<IPooledRenderTarget> PooledSecondaryRenderTarget;

	bool bShouldRecreatePooledRenderTargets = true;

};