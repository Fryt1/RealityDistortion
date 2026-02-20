// RealityDistortionPassProcessor.h
//
// FRealityDistortionPassProcessor
// ------------------------------
// 这是 RealityDistortion Pass 的决策层：
// 1) AddMeshBatch: 决定“画什么”（Receiver/空间/材质三层过滤）
// 2) TryAddMeshBatch: 决定“是否可用当前材质 + 是否需要 DefaultMaterial 回退”
// 3) Process: 决定“怎么画”（Shader/Pipeline/RenderState/DrawCommand）

#pragma once

#include "CoreMinimal.h"
#include "MeshPassProcessor.h"

class FRealityDistortionPassProcessor
	: public FSceneRenderingAllocatorObject<FRealityDistortionPassProcessor>
	, public FMeshPassProcessor
{
public:
	FRealityDistortionPassProcessor(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FSceneView* InViewIfDynamicMeshCommand,
		FMeshPassDrawListContext* InDrawListContext);

	// MeshPass 入口：每个候选 MeshBatch 都会走这里。
	virtual void AddMeshBatch(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		int32 StaticMeshId = -1) override final;

private:
	// 第二阶段过滤：处理 BlendMode/Domain，并做必要的材质回退。
	bool TryAddMeshBatch(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		int32 StaticMeshId,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material);

	// 最终构建 DrawCommand：查找 Shader、组装 RenderState、调用 BuildMeshDrawCommands。
	bool Process(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		int32 StaticMeshId,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode);

	FMeshPassProcessorRenderState PassDrawRenderState;
};
