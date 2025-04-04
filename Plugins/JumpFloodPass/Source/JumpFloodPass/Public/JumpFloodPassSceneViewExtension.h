#pragma once

#include "SceneViewExtension.h"

class UTextureRenderTarget2D;

class FJumpFloodPassSceneViewExtension final : public FSceneViewExtensionBase
{

public:

	FJumpFloodPassSceneViewExtension(const FAutoRegister &AutoRegister, UTextureRenderTarget2D* RenderTarget)
		: FSceneViewExtensionBase(AutoRegister)
		, RenderTarget(RenderTarget)
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

	void CreatePooledRenderTarget_RenderThread();

	void AddFloodPass_RenderThread(
		FRDGBuilder& GraphBuilder,
		const FGlobalShaderMap* GlobalShaderMap,
		const FViewInfo& ViewInfo,
		const FIntRect& IntermediateViewport,
		const FRDGTextureRef& ReadTexture,
		const FRDGTextureRef& WriteTexture,
		int32 FloodExponent,
		float ExponentToUVScaler);

private:

	TObjectPtr<UTextureRenderTarget2D> RenderTarget;
	TRefCountPtr<IPooledRenderTarget> PooledRenderTarget;

	bool bShouldRecreatePooledRenderTarget = true;

};