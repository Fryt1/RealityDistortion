// DistortionFieldComponent.cpp

#include "Rendering/DistortionFieldComponent.h"

#include "RealityDistortionField.h"
#include "DrawDebugHelpers.h"

UDistortionFieldComponent::UDistortionFieldComponent()
{
	// Field 发射器默认每帧推送一次，以便位置移动时立即影响过滤结果。
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.bTickEvenWhenPaused = true;
	bTickInEditor = true;
}

void UDistortionFieldComponent::EnsureFieldHandle()
{
	if (FieldHandle == RealityDistortionInvalidFieldHandle)
	{
		// 每个发射器组件独占一个 Handle，RT 侧通过 Handle 做 upsert。
		FieldHandle = CreateRealityDistortionFieldHandle_GameThread();
	}
}

void UDistortionFieldComponent::OnRegister()
{
	Super::OnRegister();
	EnsureFieldHandle();

	// 注册后立刻推一次，避免第一帧读到默认值。
	PushFieldSettingsToRenderer();
}

void UDistortionFieldComponent::OnUnregister()
{
	if (FieldHandle != RealityDistortionInvalidFieldHandle)
	{
		// 先推送禁用状态，再销毁 Handle，避免 RT 保留脏数据。
		// 顺序是：Disable -> Destroy，这样 RT 在处理队列时不会短暂读到“悬空有效数据”。
		FRealityDistortionFieldSettings DisabledSettings;
		DisabledSettings.bEnabled = false;
		SetRealityDistortionFieldSettings_GameThread(FieldHandle, DisabledSettings);

		DestroyRealityDistortionFieldHandle_GameThread(FieldHandle);
		FieldHandle = RealityDistortionInvalidFieldHandle;
	}

	Super::OnUnregister();
}

void UDistortionFieldComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Tick 时只做参数采样与推送，不在 GT 侧做渲染决策。
	PushFieldSettingsToRenderer();

	// 绘制调试可视化
	if (bShowDebugVisualization && GetWorld())
	{
		FVector Center = GetComponentLocation() + FieldCenterOffset;

		const float ScaledRadius = FieldRadius * GetComponentScale().GetMax();

		// 绘制半透明球体显示作用范围
		DrawDebugSphere(
			GetWorld(),
			Center,
			ScaledRadius,
			32,  // 球体段数
			DebugColor,
			false,  // 不持久化
			-1.0f,  // 持续时间（-1 表示一帧）
			0,      // 深度优先级
			2.0f    // 线条粗细
		);

		// 绘制中心点
		DrawDebugPoint(
			GetWorld(),
			Center,
			10.0f,  // 点大小
			DebugColor,
			false,
			-1.0f
		);

		// 绘制力场强度文本
		if (bEnableField)
		{
			DrawDebugString(
				GetWorld(),
				Center + FVector(0, 0, ScaledRadius + 50.0f),
				FString::Printf(TEXT("Field: R=%.0f S=%.0f"), ScaledRadius, FieldStrength),
				nullptr,
				DebugColor,
				-1.0f,
				true  // 绘制阴影
			);
		}
		else
		{
			DrawDebugString(
				GetWorld(),
				Center + FVector(0, 0, ScaledRadius + 50.0f),
				TEXT("Field: DISABLED"),
				nullptr,
				FColor::Red,
				-1.0f,
				true
			);
		}
	}
}

void UDistortionFieldComponent::PushFieldSettingsToRenderer() const
{
	if (FieldHandle == RealityDistortionInvalidFieldHandle)
	{
		return;
	}

	const float ScaledRadius = FieldRadius * GetComponentScale().GetMax();

	FRealityDistortionFieldSettings FieldSettings;
	FieldSettings.Center = GetComponentLocation() + FieldCenterOffset;
	FieldSettings.Radius = ScaledRadius;
	FieldSettings.Strength = FieldStrength;
	FieldSettings.bEnabled = IsRegistered() && bEnableField && ScaledRadius > 0.0f;
	FieldSettings.ReceiverTagFilter = ReceiverTagFilter;

	// 通过 GameThread API 入队，真正写入发生在 RT 的命令队列消费阶段。
	// 这里不直接触碰 RT 容器，避免 GT/RT 并发读写冲突。
	SetRealityDistortionFieldSettings_GameThread(FieldHandle, FieldSettings);
}
