// DistortionSceneProxy.h

#pragma once

#include "CoreMinimal.h"
#include "StaticMeshSceneProxy.h"

class UDistortionMeshComponent;

class FDistortionSceneProxy : public FStaticMeshSceneProxy
{
public:
	// 从组件拷贝渲染所需数据（材质代理、接收体开关、接收体标签）到 Proxy。
	FDistortionSceneProxy(UDistortionMeshComponent* InComponent);
	virtual ~FDistortionSceneProxy();

	virtual void GetDynamicMeshElements(
		const TArray<const FSceneView*>& Views,
		const FSceneViewFamily& ViewFamily,
		uint32 VisibilityMap,
		FMeshElementCollector& Collector) const override;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual SIZE_T GetTypeHash() const override;

	// 提供稳定类型标识，供 PassProcessor 在 AddMeshBatch 快速筛选 Receiver Proxy。
	// 这么做比 RTTI/dynamic_cast 成本更低，且符合 UE 渲染层常见做法。
	static SIZE_T GetStaticTypeHash();

	bool ShouldRenderInRealityDistortionPass() const
	{
		return bEnableDistortionReceiver;
	}

	// Field 侧按 Tag 过滤时使用。None 表示不限制。
	bool HasReceiverTag(const FName& ReceiverTag) const
	{
		return ReceiverTag.IsNone() || ReceiverTags.Contains(ReceiverTag);
	}

private:
	FMaterialRenderProxy* OverrideMaterialProxy = nullptr;

	// 下列数据在构造时从组件拷贝到 RT，避免跨线程直接访问 UObjects。
	bool bEnableDistortionReceiver = true;
	TArray<FName> ReceiverTags;
};
