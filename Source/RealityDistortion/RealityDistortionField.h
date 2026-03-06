// RealityDistortionField.h
//
// Reality Distortion Field Management API
// ----------------------------------------
// 提供 GT/RT 双向的力场数据管理接口

#pragma once

#include "CoreMinimal.h"

// ============================================================================
// 常量定义
// ============================================================================
constexpr uint32 RealityDistortionInvalidFieldHandle = 0;
constexpr uint32 MAX_DISTORTION_FIELDS = 4;

// ============================================================================
// 力场设置结构体
// ============================================================================
// 从 GT 推送到 RT 的力场参数
struct FRealityDistortionFieldSettings
{
	FVector Center = FVector::ZeroVector;
	float Radius = 0.0f;
	float Strength = 1.0f;
	bool bEnabled = false;
	FName ReceiverTagFilter = NAME_None;

	FRealityDistortionFieldSettings() = default;
};

// ============================================================================
// GameThread API - 力场句柄管理
// ============================================================================
// 创建一个新的力场句柄（每个 UDistortionFieldComponent 调用一次）
REALITYDISTORTION_API uint32 CreateRealityDistortionFieldHandle_GameThread();

// 销毁力场句柄（组件 OnUnregister 时调用）
REALITYDISTORTION_API void DestroyRealityDistortionFieldHandle_GameThread(uint32 Handle);

// 设置力场参数（每帧 Tick 时调用，将数据推送到 RT）
REALITYDISTORTION_API void SetRealityDistortionFieldSettings_GameThread(uint32 Handle, const FRealityDistortionFieldSettings& Settings);

// 重置所有力场（模块启动时调用，清理 PIE/热重载残留）
REALITYDISTORTION_API void ResetRealityDistortionFields_GameThread();

// ============================================================================
// RenderThread API - 力场数据读取
// ============================================================================
// 获取当前所有活跃的力场设置（在 RT 调用，用于 PassProcessor 和 Shader 绑定）
REALITYDISTORTION_API TConstArrayView<FRealityDistortionFieldSettings> GetRealityDistortionFieldSettings_RenderThread();
