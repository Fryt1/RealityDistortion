// DistortionFieldComponent.h
//
// UDistortionFieldComponent（Emitter）
// -----------------------------------
// 职责：
// 1) 在 GT 上维护一个 FieldHandle 的生命周期。
// 2) 采样组件位置/半径并推送给 Renderer（SetRealityDistortionFieldSettings_GameThread）。
// 3) 不直接参与 DrawCall，只提供“空间影响范围”数据。

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "DistortionFieldComponent.generated.h"

UCLASS(ClassGroup=(Rendering), meta=(BlueprintSpawnableComponent))
class REALITYDISTORTION_API UDistortionFieldComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UDistortionFieldComponent();

	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 发射器开关：关闭后仍保留句柄，但会以 bEnabled=false 推送到 RT。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distortion|Field")
	bool bEnableField = true;

	// 发射器中心偏移（相对于组件世界位置）。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distortion|Field")
	FVector FieldCenterOffset = FVector::ZeroVector;

	// 作用半径（世界空间）。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distortion|Field", meta = (ClampMin = "0.0"))
	float FieldRadius = 500.0f;

	// 力场匹配用 Tag。
	// None：不做 Tag 过滤（命中半径即可）。
	// 非 None：仅影响“组件 Tag 或 Actor Tag”命中该值的接收体。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distortion|Field")
	FName ReceiverTagFilter = NAME_None;

private:
	// 延迟创建 Handle，保证每个组件对应一个独立 Field 实例。
	void EnsureFieldHandle();

	// GT 采样组件状态并通过 SetRealityDistortionFieldSettings_GameThread 推送到 RT。
	void PushFieldSettingsToRenderer() const;

	// 0 代表无效句柄（RealityDistortionInvalidFieldHandle）。
	uint32 FieldHandle = 0;
};
