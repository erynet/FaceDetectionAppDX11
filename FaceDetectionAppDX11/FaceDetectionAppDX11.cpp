#include <windows.h>
#include <mmSystem.h>
#pragma comment (lib, "Winmm.lib")

#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <directxcolors.h>

#include <opencv2/opencv.hpp>

#include "resource.h"
#include <tchar.h>

#define SAFE_FREE(p) if (p != nullptr) { ::free(p); p = nullptr; }
#define SAFE_DELETE(p) if (p != nullptr) { delete p; p = nullptr; }
#define SAFE_DELETE_ARRAY(p) if (p != nullptr) { delete[] p; p = nullptr; }
#define SAFE_RELEASE(p) if (p != nullptr) { p->Release(); p = nullptr; }

#define SHM_REQ_SIZE		(8+(512*512*3))
#define SHM_RES_SIZE		(2+((4*6)*32)+6)

using namespace DirectX;

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct _ShmReqLayout
{
	__int64 ts;
	char data[512 * 512 * 3];
};

struct _ShmResLayout
{
	short cnt;
	float data[6 * 32];
};

struct _SimpleVertex
{
	XMFLOAT3 Pos;
	XMFLOAT2 Tex;
};

struct _CBTextureIndex { XMFLOAT4 mTxIdx; };

