// シェーダ資源の読み込みと保持を行う
// 描画初期化や描画処理から参照される

#ifndef __SHADER_H__
#define __SHADER_H__

#include "DirectX.h"
#include "Texture.h"
#include <string>
#include <map>
#include <vector>

// VF[_[̊{NX
class Shader
{
protected:
	enum Kind
	{
		Vertex,
		Pixel
	};
protected:
	Shader(Kind kind);
public:
	virtual ~Shader();
public:
	// VF[_[t@C(*.cso)ǂݍޏ
	HRESULT Load(const char* pFileName);
	// 񂩂VF[_RpC
	HRESULT Compile(const char* pCode);


	// 萔̏
	void WriteBuffer(UINT slot, void* pData);
	// eNX`̐ݒ
	void SetTexture(UINT slot, Texture* tex);
	// eNX`̐ݒ
	void SetTexture(UINT slot, ID3D11ShaderResourceView* tex);

	// VF[_[`Ɏgp
	virtual void Bind(void) = 0;

private:
	HRESULT Make(void* pData, UINT size);
protected:
	// VF[_[t@Cǂݍ񂾌AVF[_[̎ޕʂɏs
	virtual HRESULT MakeShader(void* pData, UINT size) = 0;

private:
	Kind m_kind;
protected:
	std::vector<ID3D11Buffer*> m_pBuffers;
	std::vector<ID3D11ShaderResourceView*> m_pTextures;
};

//----------
// _VF[_
class VertexShader : public Shader
{
public:
	VertexShader();
	~VertexShader();
	void Bind(void);
protected:
	HRESULT MakeShader(void* pData, UINT size);

private:
	ID3D11VertexShader* m_pVS;
	ID3D11InputLayout* m_pInputLayout;
};
//----------
// sNZVF[_
class PixelShader : public Shader
{
public:
	PixelShader();
	~PixelShader();
	void Bind(void);
protected:
	HRESULT MakeShader(void* pData, UINT size);
private:
	ID3D11PixelShader* m_pPS;
};

#endif