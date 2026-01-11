// DistortionMeshComponent.h
// Phase 1 实战任务：数据劫持 - 自定义 SceneProxy 劫持材质

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "DistortionMeshComponent.generated.h"

/**
 * UDistortionMeshComponent
 * 
 * 继承自 UStaticMeshComponent，重写 CreateSceneProxy() 返回自定义的 FDistortionSceneProxy。
 * 目的：劫持渲染管线，在 GetDynamicMeshElements 中强制替换材质。
 */
UCLASS(ClassGroup=(Rendering), meta=(BlueprintSpawnableComponent))
class REALITYDISTORTION_API UDistortionMeshComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UDistortionMeshComponent(const FObjectInitializer& ObjectInitializer);

	//~ Begin UPrimitiveComponent Interface
	/** 重写此函数，返回我们自定义的 FDistortionSceneProxy */
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface

	/** 用于替换的材质，在编辑器 Details 面板中可设置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distortion")
	TObjectPtr<UMaterialInterface> OverrideMaterial;
};
