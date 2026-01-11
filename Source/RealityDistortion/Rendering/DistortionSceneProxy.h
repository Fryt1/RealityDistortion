// DistortionSceneProxy.h
// Phase 1 实战任务：数据劫持 - 自定义 SceneProxy

#pragma once

#include "CoreMinimal.h"
#include "StaticMeshSceneProxy.h"

class UDistortionMeshComponent;

/**
 * FDistortionSceneProxy
 * 
 * 继承自 FStaticMeshSceneProxy，重写 GetDynamicMeshElements。
 * 在提交 MeshBatch 前，强制替换 MaterialRenderProxy。
 * 
 * 这是渲染线程的对象，由 UDistortionMeshComponent::CreateSceneProxy() 创建。
 */
class FDistortionSceneProxy : public FStaticMeshSceneProxy
{
public:
	/** 构造函数，从组件拷贝材质数据 */
	FDistortionSceneProxy(UDistortionMeshComponent* InComponent);
	virtual ~FDistortionSceneProxy();

	//~ Begin FPrimitiveSceneProxy Interface
	/** 重写此函数，在父类逻辑基础上劫持材质 */
	virtual void GetDynamicMeshElements(
		const TArray<const FSceneView*>& Views,
		const FSceneViewFamily& ViewFamily,
		uint32 VisibilityMap,
		FMeshElementCollector& Collector) const override;
	
	/** 强制使用动态绘制路径（否则走静态缓存，不会调用 GetDynamicMeshElements）*/
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	
	virtual SIZE_T GetTypeHash() const override;
	//~ End FPrimitiveSceneProxy Interface

private:
	/** 缓存的材质渲染代理，从 GameThread 拷贝供 RenderThread 使用 */
	FMaterialRenderProxy* OverrideMaterialProxy;
};
