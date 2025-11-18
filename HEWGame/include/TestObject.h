// ============================================================================
// 概要: TestObject に関するインターフェースやデータ構造を宣言し、include から再利用できるようにする。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: include 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

#pragma once

#include "Vector.h"
#include "Model.h"
#include <string>
#include <memory>




class TestObject
{
public:
	TestObject(std::string a_szModelKey, float3 a_fPosition, float3 a_fScale, float3 a_fRads);

	~TestObject() = default;

	const float3 GetPosition();
	const float3 GetScale();
	const float3 GetfRads();

	Model& GetModel();


private:
	std::unique_ptr<Model> m_pModel;

	float3 m_fPos;
	float3 m_fScale;
	float3 m_fRads;



};