class FaceDetectionAppDX11
{
public:
	FaceDetectionAppDX11(int srcWidth, int srcHeight)
		: m_driverType(D3D_DRIVER_TYPE_NULL)
		, m_featureLevel(D3D_FEATURE_LEVEL_11_0)
		, m_pd3dDevice(nullptr)
		, m_pd3dDevice1(nullptr)
		, m_pImmediateContext(nullptr)
		, m_pImmediateContext1(nullptr)
		, m_pSwapChain(nullptr)
		, m_pSwapChain1(nullptr)
		, m_pRenderTargetView(nullptr)
		, m_pDepthStencil(nullptr)
		, m_pDepthStencilView(nullptr)
		, m_pTx0(nullptr)
		, m_pTx1(nullptr)
		, m_pTx0RV(nullptr)
		, m_pTx1RV(nullptr)
		, m_pVertexShader(nullptr)
		, m_pPixelShader(nullptr)
		, m_pVertexBuffer(nullptr)
		, m_pIndexBuffer(nullptr)
		, m_pCBTextureIndex(nullptr)
		, m_pSamplerLinear(nullptr)
		, m_srcWidth(srcWidth)
		, m_srcHeight(srcHeight)
		, m_hWnd(NULL)
		, m_pMatTmp(nullptr)
	{
		if (!InitializeCriticalSectionAndSpinCount(&m_csVCap, 0x00000400))
			return;
	}
	~FaceDetectionAppDX11()
	{
		if (m_pImmediateContext)
		{
			m_pImmediateContext->ClearState();
			m_pImmediateContext = nullptr;
		}

		SAFE_RELEASE(m_pSamplerLinear);
		SAFE_RELEASE(m_pCBTextureIndex);
		SAFE_RELEASE(m_pTx0RV);
		SAFE_RELEASE(m_pTx1RV);
		SAFE_RELEASE(m_pTx0);
		SAFE_RELEASE(m_pTx1);
		SAFE_RELEASE(m_pVertexBuffer);
		SAFE_RELEASE(m_pIndexBuffer);
		SAFE_RELEASE(m_pVertexLayout);
		SAFE_RELEASE(m_pVertexShader);
		SAFE_RELEASE(m_pPixelShader);
		SAFE_RELEASE(m_pDepthStencil);
		SAFE_RELEASE(m_pDepthStencilView);
		SAFE_RELEASE(m_pRenderTargetView);
		SAFE_RELEASE(m_pSwapChain1);
		SAFE_RELEASE(m_pSwapChain);
		SAFE_RELEASE(m_pImmediateContext1);
		SAFE_RELEASE(m_pImmediateContext);
		SAFE_RELEASE(m_pd3dDevice1);
		SAFE_RELEASE(m_pd3dDevice);

		m_hWnd = NULL;
		SAFE_DELETE(m_pMatTmp);

		DeleteCriticalSection(&m_csVCap);
	}
	bool Attach(HWND hWnd)
	{
		HRESULT hr = S_OK;

		RECT rc;
		GetClientRect(hWnd, &rc);
		UINT width = rc.right - rc.left;
		UINT height = rc.bottom - rc.top;

		UINT createDeviceFlags = 0;
#ifdef _DEBUG
		createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

		D3D_DRIVER_TYPE driverTypes[] =
		{
			D3D_DRIVER_TYPE_HARDWARE,
			D3D_DRIVER_TYPE_WARP,
			D3D_DRIVER_TYPE_REFERENCE,
		};
		UINT numDriverTypes = ARRAYSIZE(driverTypes);

		D3D_FEATURE_LEVEL featureLevels[] =
		{
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0,
		};
		UINT numFeatureLevels = ARRAYSIZE(featureLevels);

		for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
		{
			m_driverType = driverTypes[driverTypeIndex];
			hr = D3D11CreateDevice(nullptr, m_driverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels,
				D3D11_SDK_VERSION, &m_pd3dDevice, &m_featureLevel, &m_pImmediateContext);

			if (hr == E_INVALIDARG)
			{
				// DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
				hr = D3D11CreateDevice(nullptr, m_driverType, nullptr, createDeviceFlags, &featureLevels[1], numFeatureLevels - 1,
					D3D11_SDK_VERSION, &m_pd3dDevice, &m_featureLevel, &m_pImmediateContext);
			}

			if (SUCCEEDED(hr))
				break;
		}
		if (FAILED(hr))
			return hr;

		// Obtain DXGI factory from device (since we used nullptr for pAdapter above)
		IDXGIFactory1* dxgiFactory = nullptr;
		{
			IDXGIDevice* dxgiDevice = nullptr;
			hr = m_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
			if (SUCCEEDED(hr))
			{
				IDXGIAdapter* adapter = nullptr;
				hr = dxgiDevice->GetAdapter(&adapter);
				if (SUCCEEDED(hr))
				{
					hr = adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory));
					adapter->Release();
				}
				dxgiDevice->Release();
			}
		}
		if (FAILED(hr))
			return hr;

		// Create swap chain
		IDXGIFactory2* dxgiFactory2 = nullptr;
		hr = dxgiFactory->QueryInterface(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&dxgiFactory2));
		if (dxgiFactory2)
		{
			// DirectX 11.1 or later
			hr = m_pd3dDevice->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(&m_pd3dDevice1));
			if (SUCCEEDED(hr))
			{
				(void)m_pImmediateContext->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&m_pImmediateContext1));
			}

			DXGI_SWAP_CHAIN_DESC1 sd;
			ZeroMemory(&sd, sizeof(sd));
			sd.Width = width;
			sd.Height = height;
			sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			sd.SampleDesc.Count = 1;
			sd.SampleDesc.Quality = 0;
			sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			sd.BufferCount = 1;

			hr = dxgiFactory2->CreateSwapChainForHwnd(m_pd3dDevice, hWnd, &sd, nullptr, nullptr, &m_pSwapChain1);
			if (SUCCEEDED(hr))
			{
				hr = m_pSwapChain1->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(&m_pSwapChain));
			}

			dxgiFactory2->Release();
		}
		else
		{
			// DirectX 11.0 systems
			DXGI_SWAP_CHAIN_DESC sd;
			ZeroMemory(&sd, sizeof(sd));
			sd.BufferCount = 1;
			sd.BufferDesc.Width = width;
			sd.BufferDesc.Height = height;
			sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			sd.BufferDesc.RefreshRate.Numerator = 60;
			sd.BufferDesc.RefreshRate.Denominator = 1;
			sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			sd.OutputWindow = hWnd;
			sd.SampleDesc.Count = 1;
			sd.SampleDesc.Quality = 0;
			sd.Windowed = TRUE;

			hr = dxgiFactory->CreateSwapChain(m_pd3dDevice, &sd, &m_pSwapChain);
		}

		// Note this tutorial doesn't handle full-screen swapchains so we block the ALT+ENTER shortcut
		//dxgiFactory->MakeWindowAssociation( g_hWnd, DXGI_MWA_NO_ALT_ENTER );

		dxgiFactory->Release();

		if (FAILED(hr))
			return hr;

		// Create a render target view
		ID3D11Texture2D* pBackBuffer = nullptr;
		hr = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
		if (FAILED(hr))
			return hr;

		hr = m_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_pRenderTargetView);
		pBackBuffer->Release();
		if (FAILED(hr))
			return hr;

		// Create depth stencil texture
		D3D11_TEXTURE2D_DESC descDepth;
		ZeroMemory(&descDepth, sizeof(descDepth));
		descDepth.Width = width;
		descDepth.Height = height;
		descDepth.MipLevels = 1;
		descDepth.ArraySize = 1;
		descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		descDepth.SampleDesc.Count = 1;
		descDepth.SampleDesc.Quality = 0;
		descDepth.Usage = D3D11_USAGE_DEFAULT;
		descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		descDepth.CPUAccessFlags = 0;
		descDepth.MiscFlags = 0;
		hr = m_pd3dDevice->CreateTexture2D(&descDepth, nullptr, &m_pDepthStencil);
		if (FAILED(hr))
			return hr;

		// Create the depth stencil view
		D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
		ZeroMemory(&descDSV, sizeof(descDSV));
		descDSV.Format = descDepth.Format;
		descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		descDSV.Texture2D.MipSlice = 0;
		hr = m_pd3dDevice->CreateDepthStencilView(m_pDepthStencil, &descDSV, &m_pDepthStencilView);
		if (FAILED(hr))
			return hr;

		m_pImmediateContext->OMSetRenderTargets(1, &m_pRenderTargetView, nullptr);

		// Setup the viewport
		D3D11_VIEWPORT vp;
		vp.Width = (FLOAT)width;
		vp.Height = (FLOAT)height;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;
		m_pImmediateContext->RSSetViewports(1, &vp);

		//LoadString()

		// Compile the vertex shader
		ID3DBlob* pVSBlob = nullptr;
		hr = CompileShaderFromFile(L"FaceDetectionAppDX11.fx", "VS", "vs_4_0", &pVSBlob);
		if (FAILED(hr))
		{
			MessageBox(nullptr,
				L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
			return hr;
		}

		// Create the vertex shader
		hr = m_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &m_pVertexShader);
		if (FAILED(hr))
		{
			pVSBlob->Release();
			return hr;
		}

		// Define the input layout
		D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		UINT numElements = ARRAYSIZE(layout);

		// Create the input layout
		hr = m_pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
			pVSBlob->GetBufferSize(), &m_pVertexLayout);
		pVSBlob->Release();
		if (FAILED(hr))
			return hr;

		// Set the input layout
		m_pImmediateContext->IASetInputLayout(m_pVertexLayout);

		// Compile the pixel shader
		ID3DBlob* pPSBlob = nullptr;
		hr = CompileShaderFromFile(L"FaceDetectionAppDX11.fx", "PS", "ps_4_0", &pPSBlob);
		if (FAILED(hr))
		{
			MessageBox(nullptr,
				L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
			return hr;
		}

		// Create the pixel shader
		hr = m_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &m_pPixelShader);
		pPSBlob->Release();
		if (FAILED(hr))
			return hr;

		// Create vertex buffer
		_SimpleVertex vertices[] =
		{
			{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },
			{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) },
			{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) },
			{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },
		};

		D3D11_BUFFER_DESC bd;
		ZeroMemory(&bd, sizeof(bd));
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(_SimpleVertex) * 6;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;
		D3D11_SUBRESOURCE_DATA InitData;
		ZeroMemory(&InitData, sizeof(InitData));
		InitData.pSysMem = vertices;
		hr = m_pd3dDevice->CreateBuffer(&bd, &InitData, &m_pVertexBuffer);
		if (FAILED(hr))
			return hr;

		// Set vertex buffer
		UINT stride = sizeof(_SimpleVertex);
		UINT offset = 0;
		m_pImmediateContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);

		WORD indices[] =
		{
			2,3,0,
			1,3,2,
		};

		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(WORD) * 6;
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bd.CPUAccessFlags = 0;
		InitData.pSysMem = indices;
		hr = m_pd3dDevice->CreateBuffer(&bd, &InitData, &m_pIndexBuffer);
		if (FAILED(hr))
			return hr;

		// Set index buffe
		m_pImmediateContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

		// Set primitive topology
		m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(_CBTextureIndex);
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = 0;
		hr = m_pd3dDevice->CreateBuffer(&bd, nullptr, &m_pCBTextureIndex);
		if (FAILED(hr))
			return hr;

		m_cbTI.mTxIdx.x = 0.0;

		//// Load the Texture
		//hr = CreateDDSTextureFromFile(g_pd3dDevice, L"seafloor.dds", nullptr, &g_pTextureRV);
		//if (FAILED(hr))
		//	return hr;

		// Create the sample state
		D3D11_SAMPLER_DESC sampDesc;
		ZeroMemory(&sampDesc, sizeof(sampDesc));
		sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		sampDesc.MinLOD = 0;
		sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
		hr = m_pd3dDevice->CreateSamplerState(&sampDesc, &m_pSamplerLinear);
		if (FAILED(hr))
			return hr;

		D3D11_TEXTURE2D_DESC texDesc;
		texDesc.Width = m_srcWidth;
		texDesc.Height = m_srcHeight;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		//texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		//texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		texDesc.Format = DXGI_FORMAT_B8G8R8X8_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DYNAMIC;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		texDesc.MiscFlags = 0;

		hr = m_pd3dDevice->CreateTexture2D(&texDesc, 0, &m_pTx0);
		if (FAILED(hr))
			return hr;

		hr = m_pd3dDevice->CreateTexture2D(&texDesc, 0, &m_pTx1);
		if (FAILED(hr))
			return hr;

		/*ZeroMemory(&g_mapTx0, sizeof(D3D11_MAPPED_SUBRESOURCE));
		hr = m_pImmediateContext->Map(m_pTx0, 0, D3D11_MAP_WRITE_DISCARD, 0, &g_mapTx0);
		if (FAILED(hr))
			return hr;
		memcpy(m_mapTx0.pData, m_mat0.data, (TX_IMG_WIDTH * TX_IMG_HEIGHT * 4));
		m_pImmediateContext->Unmap(m_pTx0, 0);

		ZeroMemory(&g_mapTx1, sizeof(D3D11_MAPPED_SUBRESOURCE));
		hr = m_pImmediateContext->Map(m_pTx1, 0, D3D11_MAP_WRITE_DISCARD, 0, &g_mapTx1);
		if (FAILED(hr))
			return hr;
		memcpy(g_mapTx1.pData, m_mat1.data, (TX_IMG_WIDTH * TX_IMG_HEIGHT * 4));
		m_pImmediateContext->Unmap(m_pTx1, 0);*/

		// g_pTx0RV, g_pTx1RV
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory(&srvDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Format = texDesc.Format;
		srvDesc.Texture2D.MipLevels = texDesc.MipLevels;
		
		hr = m_pd3dDevice->CreateShaderResourceView(m_pTx0, &srvDesc, &m_pTx0RV);
		if (FAILED(hr))
			return hr;

		hr = m_pd3dDevice->CreateShaderResourceView(m_pTx1, &srvDesc, &m_pTx1RV);
		if (FAILED(hr))
			return hr;

		m_hWnd = hWnd;

		return S_OK;
	}
	bool Detach()
	{

	}
	bool OnResize()
	{

	}
	void Render()
	{
		EnterCriticalSection(&m_csVCap);
		// Clear the back buffer 
		m_pImmediateContext->ClearRenderTargetView(m_pRenderTargetView, Colors::Black);

		// Clear the depth buffer to 1.0 (max depth)
		//m_pImmediateContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

		//if (m_cbTI.mTxIdx.x == 0.0)
		m_pImmediateContext->PSSetShaderResources(0, 1, &m_pTx0RV);
		//else
		m_pImmediateContext->PSSetShaderResources(1, 1, &m_pTx1RV);
		
		//g_cbTI.mTxIdx.x = (g_cbTI.mTxIdx.x == 0.0) ? 1.0 : 0.0;
		//g_cbTI.mTxIdx.x++;
		m_pImmediateContext->UpdateSubresource(m_pCBTextureIndex, 0, nullptr, &m_cbTI, 0, 0);
		//LeaveCriticalSection(&m_csVCap);


		// Render a triangle
		m_pImmediateContext->VSSetShader(m_pVertexShader, nullptr, 0);
		m_pImmediateContext->PSSetConstantBuffers(0, 1, &m_pCBTextureIndex);
		m_pImmediateContext->PSSetShader(m_pPixelShader, nullptr, 0);
		//g_pImmediateContext->Draw( 6, 0 );
		m_pImmediateContext->PSSetSamplers(0, 1, &m_pSamplerLinear);
		m_pImmediateContext->DrawIndexed(6, 0, 0);

		//LeaveCriticalSection(&m_csVCap);

		// Present the information rendered to the back buffer to the front buffer (the screen)
		m_pSwapChain->Present(1, 0);

		LeaveCriticalSection(&m_csVCap);
	}
	FaceDetectionAppDX11& operator<<(const cv::Mat& buf)
	{
		unsigned char* pSrc = buf.data;
		if (buf.type() == CV_8UC3)
		{
			if (m_pMatTmp == nullptr)
				m_pMatTmp = new cv::Mat(m_srcHeight, m_srcWidth, CV_8UC4);
			cv::cvtColor(buf, *m_pMatTmp, CV_RGB2RGBA);
			pSrc = m_pMatTmp->data;
		}
		
		D3D11_MAPPED_SUBRESOURCE mapTx;
		ZeroMemory(&mapTx, sizeof(D3D11_MAPPED_SUBRESOURCE));

		EnterCriticalSection(&m_csVCap);
		if (m_cbTI.mTxIdx.x == 0.0)
		{
			HRESULT hr = m_pImmediateContext->Map(m_pTx1, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapTx);
			if (SUCCEEDED(hr))
			//if (SUCCEEDED(m_pImmediateContext->Map(m_pTx1, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mapTx)))
			{
				memcpy(mapTx.pData, pSrc, (m_srcWidth * m_srcHeight * 4));
				m_pImmediateContext->Unmap(m_pTx1, 0);

				m_cbTI.mTxIdx.x = 1.0;
			}
		}
		else
		{
			HRESULT hr = m_pImmediateContext->Map(m_pTx0, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapTx);
			if (SUCCEEDED(hr))
			//if (SUCCEEDED(m_pImmediateContext->Map(m_pTx0, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapTx)))
			{
				memcpy(mapTx.pData, pSrc, (m_srcWidth * m_srcHeight * 4));
				m_pImmediateContext->Unmap(m_pTx0, 0);

				m_cbTI.mTxIdx.x = 0.0;
			}
		}
		LeaveCriticalSection(&m_csVCap);

		/*ZeroMemory(&g_mapTx0, sizeof(D3D11_MAPPED_SUBRESOURCE));
		hr = m_pImmediateContext->Map(m_pTx0, 0, D3D11_MAP_WRITE_DISCARD, 0, &g_mapTx0);
		if (FAILED(hr))
		return hr;
		memcpy(m_mapTx0.pData, m_mat0.data, (TX_IMG_WIDTH * TX_IMG_HEIGHT * 4));
		m_pImmediateContext->Unmap(m_pTx0, 0);

		ZeroMemory(&g_mapTx1, sizeof(D3D11_MAPPED_SUBRESOURCE));
		hr = m_pImmediateContext->Map(m_pTx1, 0, D3D11_MAP_WRITE_DISCARD, 0, &g_mapTx1);
		if (FAILED(hr))
		return hr;
		memcpy(g_mapTx1.pData, m_mat1.data, (TX_IMG_WIDTH * TX_IMG_HEIGHT * 4));
		m_pImmediateContext->Unmap(m_pTx1, 0);*/

		return *this;
	}
