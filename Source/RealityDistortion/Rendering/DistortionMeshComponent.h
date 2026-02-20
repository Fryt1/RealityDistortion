// DistortionMeshComponent.h
//
// UDistortionMeshComponent（Receiver）
// ------------------------------------
// 职责：
// 1) 继承 UStaticMeshComponent，保持普通静态网格的编辑体验。
// 2) 重写 CreateSceneProxy()，把渲染代理切换为 FDistortionSceneProxy。
// 3) 提供接收体主开关与覆盖材质。
//
// 注意：
// - 这个组件不负责“发射力场”，发射职责在 UDistortionFieldComponent。
// - 这个组件不直接做渲染决策，真正决策在 FRealityDistortionPassProcessor::AddMeshBatch。

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "DistortionMeshComponent.generated.h"

UCLASS(ClassGroup=(Rendering), meta=(BlueprintSpawnableComponent))
class REALITYDISTORTION_API UDistortionMeshComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UDistortionMeshComponent(const FObjectInitializer& ObjectInitializer);

	// 通过自定义 SceneProxy 接入渲染管线。
	// 之后该 Primitive 在 RT 会以 FDistortionSceneProxy 的形态参与收集与过滤。
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

	// Phase 1 材质劫持入口。
	// DistortionSceneProxy::GetDynamicMeshElements 会把 MeshBatch.MaterialRenderProxy
	// 替换为此材质的 RenderProxy。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distortion")
	TObjectPtr<UMaterialInterface> OverrideMaterial;

	// Receiver 主开关：false 表示该组件永远不作为 Distortion 接收体。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distortion|Receiver")
	bool bEnableDistortionReceiver = true;
};
