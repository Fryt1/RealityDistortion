// RealityDistortionPassProcessor.h
// Phase 2 实战任务：自定义 MeshPassProcessor - 介入渲染管线的"决策层"

#pragma once

#include "CoreMinimal.h"
#include "MeshPassProcessor.h"

/**
 * FRealityDistortionPassProcessor
 * 
 * 自定义的 MeshPassProcessor，控制"画什么"的决策。
 * 核心功能：根据 Primitive 与 FieldCenter 的距离，决定是否为其生成 MeshDrawCommand。
 * 
 * 三层决策逻辑：
 * 1. 空间过滤：距离 FieldCenter 超过 FieldRadius 的物体直接跳过
 * 2. 材质解析：获取有效材质，处理 Fallback 链
 * 3. 指令生成：配置 PSO + Shader，调用 BuildMeshDrawCommands
 */
class FRealityDistortionPassProcessor : public FSceneRenderingAllocatorObject<FRealityDistortionPassProcessor>, public FMeshPassProcessor
{
public:
	FRealityDistortionPassProcessor(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FSceneView* InViewIfDynamicMeshCommand,
		FMeshPassDrawListContext* InDrawListContext,
		const FVector& InFieldCenter,
		float InFieldRadius);

	// FMeshPassProcessor interface
	virtual void AddMeshBatch(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		int32 StaticMeshId = -1) override final;

private:
	bool TryAddMeshBatch(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		int32 StaticMeshId,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material);

	bool Process(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		int32 StaticMeshId,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode);

	/** 空间过滤参数 */
	FVector FieldCenter;
	float FieldRadius;

	/** 渲染状态 */
	FMeshPassProcessorRenderState PassDrawRenderState;
};