private:
	HRESULT CompileShader(WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
	{
		return S_OK;
	}
	HRESULT CompileShaderFromFile(WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
	{
		HRESULT hr = S_OK;

		DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
		// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
		// Setting this flag improves the shader debugging experience, but still allows 
		// the shaders to be optimized and to run exactly the way they will run in 
		// the release configuration of this program.
		dwShaderFlags |= D3DCOMPILE_DEBUG;

		// Disable optimizations to further improve shader debugging
		dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

		ID3DBlob* pErrorBlob = nullptr;
		hr = D3DCompileFromFile(szFileName, nullptr, nullptr, szEntryPoint, szShaderModel,
			dwShaderFlags, 0, ppBlobOut, &pErrorBlob);
		if (FAILED(hr))
		{
			if (pErrorBlob)
			{
				OutputDebugStringA(reinterpret_cast<const char*>(pErrorBlob->GetBufferPointer()));
				pErrorBlob->Release();
			}
			return hr;
		}
		if (pErrorBlob) pErrorBlob->Release();

		return S_OK;
	}
private:
	D3D_DRIVER_TYPE				m_driverType;
	D3D_FEATURE_LEVEL			m_featureLevel;
	ID3D11Device*				m_pd3dDevice;
	ID3D11Device1*				m_pd3dDevice1;
	ID3D11DeviceContext*		m_pImmediateContext;
	ID3D11DeviceContext1*		m_pImmediateContext1;
	IDXGISwapChain*				m_pSwapChain;
	IDXGISwapChain1*			m_pSwapChain1;
	ID3D11RenderTargetView*		m_pRenderTargetView;
	ID3D11Texture2D*            m_pDepthStencil;
	ID3D11DepthStencilView*     m_pDepthStencilView;
	ID3D11Texture2D             *m_pTx0, *m_pTx1;
	ID3D11ShaderResourceView    *m_pTx0RV, *m_pTx1RV;
	//D3D11_MAPPED_SUBRESOURCE	m_mapTx0, m_mapTx1;
	ID3D11VertexShader*			m_pVertexShader;
	ID3D11PixelShader*			m_pPixelShader;
	ID3D11InputLayout*			m_pVertexLayout;
	ID3D11Buffer				*m_pVertexBuffer, *m_pIndexBuffer, *m_pCBTextureIndex;
	ID3D11SamplerState*			m_pSamplerLinear;
	_CBTextureIndex				m_cbTI;
private:
	int							m_srcWidth, m_srcHeight;
	HWND						m_hWnd;
	CRITICAL_SECTION			m_csVCap;
	cv::Mat*					m_pMatTmp;
	
};

class CamDrv
{
public:
	CamDrv(int devIdx, int evalReqSide, int camMaxWidth = 1920, int camMaxHeight = 1080, double camFps = 30.0)
		: m_pCap(nullptr)
		, m_evalReqSide(evalReqSide)
		, m_hReqShm(NULL)
		, m_hResShm(NULL)
		, m_pReqMv(nullptr)
		, m_pResMv(nullptr)
		, m_pResBk(nullptr)
		, m_pFDADX11(nullptr)
		, m_mmrEvtGrab(NULL)
		, m_mmrEvtRetrieve(NULL)
	{
		m_pCap = new cv::VideoCapture(devIdx);
		if ((m_pCap == nullptr) || (!m_pCap->isOpened()))
			return;

		m_pCap->set(cv::CAP_PROP_FRAME_WIDTH, camMaxWidth);
		m_pCap->set(cv::CAP_PROP_FRAME_HEIGHT, camMaxHeight);
		m_pCap->set(cv::CAP_PROP_FPS, camFps);
		m_pCap->set(cv::CAP_PROP_ZOOM, 0.0);

		if (!InitializeCriticalSectionAndSpinCount(&m_csVCap, 0x00000400))
			return;

		m_hReqShm = ::CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, SHM_REQ_SIZE, _T("__SHM_REQ_IMG"));
		m_hResShm = ::CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, SHM_RES_SIZE, _T("__SHM_RES_RECT"));

		if ((m_hReqShm == NULL) || (m_hResShm == NULL))
			return;

		m_pReqMv = ::MapViewOfFile(m_hReqShm, FILE_MAP_ALL_ACCESS, 0, 0, SHM_REQ_SIZE);
		m_pResMv = ::MapViewOfFile(m_hResShm, FILE_MAP_ALL_ACCESS, 0, 0, SHM_RES_SIZE);

		if ((m_pReqMv == nullptr) || (m_pResMv == nullptr))
			return;

		_ShmReqLayout* pShmReq = reinterpret_cast<_ShmReqLayout*>(m_pReqMv);
		memset(pShmReq->data, 0, SHM_REQ_SIZE);
		_ShmResLayout* pShmRes = reinterpret_cast<_ShmResLayout*>(m_pResMv);
		memset(pShmRes->data, 0, SHM_RES_SIZE);

		m_pResBk = static_cast<char*>(::malloc(SHM_RES_SIZE));
		m_pMatTmp = new cv::Mat(static_cast<int>(m_pCap->get(cv::CAP_PROP_FRAME_HEIGHT)), static_cast<int>(m_pCap->get(cv::CAP_PROP_FRAME_WIDTH)), CV_8UC3);

	}
	~CamDrv()
	{
		SAFE_DELETE(m_pCap);
		SAFE_DELETE(m_pMatTmp);
		SAFE_FREE(m_pResBk);

		DeleteCriticalSection(&m_csVCap);
	}
	bool IsWorking()
	{
		if ((m_pCap == nullptr) || (!m_pCap->isOpened()) || 
			(m_hReqShm == NULL) || (m_hResShm == NULL) || 
			(m_pReqMv == nullptr) || (m_pResMv == nullptr) || 
			(m_pMatTmp == nullptr))
			return false;
		return true;
	}
	int GetWidth()
	{
		if (!IsWorking())
			return 0;
		return static_cast<int>(m_pCap->get(cv::CAP_PROP_FRAME_WIDTH));
	}
	int GetHeight()
	{
		if (!IsWorking())
			return 0;
		return static_cast<int>(m_pCap->get(cv::CAP_PROP_FRAME_HEIGHT));
	}
	int GetFPS()
	{
		if (!IsWorking())
			return 0;
		return static_cast<int>(m_pCap->get(cv::CAP_PROP_FPS));
	}
