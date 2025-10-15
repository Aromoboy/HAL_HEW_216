
#include"DirectX.h"


ID3D11Device* g_pDevice; // DirectXでアセットの作成を行う機能
ID3D11DeviceContext* g_pContext;  // DirectXで描画命令を発行する機能
IDXGISwapChain* g_pSwapChain;// フレームバッファとディスプレイの橋渡し
ID3D11RenderTargetView* g_pRTV;		 // フレームバッファそのもの
ID3D11BlendState* g_pBlendState;	//ブレンドのステート






HRESULT InitDirectX(HWND hwnd, UINT width, UINT height, BOOL fullscreen)
{
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));//初期化
	sd.BufferDesc.Width = width;						// フレームバッファの幅
	sd.BufferDesc.Height = height;						// フレームバッファの高さ
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;	// フレームバッファの色情報(各8bit)
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.SampleDesc.Count = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;   // バックバッファの使用方法
	sd.BufferCount = 1;					// バックバッファの数
	sd.OutputWindow = hwnd;					// 関連付けるウィンドウ
	sd.Windowed = fullscreen ? FALSE : TRUE;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;


	// ドライバの種類
	D3D_DRIVER_TYPE driverType;
	D3D_DRIVER_TYPE driverTypes[] = {
		D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP, D3D_DRIVER_TYPE_REFERENCE
	};
	UINT numDriverTypes = ARRAYSIZE(driverTypes);
	
	//機能レベル
	D3D_FEATURE_LEVEL featureLevel;
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2,D3D_FEATURE_LEVEL_9_1
	};
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	//DirectXのDebug設定
	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	//createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	//作成可能な設定でDirectXの機能を有効化
	HRESULT hr;
	for (UINT i = 0; i < numDriverTypes; i++)
	{
		driverType = driverTypes[i];
		hr = D3D11CreateDeviceAndSwapChain(
			NULL, driverType, NULL, createDeviceFlags, featureLevels, numFeatureLevels,
			D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pDevice, &featureLevel, &g_pContext);
		if (SUCCEEDED(hr)) { break; }
	}
	if (FAILED(hr)) { return hr; }

	//フレームバッファポインタを取得
	ID3D11Texture2D* pBackBuffer = nullptr;
	hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
	//フレームバッファへのポインタを紐づけて、レンダーターゲットレビューを作成	
	if (SUCCEEDED(hr))
	{
		hr = g_pDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRTV);
		g_pContext->OMSetRenderTargets(1, &g_pRTV, nullptr);	// GPUの出力設定
	}



	D3D11_VIEWPORT vp;
	vp.TopLeftX = 0.0f;
	vp.TopLeftY = 0.0f;
	vp.Width = width;
	vp.Height = height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	g_pContext->RSSetViewports(1, &vp);


	D3D11_BLEND_DESC blendDesc;
	ZeroMemory(&blendDesc, sizeof(blendDesc));
	blendDesc.RenderTarget[0].BlendEnable = true;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	hr = g_pDevice->CreateBlendState(&blendDesc, &g_pBlendState);
	if (FAILED(hr)) { return hr; }

	//ブレンドステートの設定
	float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	g_pContext->OMSetBlendState(g_pBlendState, blendFactor, 0xffffffff);
	

	return hr;
}


void UninitDirectX()
{
	g_pBlendState->Release();
	g_pRTV->Release();
	g_pSwapChain->SetFullscreenState(false, NULL);
	g_pSwapChain->Release();
	g_pContext->ClearState();
	g_pContext->Release();
	g_pDevice->Release();
}

void BeginDrawDirectX()
{
	float color[4] = { 0.8f, 0.9f, 1.0f, 1.0f };
	g_pContext->ClearRenderTargetView(g_pRTV, color);
}

void	EndDrawDirectX()
{
	g_pSwapChain->Present(0, 0);
}

ID3D11Device* GetDevice()
{
	return g_pDevice;
}

ID3D11DeviceContext* GetContext()// Contextの取得
{
	return g_pContext;
}