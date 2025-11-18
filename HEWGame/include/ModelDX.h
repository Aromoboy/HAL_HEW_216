// ============================================================================
// 概要: ModelDX に関するインターフェースやデータ構造を宣言し、include から再利用できるようにする。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: include 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

#ifndef ___ModelDX_H___
#define ___ModelDX_H___

#include <vector>
#include <memory>

#include "MeshDX.h"
#include "MaterialDX.h"

class ModelDX
{
public:
	ModelDX(std::vector<std::shared_ptr<MeshDX>> a_meshes, std::vector<std::shared_ptr<MaterialDX>> a_MaterialDXs);
	~ModelDX();

	unsigned int GetMeshCount();

	std::weak_ptr<MeshDX> GetMesh(int a_meshIndex);
	std::weak_ptr<MaterialDX> GetMaterial(int a_materialIndex);

private:
	
	std::vector<std::shared_ptr<MeshDX>> m_meshes;
	std::vector<std::shared_ptr<MaterialDX>> m_materials;

	unsigned int m_meshCount;
};



#endif // 備考: !__ModelDX_H___ に関する説明を日本語で補足している。
