// DistortionMeshComponent.cpp
// Phase 1 实战任务：数据劫持 - 自定义组件实现

#include "Rendering/DistortionMeshComponent.h"
#include "Rendering/DistortionSceneProxy.h"
#include "Materials/Material.h"
#include "Engine/Engine.h"

UDistortionMeshComponent::UDistortionMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 加载引擎自带的 WorldGridMaterial 作为默认覆盖材质
	// 你可以在编辑器 Details 面板中换成自己的材质
	static ConstructorHelpers::FObjectFinder<UMaterial> RedMaterialFinder(
		TEXT("/Engine/EngineMaterials/WorldGridMaterial")
	);
	
	if (RedMaterialFinder.Succeeded())
	{
		OverrideMaterial = RedMaterialFinder.Object;
	}
}

FPrimitiveSceneProxy* UDistortionMeshComponent::CreateSceneProxy()
{
	// ========================================
	// 关键点：这是"劫持"的入口
	// ========================================
	// 正常情况：UStaticMeshComponent 返回 FStaticMeshSceneProxy
	// 我们返回自定义的 FDistortionSceneProxy
	
	if (GetStaticMesh() == nullptr)
	{
		return nullptr;
	}

	if (GetStaticMesh()->GetRenderData() == nullptr)
	{
		return nullptr;
	}

	if (!GetStaticMesh()->GetRenderData()->IsInitialized())
	{
		return nullptr;
	}

	// 创建并返回自定义 Proxy，引擎会管理其生命周期
	return new FDistortionSceneProxy(this);
}