public:
	bool Run(FaceDetectionAppDX11* pDx)
	{
		if ((pDx == nullptr) || (!IsWorking()))
			return false;
		m_pFDADX11 = pDx;

		// Initialize multimedia timer
		TIMECAPS tc;
		timeGetDevCaps(&tc, sizeof(TIMECAPS));
		unsigned int mmtRes = MIN(MAX(tc.wPeriodMin, 0), tc.wPeriodMax);
		timeBeginPeriod(mmtRes);

		int fps = GetFPS();

		m_mmrEvtGrab = timeSetEvent(
			1000 / (fps * 4),
			mmtRes,
			MMTCB_HighSpeedGrab,
			reinterpret_cast<DWORD_PTR>(this),
			TIME_PERIODIC);

		m_mmrEvtRetrieve = timeSetEvent(
			1000 / fps,
			mmtRes,
			MMTCB_Retrieve,
			reinterpret_cast<DWORD_PTR>(this),
			TIME_PERIODIC);

		if ((m_mmrEvtGrab == NULL) || (m_mmrEvtRetrieve == NULL))
			return false;
		return true;
	}
	void Stop()
	{
		if (m_mmrEvtGrab != NULL)
			timeKillEvent(m_mmrEvtGrab);
		if (m_mmrEvtRetrieve != NULL)
			timeKillEvent(m_mmrEvtRetrieve);

		m_pFDADX11 = nullptr;
	}
