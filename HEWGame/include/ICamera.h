// ============================================================================
// 概要: ICamera に関するインターフェースやデータ構造を宣言し、include から再利用できるようにする。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: include 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

#pragma once

#include <DirectXMath.h>

namespace CameraSpace
{
	class ICamera
	{
	public:
		virtual ~ICamera() = default;

		virtual const DirectX::XMFLOAT4X4& GetView() const = 0;
		virtual const DirectX::XMFLOAT4X4& GetProjection() const = 0;

	protected:
		virtual void RecalculateView() = 0;
	};
}
