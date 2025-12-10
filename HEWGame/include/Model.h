#ifndef __MODEL_H__
#define __MODEL_H__

#include <DirectXMath.h>
#include <cstdint>
#include <vector>
#include "Shader.h"
#include "MeshBuffer.h"
#include "WorldBuffer.h"

class Model
{
public:
        // モデル反転設定
        enum Flip
        {
                None,                   // DirectX準拠(反転する
                XFlip,                  // Maya準拠
                ZFlip,                  // DirectX準拠(Mayaから180回転した状態
                ZFlipUseAnime,  // DirecX準拠(アニメーションさせる場合
        };

public:
        // 頂点情報
        struct Vertex
        {
                DirectX::XMFLOAT3       pos;
                DirectX::XMFLOAT2       uv;
                DirectX::XMFLOAT3       normal;
        };
        using Vertices  = std::vector<Vertex>;
        using Indices   = std::vector<uint32_t>;
        static_assert(sizeof(Indices::value_type) == sizeof(uint32_t), "Model indices must remain 32-bit.");

        // メッシュ
        struct Mesh
        {
                Vertices                vertices;
                Indices                 indices;
                unsigned int    materialID;
                MeshBuffer*             pMesh;
        };
        using Meshes = std::vector<Mesh>;

        // マテリアル情報
        struct Material
        {
                DirectX::XMFLOAT4       diffuse;        // 拡散光(メインカラー
                DirectX::XMFLOAT4       ambient;        // 環境光(陰の部分のカラー
                DirectX::XMFLOAT4       specular;       // 鏡面反射光(強く光る部分のカラー
                Texture* pTexture;      // テクスチャ
        };
        using Materials = std::vector<Material>;

public:
        Model();
        ~Model();
        void Reset();
        void SetVertexShader(VertexShader* vs);
        void SetPixelShader(PixelShader* ps);
        bool Load(const char* file, float scale = 1.0f, Flip flip = Flip::None);
        void Draw(const WorldBuffer& wvp, int meshNo);

        //--- 各種情報取得
        const Mesh* GetMesh(unsigned int index);
        uint32_t GetMeshNum();
        const Material* GetMaterial(unsigned int index);
        uint32_t GetMaterialNum();

#ifdef _DEBUG
        static std::string GetError();
#endif


private:
        // 各種生成
        void MakeMesh(const void* ptr, float scale, Flip flip);
        void MakeMaterial(const void* ptr, std::string directory);

private:
        static VertexShader*    m_pDefVS;               // デフォルト頂点シェーダー
        static PixelShader*             m_pDefPS;               // デフォルトピクセルシェーダー
        static unsigned int             m_shaderRef;    // シェーダー参照数
#ifdef _DEBUG
        static std::string m_errorStr;
#endif

private:
        Meshes                  m_meshes;               // メッシュ配列
        Materials               m_materials;    // マテリアル配列
        VertexShader*   m_pVS;                  // 設定中の頂点シェーダ
        PixelShader*    m_pPS;                  // 設定中のピクセルシェーダ
};


#endif // __MODEL_H__