private:
	static void CALLBACK MMTCB_HighSpeedGrab(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
	{
		CamDrv* p = reinterpret_cast<CamDrv*>(dwUser);
		if (!p->IsWorking())
			return;

		EnterCriticalSection(&(p->m_csVCap));
		p->m_pCap->grab();
		LeaveCriticalSection(&(p->m_csVCap));
	}
	static void CALLBACK MMTCB_Retrieve(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
	{
		CamDrv* p = reinterpret_cast<CamDrv*>(dwUser);
		if ((p->m_pFDADX11 == nullptr) || !p->IsWorking())
			return;

		EnterCriticalSection(&(p->m_csVCap));
		bool bRetSuccess = p->m_pCap->retrieve(*(p->m_pMatTmp));
		LeaveCriticalSection(&(p->m_csVCap));

		if (!bRetSuccess)
			return;

		(*p->m_pFDADX11) << *(p->m_pMatTmp);

		// TO DO
	}
private:
	cv::VideoCapture*		m_pCap;
	int						m_evalReqSide;
	cv::Mat*				m_pMatTmp;
private:
	HANDLE					m_hReqShm, m_hResShm;
	void					*m_pReqMv, *m_pResMv;
	char*					m_pResBk;
	CRITICAL_SECTION		m_csVCap;
	MMRESULT				m_mmrEvtGrab, m_mmrEvtRetrieve;
private:
	FaceDetectionAppDX11	*m_pFDADX11;
};


//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

		// Note that this tutorial does not handle resizing (WM_SIZE) requests,
		// so we created the window without the resize border.
	// https://bell0bytes.eu/fullscreen/

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// Register class
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, (LPCTSTR)IDI_FDADX11);
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = L"FaceDetectionAppDX11Class";
	wcex.hIconSm = LoadIcon(wcex.hInstance, (LPCTSTR)IDI_FDADX11);
	if (!RegisterClassEx(&wcex))
		return E_FAIL;

	// Check & Initialize WebCam
	CamDrv* pWebCamDrv = new CamDrv(0, 320);
	if (!pWebCamDrv->IsWorking())
		return E_FAIL;
	
	// Create window
	HINSTANCE hInst = hInstance;
	RECT rc = { 0, 0, 1280, 720 };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	HWND hWnd = CreateWindow(L"FaceDetectionAppDX11Class", L"Sualab FaceDetectionApp(DX11)",
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance,
		nullptr);
	if (!hWnd)
		return E_FAIL;

	// Alloc FaceDetectionAppDX11 obj
	FaceDetectionAppDX11* pFaceDetectionAppDX11 = new FaceDetectionAppDX11(pWebCamDrv->GetWidth(), pWebCamDrv->GetHeight());
	if (FAILED(pFaceDetectionAppDX11->Attach(hWnd)))
		return E_FAIL;

	if (!pWebCamDrv->Run(pFaceDetectionAppDX11))
		return E_FAIL;

	ShowWindow(hWnd, nCmdShow);

	// Main message loop
	MSG msg = { 0 };
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			pFaceDetectionAppDX11->Render();
		}
	}

	// Dealloc FaceDetectionAppDX11 obj
	// CleanUp WebCam
	pWebCamDrv->Stop();

	SAFE_DELETE(pFaceDetectionAppDX11);
	SAFE_DELETE(pWebCamDrv);

	return (int)msg.wParam;
}