// DistortionMeshComponent.cpp

#include "Rendering/DistortionMeshComponent.h"

#include "Materials/Material.h"
#include "Rendering/DistortionSceneProxy.h"
#include "UObject/ConstructorHelpers.h"

UDistortionMeshComponent::UDistortionMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 默认给一个可见材质，便于未配置时直接看到”材质劫持是否生效”。
	// 后续你可以在蓝图/细节面板替换为自己的 Distortion 材质。
	static ConstructorHelpers::FObjectFinder<UMaterial> DefaultMaterialFinder(
		TEXT("/Engine/EngineMaterials/WorldGridMaterial"));
	if (DefaultMaterialFinder.Succeeded())
	{
		OverrideMaterial = DefaultMaterialFinder.Object;
	}

	// Receiver 走常规 PrePass / BasePass 深度链路。
	// 具体“力场挖洞”在 DepthOnly + BasePass 的像素级 clip 里完成。
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

	// 通过 CustomPrimitiveData[0] 标记本 Primitive 为 Distortion Receiver，
	// 这样 BasePassPixelShader.usf 里的 clip() 只对标记了的 mesh 生效，
	// 避免普通 StaticMeshComponent 被误裁导致黑块。
	SetCustomPrimitiveDataFloat(0, 1.0f);

	// 交给 FDistortionSceneProxy，后续 MeshBatch 会在其 GetDynamicMeshElements 中被”劫持”。
	return new FDistortionSceneProxy(this);
}
