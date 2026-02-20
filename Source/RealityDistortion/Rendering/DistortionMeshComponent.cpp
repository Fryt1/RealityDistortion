// DistortionMeshComponent.cpp

#include "Rendering/DistortionMeshComponent.h"

#include "Materials/Material.h"
#include "Rendering/DistortionSceneProxy.h"
#include "UObject/ConstructorHelpers.h"

UDistortionMeshComponent::UDistortionMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 默认给一个可见材质，便于未配置时直接看到“材质劫持是否生效”。
	// 后续你可以在蓝图/细节面板替换为自己的 Distortion 材质。
	static ConstructorHelpers::FObjectFinder<UMaterial> DefaultMaterialFinder(
		TEXT("/Engine/EngineMaterials/WorldGridMaterial"));
	if (DefaultMaterialFinder.Succeeded())
	{
		OverrideMaterial = DefaultMaterialFinder.Object;
	}
}

FPrimitiveSceneProxy* UDistortionMeshComponent::CreateSceneProxy()
{
	// ------------------------------
	// 关键入口：切换到自定义 SceneProxy
	// ------------------------------
	// 这里做足前置检查，避免把无效资源带到 RT。
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

	// 交给 FDistortionSceneProxy，后续 MeshBatch 会在其 GetDynamicMeshElements 中被“劫持”。
	return new FDistortionSceneProxy(this);
}
