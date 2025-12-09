/*
* @brief
* @ 合成アニメーションとは
*/
#ifndef __MODEL_H__
#define __MODEL_H__

#include <DirectXMath.h>
#include <string>
#include <vector>
#include "Shader.h"
#include "MeshBuffer.h"

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
                DirectX::XMFLOAT3       normal;
                DirectX::XMFLOAT2       uv;
                DirectX::XMFLOAT4       color;
        };
        using Vertices  = std::vector<Vertex>;
        using Indices   = std::vector<unsigned long>;

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
        void Draw(int meshNo = -1);

        //--- 各種情報取得
        const Mesh* GetMesh(unsigned int index);
        uint32_t GetMeshNum();
        const Material* GetMaterial(unsigned int index);
        uint32_t GetMaterialNum();

#ifdef _DEBUG
        static std::string GetError();
#endif

private:
        static Texture*         m_pWhiteTex;    // ftHgeNX`
        static unsigned int             m_textureRef;   // ftHgeNX`QƐ
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
        float                   m_loadScale;    //
        Flip                    m_loadFlip;             //

        Meshes                  m_meshes;               // メッシュ配列
        Materials               m_materials;    // マテリアル配列
        VertexShader*   m_pVS;                  // 設定中の頂点シェーダ
        PixelShader*    m_pPS;                  // 設定中のピクセルシェーダ

};


#endif // __MODEL_H__
