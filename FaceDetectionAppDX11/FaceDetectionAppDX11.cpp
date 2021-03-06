#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <directxcolors.h>
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"comctl32.lib")

#include "resource.h"
#include <tchar.h>

#include <vector>
#include <map>

#include <numeric>
#include <atomic>

#include "MediaFoundationCamDrv.h"

#define SAFE_FREE(p) if (p != nullptr) { ::free(p); p = nullptr; }
#define SAFE_DELETE(p) if (p != nullptr) { delete p; p = nullptr; }
#define SAFE_DELETE_ARRAY(p) if (p != nullptr) { delete[] p; p = nullptr; }
#define SAFE_DELETE_PVECTOR(v) for (int i = 0; i < v.size(); i++) { delete v[i]; v[i] = nullptr; }
#define SAFE_RELEASE(p) if (p != nullptr) { p->Release(); p = nullptr; }
#define SAFE_RELEASE_PARRAY(arr,n) for (int i = 0; i < n; i++) { if (arr[i] != nullptr) { arr[i]->Release(); arr[i] = nullptr;} }

#define BUF_SIZE			3
#define SHM_REQ_SIZE		(8+(512*512*3))
#define SHM_RES_SIZE		(2+((4*6)*32)+6)

#define MAX_HLSL_LEN		2048

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

struct _cbMetaData
{
	XMFLOAT4	RenderTargetRes;
	XMFLOAT4	TextureRes;
};

struct _Tx_Srv_DXGI
{
	DXGI_FORMAT TxFmt;
	DXGI_FORMAT SrvFmt;
	char		EntryPoint[128];

	_Tx_Srv_DXGI() : TxFmt((DXGI_FORMAT)0), SrvFmt((DXGI_FORMAT)0) {}

	_Tx_Srv_DXGI(DXGI_FORMAT txFmt, DXGI_FORMAT srvFmt, const char* entryPoint)
		: TxFmt(txFmt)
		, SrvFmt(srvFmt)
	{
		strcpy_s(EntryPoint, entryPoint);
	}
};

int ___GCD(int n1, int n2) { return (n2 == 0) ? n1 : ___GCD(n2, n1 % n2); }

//class CamDrv
//{
//public:
//	CamDrv(int devIdx, int evalReqSide, int camMaxWidth = 1920, int camMaxHeight = 1080, double camFps = 30.0)
//		: m_pCap(nullptr)
//		, m_evalReqSide(evalReqSide)
//		, m_hReqShm(NULL)
//		, m_hResShm(NULL)
//		, m_pReqMv(nullptr)
//		, m_pResMv(nullptr)
//		, m_pResBk(nullptr)
//		//, m_mmrEvtRetrieve(NULL)
//	{
//		m_pCap = new cv::VideoCapture(devIdx);
//		if ((m_pCap == nullptr) || (!m_pCap->isOpened()))
//			return;
//
//		m_pCap->set(cv::CAP_PROP_FRAME_WIDTH, camMaxWidth);
//		m_pCap->set(cv::CAP_PROP_FRAME_HEIGHT, camMaxHeight);
//		m_pCap->set(cv::CAP_PROP_FPS, camFps);
//		m_pCap->set(cv::CAP_PROP_ZOOM, 0.0);
//		
//		m_hReqShm = ::CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, SHM_REQ_SIZE, _T("__SHM_REQ_IMG"));
//		m_hResShm = ::CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, SHM_RES_SIZE, _T("__SHM_RES_RECT"));
//
//		if ((m_hReqShm == NULL) || (m_hResShm == NULL))
//			return;
//
//		m_pReqMv = ::MapViewOfFile(m_hReqShm, FILE_MAP_ALL_ACCESS, 0, 0, SHM_REQ_SIZE);
//		m_pResMv = ::MapViewOfFile(m_hResShm, FILE_MAP_ALL_ACCESS, 0, 0, SHM_RES_SIZE);
//
//		if ((m_pReqMv == nullptr) || (m_pResMv == nullptr))
//			return;
//
//		_ShmReqLayout* pShmReq = reinterpret_cast<_ShmReqLayout*>(m_pReqMv);
//		memset(pShmReq->data, 0, SHM_REQ_SIZE);
//		_ShmResLayout* pShmRes = reinterpret_cast<_ShmResLayout*>(m_pResMv);
//		memset(pShmRes->data, 0, SHM_RES_SIZE);
//
//		m_pResBk = static_cast<char*>(::malloc(SHM_RES_SIZE));
//
//		m_camPropHeight = static_cast<int>(m_pCap->get(cv::CAP_PROP_FRAME_HEIGHT));
//		m_camPropWidth = static_cast<int>(m_pCap->get(cv::CAP_PROP_FRAME_WIDTH));
//		m_camPropFPS = m_pCap->get(cv::CAP_PROP_FPS);
//
//		for (int i = 0; i < BUF_SIZE; i++)
//			m_vMatBuf.push_back(new cv::Mat(m_camPropHeight, m_camPropWidth, CV_8UC3));
//
//		m_aiFrameIdx = 0;
//		ZeroMemory(&m_FrameLayout, sizeof(m_FrameLayout));
//	}
//	~CamDrv()
//	{
//		Stop();
//
//		SAFE_DELETE(m_pCap);
//		SAFE_FREE(m_pResBk);
//		SAFE_DELETE_PVECTOR(m_vMatBuf);
//	}
//	bool IsWorking() const
//	{
//		if ((m_pCap == nullptr) || (!m_pCap->isOpened()) ||
//			(m_hReqShm == NULL) || (m_hResShm == NULL) ||
//			(m_pReqMv == nullptr) || (m_pResMv == nullptr) ||
//			(m_vMatBuf.size() == 0))
//			return false;
//		return true;
//	}
//	int GetWidth() const
//	{
//		if (!IsWorking())
//			return 0;
//		return m_camPropWidth;
//	}
//	int GetHeight() const
//	{
//		if (!IsWorking())
//			return 0;
//		return m_camPropHeight;
//	}
//	double GetFPS() const
//	{
//		if (!IsWorking())
//			return 0;
//		return m_camPropFPS;
//	}
//	void IncFocus()
//	{
//		m_pCap->set(cv::CAP_PROP_FOCUS, m_pCap->get(cv::CAP_PROP_FOCUS) + 1.0);
//	}
//	void DecFocus()
//	{
//		m_pCap->set(cv::CAP_PROP_FOCUS, m_pCap->get(cv::CAP_PROP_FOCUS) - 1.0);
//	}
//	void IncExposure()
//	{
//		m_pCap->set(cv::CAP_PROP_EXPOSURE, m_pCap->get(cv::CAP_PROP_EXPOSURE) + 1.0);
//	}
//	void DecExposure()
//	{
//		m_pCap->set(cv::CAP_PROP_EXPOSURE, m_pCap->get(cv::CAP_PROP_EXPOSURE) - 1.0);
//	}
//	const _FrameLayout& GetFrame() const
//	{
//		return m_FrameLayout;
//	}
//public:
//	bool Run()
//	{
//		if (!IsWorking())
//			return false;
//
//		//// Initialize multimedia timer
//		//TIMECAPS tc;
//		//timeGetDevCaps(&tc, sizeof(TIMECAPS));
//		//unsigned int mmtRes = MIN(MAX(tc.wPeriodMin, 0), tc.wPeriodMax);
//		//timeBeginPeriod(mmtRes);
//
//		//double fps = GetFPS();
//
//		//m_mmrEvtRetrieve = timeSetEvent(
//		//	1000 / (fps * 1.01),
//		//	mmtRes,
//		//	MMTCB_Retrieve,
//		//	reinterpret_cast<DWORD_PTR>(this),
//		//	TIME_PERIODIC);
//
//		//if (m_mmrEvtRetrieve == NULL)
//		//	return false;
//		return true;
//	}
//	
//	void Stop()
//	{
//		/*if (m_mmrEvtRetrieve != NULL)
//			timeKillEvent(m_mmrEvtRetrieve);*/
//
//		m_aiFrameIdx = 0;
//		ZeroMemory(&m_FrameLayout, sizeof(m_FrameLayout));
//	}
//private:
//	static void CALLBACK MMTCB_Retrieve(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
//	{
//		CamDrv* p = reinterpret_cast<CamDrv*>(dwUser);
//		if (!p->IsWorking())
//			return;
//
//		int curIdx = p->m_aiFrameIdx;
//		int nextSlot = (curIdx + 1) % BUF_SIZE;
//
//		bool bRetSuccess = p->m_pCap->read(*(p->m_vMatBuf[nextSlot]));
//		
//		if (!bRetSuccess)
//			return;
//
//		p->m_aiFrameIdx++;
//
//		p->m_FrameLayout.idx = p->m_aiFrameIdx;
//		p->m_FrameLayout.pMat = p->m_vMatBuf[nextSlot];
//
//		// TO DO
//	}
//private:
//	cv::VideoCapture*		m_pCap;
//	int						m_evalReqSide;
//	int						m_camPropWidth, m_camPropHeight;
//	double					m_camPropFPS;
//	std::vector<cv::Mat*>	m_vMatBuf;
//	std::atomic<int>		m_aiFrameIdx;
//	_FrameLayout			m_FrameLayout;
//private:
//	HANDLE					m_hReqShm, m_hResShm;
//	void					*m_pReqMv, *m_pResMv;
//	char*					m_pResBk;
//	//MMRESULT				m_mmrEvtRetrieve;
//};

class FaceDetectionAppDX11
{
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
	ID3D11Texture2D*            m_paTx[2];
	ID3D11ShaderResourceView*   m_paTxRV[2];
	ID3D11VertexShader*			m_pVertexShader;
	ID3D11PixelShader*			m_pPixelShader;
	ID3D11InputLayout*			m_pVertexLayout;
	ID3D11Buffer				*m_pVertexBuffer, *m_pIndexBuffer, *m_pMetaDataBuffer;
	ID3D11SamplerState*			m_pLinearSampler;
private:
	int									m_sourceWidth, m_sourceHeight;
	int									m_texturePixelType, m_textureBytePerPixel;
	int									m_frameIdx, m_frameUpdateCount;
	HINSTANCE							m_hInst;
	HWND								m_hWnd;
	bool								m_bCreateDeviceSuccess, m_bAttachSuccess;
	std::map<int, _Tx_Srv_DXGI>			m_mapDxgiTxToSrv;
private:
	_cbMetaData							m_CBMeataData;
public:
	FaceDetectionAppDX11(HINSTANCE hInstance, int srcWidth, int srcHeight, int texturePixelType, int textureBytePerPixel)
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
		, m_pVertexShader(nullptr)
		, m_pPixelShader(nullptr)
		, m_pVertexBuffer(nullptr)
		, m_pIndexBuffer(nullptr)
		, m_pMetaDataBuffer(nullptr)
		, m_pLinearSampler(nullptr)
		, m_sourceWidth(srcWidth)
		, m_sourceHeight(srcHeight)
		, m_texturePixelType(texturePixelType)
		, m_textureBytePerPixel(textureBytePerPixel)
		, m_frameIdx(0)
		, m_frameUpdateCount(0)
		, m_hInst(hInstance)
		, m_hWnd(NULL)
		, m_bCreateDeviceSuccess(false)
		, m_bAttachSuccess(false)
	{
		HRESULT hr = S_OK;

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

		// https://docs.microsoft.com/en-us/windows/desktop/api/dxgiformat/ne-dxgiformat-dxgi_format
		m_mapDxgiTxToSrv[D3DFMT_A8R8G8B8] = _Tx_Srv_DXGI(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM, "PS_ARGBtoRGBA");
		m_mapDxgiTxToSrv[D3DFMT_X8R8G8B8] = _Tx_Srv_DXGI(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM, "PS_XRGBtoRGBA");
		m_mapDxgiTxToSrv[FCC('YUY2')] = _Tx_Srv_DXGI(DXGI_FORMAT_YUY2, DXGI_FORMAT_R8G8_B8G8_UNORM, "PS_YUY2toRGBA");
		m_mapDxgiTxToSrv[FCC('AYUV')] = _Tx_Srv_DXGI(DXGI_FORMAT_AYUV, DXGI_FORMAT_R8G8B8A8_UNORM, "PS_AYUVtoRGBA");
		m_mapDxgiTxToSrv[FCC('NV12')] = _Tx_Srv_DXGI(DXGI_FORMAT_NV12, DXGI_FORMAT_R8G8_UNORM, "PS_NV12toRGBA");

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
			return;

		// Compile the vertex shader
		ID3DBlob* pVSBlob = nullptr;
		//hr = CompileShaderFromFile(L"FaceDetectionAppDX11.fx", "VS", "vs_4_0", &pVSBlob);
		hr = CompileShader("VS", "vs_4_0", &pVSBlob);
		if (FAILED(hr))
		{
			MessageBox(nullptr,
				L"The FX cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
			return;
		}

		// Create the vertex shader
		hr = m_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &m_pVertexShader);
		if (FAILED(hr))
		{
			pVSBlob->Release();
			return;
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
			return;

		// Set the input layout
		m_pImmediateContext->IASetInputLayout(m_pVertexLayout);

		// Compile the pixel shader
		ID3DBlob* pPSBlob = nullptr;
		//hr = CompileShaderFromFile(L"FaceDetectionAppDX11.fx", m_mapDxgiTxToSrv[m_texturePixelType].EntryPoint, "ps_4_0", &pPSBlob);
		hr = CompileShader(m_mapDxgiTxToSrv[m_texturePixelType].EntryPoint, "ps_4_0", &pPSBlob);
		if (FAILED(hr))
		{
			MessageBox(nullptr,
				L"The FX cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
			return;
		}

		// Create the pixel shader
		hr = m_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &m_pPixelShader);
		pPSBlob->Release();
		if (FAILED(hr))
			return;

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

		hr = m_pd3dDevice->CreateSamplerState(&sampDesc, &m_pLinearSampler);
		if (FAILED(hr))
			return;

		D3D11_TEXTURE2D_DESC texDesc;
		texDesc.Width = m_sourceWidth;
		texDesc.Height = m_sourceHeight;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		//texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		texDesc.Format = m_mapDxgiTxToSrv[m_texturePixelType].TxFmt;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DYNAMIC;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		texDesc.MiscFlags = 0;

		hr = m_pd3dDevice->CreateTexture2D(&texDesc, 0, &(m_paTx[0]));
		if (FAILED(hr))
			return;
		hr = m_pd3dDevice->CreateTexture2D(&texDesc, 0, &(m_paTx[1]));
		if (FAILED(hr))
			return;

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory(&srvDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
		srvDesc.ViewDimension = D3D_SRV_DIMENSION_TEXTURE2D;
		//srvDesc.Format = texDesc.Format;
		srvDesc.Format = m_mapDxgiTxToSrv[m_texturePixelType].SrvFmt;
		srvDesc.Texture2D.MipLevels = texDesc.MipLevels;

		hr = m_pd3dDevice->CreateShaderResourceView(m_paTx[0], &srvDesc, &(m_paTxRV[0]));
		if (FAILED(hr))
			return;
		hr = m_pd3dDevice->CreateShaderResourceView(m_paTx[1], &srvDesc, &(m_paTxRV[1]));
		if (FAILED(hr))
			return;

		m_bCreateDeviceSuccess = true;
	}
	~FaceDetectionAppDX11()
	{
		m_bCreateDeviceSuccess = false;
		m_bAttachSuccess = false;

		SAFE_RELEASE(m_pVertexBuffer);
		SAFE_RELEASE(m_pIndexBuffer);
		SAFE_RELEASE(m_pMetaDataBuffer);
		SAFE_RELEASE(m_pDepthStencil);
		SAFE_RELEASE(m_pDepthStencilView);
		SAFE_RELEASE(m_pRenderTargetView);
		SAFE_RELEASE(m_pSwapChain1);
		SAFE_RELEASE(m_pSwapChain);

		if (m_pImmediateContext)
		{
			m_pImmediateContext->ClearState();
			m_pImmediateContext = nullptr;
		}

		SAFE_RELEASE(m_pLinearSampler);
		SAFE_RELEASE_PARRAY(m_paTx, 2);
		SAFE_RELEASE_PARRAY(m_paTxRV, 2);
		SAFE_RELEASE(m_pVertexLayout);
		SAFE_RELEASE(m_pVertexShader);
		SAFE_RELEASE(m_pPixelShader);
		SAFE_RELEASE(m_pImmediateContext1);
		SAFE_RELEASE(m_pImmediateContext);
		SAFE_RELEASE(m_pd3dDevice1);
		SAFE_RELEASE(m_pd3dDevice);

		m_hWnd = NULL;
	}
	HRESULT Attach(HWND hWnd)
	{
		if (!m_bCreateDeviceSuccess)
			return S_FALSE;
		m_bAttachSuccess = false;

		SAFE_RELEASE(m_pVertexBuffer);
		SAFE_RELEASE(m_pIndexBuffer);
		SAFE_RELEASE(m_pDepthStencil);
		SAFE_RELEASE(m_pDepthStencilView);
		SAFE_RELEASE(m_pRenderTargetView);
		SAFE_RELEASE(m_pSwapChain1);
		SAFE_RELEASE(m_pSwapChain);

		HRESULT hr = S_OK;

		RECT rc;
		GetClientRect(hWnd, &rc);
		UINT renderWidth = rc.right - rc.left;
		UINT renderHeight = rc.bottom - rc.top;

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
			sd.Width = renderWidth;
			sd.Height = renderHeight;
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
			sd.BufferDesc.Width = renderWidth;
			sd.BufferDesc.Height = renderHeight;
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
		descDepth.Width = renderWidth;
		descDepth.Height = renderHeight;
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
		vp.Width = (FLOAT)renderWidth;
		vp.Height = (FLOAT)renderHeight;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;
		m_pImmediateContext->RSSetViewports(1, &vp);

		int sourceResGCD = ___GCD(m_sourceWidth, m_sourceHeight);
		int renderResGCD = ___GCD(renderWidth, renderHeight);

		int sourceResWR = m_sourceWidth / sourceResGCD;
		int sourceResHR = m_sourceHeight / sourceResGCD;
		int renderResWR = renderWidth / renderResGCD;
		int renderResHR = renderHeight / renderResGCD;

		float wMargin = 0.0f;
		float hMargin = 0.0f;

		if ((sourceResWR != renderResWR) || (sourceResHR != renderResHR))
		{
			float sourceRatio = (float)sourceResWR / (float)sourceResHR;
			float renderRatio = (float)renderResWR / (float)renderResHR;

			if (sourceRatio > renderRatio)
				hMargin = 1.0f - (renderRatio / sourceRatio);
			else if (sourceRatio < renderRatio)
				wMargin = 1.0f - (sourceRatio / renderRatio);
		}

		// Create vertex buffer
		_SimpleVertex vertices[] =
		{
			{ XMFLOAT3(-1.0f + wMargin, -1.0f + hMargin, 1.0f), XMFLOAT2(0.0f, 1.0f) },
			{ XMFLOAT3(1.0f - wMargin, 1.0f - hMargin, 1.0f), XMFLOAT2(1.0f, 0.0f) },
			{ XMFLOAT3(-1.0f + wMargin, 1.0f - hMargin, 1.0f), XMFLOAT2(0.0f, 0.0f) },
			{ XMFLOAT3(1.0f - wMargin, -1.0f + hMargin, 1.0f), XMFLOAT2(1.0f, 1.0f) },
		};

		//// VERTEX BUFFER

		D3D11_BUFFER_DESC bd;
		D3D11_SUBRESOURCE_DATA sd;

		ZeroMemory(&bd, sizeof(bd));
		ZeroMemory(&sd, sizeof(sd));
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(_SimpleVertex) * 6;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;
		sd.pSysMem = vertices;
		hr = m_pd3dDevice->CreateBuffer(&bd, &sd, &m_pVertexBuffer);
		if (FAILED(hr))
			return hr;
		// Set vertex buffer
		UINT stride = sizeof(_SimpleVertex);
		UINT offset = 0;
		m_pImmediateContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);

		//// INDEX BUFFER

		WORD indices[] =
		{
			2,3,0,
			1,3,2,
		};

		ZeroMemory(&bd, sizeof(bd));
		ZeroMemory(&sd, sizeof(sd));
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(WORD) * 6;
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bd.CPUAccessFlags = 0;
		sd.pSysMem = indices;
		hr = m_pd3dDevice->CreateBuffer(&bd, &sd, &m_pIndexBuffer);
		if (FAILED(hr))
			return hr;
		// Set index buffer
		m_pImmediateContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

		//// cbMetaData BUFFER

		m_CBMeataData.RenderTargetRes.x = (float)renderWidth;
		m_CBMeataData.RenderTargetRes.y = (float)renderHeight;
		m_CBMeataData.TextureRes.x = (float)m_sourceWidth;
		m_CBMeataData.TextureRes.y = (float)m_sourceHeight;

		ZeroMemory(&bd, sizeof(bd));
		ZeroMemory(&sd, sizeof(sd));
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(_cbMetaData);
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = 0;
		sd.pSysMem = &m_CBMeataData;
		hr = m_pd3dDevice->CreateBuffer(&bd, &sd, &m_pMetaDataBuffer);
		if (FAILED(hr))
			return hr;
		// Set buffer
		//m_pImmediateContext->UpdateSubresource(m_pMetaDataBuffer, 0, nullptr, &m_CBMeataData, 0, 0);

		// Set primitive topology
		m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		m_hWnd = hWnd;
		m_frameIdx = 0;
		m_frameUpdateCount = 0;
		m_bAttachSuccess = true;

		return S_OK;
	}
	bool Detach()
	{

	}
	bool OnResize()
	{

	}
	void Render(const _FrameLayout& frame)
	{
		if (!m_bAttachSuccess)
			return;

		if (frame.idx > m_frameIdx)
		{
			unsigned char* pSrc = reinterpret_cast<unsigned char*>(frame.pData);
			m_frameIdx = frame.idx;
			m_frameUpdateCount++;

			D3D11_MAPPED_SUBRESOURCE mapTx;
			ZeroMemory(&mapTx, sizeof(D3D11_MAPPED_SUBRESOURCE));
			HRESULT hr = m_pImmediateContext->Map(m_paTx[m_frameUpdateCount % 2], 0, D3D11_MAP_WRITE_DISCARD, 0, &mapTx);
			if (SUCCEEDED(hr))
			{
				memcpy(mapTx.pData, pSrc, (m_sourceWidth * m_sourceHeight * m_textureBytePerPixel));
				m_pImmediateContext->Unmap(m_paTx[m_frameUpdateCount % 2], 0);
			}
		}

		// Clear the back buffer 
		m_pImmediateContext->ClearRenderTargetView(m_pRenderTargetView, Colors::Black);

		// Clear the depth buffer to 1.0 (max depth)
		//m_pImmediateContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

		m_pImmediateContext->PSSetShaderResources(0, 1, &(m_paTxRV[m_frameUpdateCount % 2]));

		// Render a triangle
		m_pImmediateContext->VSSetShader(m_pVertexShader, nullptr, 0);
		m_pImmediateContext->PSSetShader(m_pPixelShader, nullptr, 0);
		m_pImmediateContext->PSSetSamplers(0, 1, &m_pLinearSampler);
		m_pImmediateContext->PSSetConstantBuffers(0, 1, &m_pMetaDataBuffer);
		m_pImmediateContext->DrawIndexed(6, 0, 0);

		// Present the information rendered to the back buffer to the front buffer (the screen)
		m_pSwapChain->Present(1, 0);
	}
private:
	HRESULT CompileShader(LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
	{
		HRESULT hr = S_OK;

		CHAR hlsl[MAX_HLSL_LEN];
		ZeroMemory(hlsl, sizeof(hlsl));
		LoadStringA(m_hInst, IDS_HLSL, hlsl, MAX_HLSL_LEN);
		size_t hlslLen = strlen(hlsl);

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
		hr = D3DCompile(hlsl, hlslLen, NULL, nullptr, nullptr, szEntryPoint, szShaderModel,
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
};


//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	MediaFoundationCamDrv* pMediaFoundationCamDrv = nullptr;

	switch (message)
	{
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_KEYUP:
		pMediaFoundationCamDrv = reinterpret_cast<MediaFoundationCamDrv*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

		if ((wParam == VK_NUMPAD2) || (wParam == VK_DOWN))
		{
			pMediaFoundationCamDrv->DecExposure();
		}
		else if ((wParam == VK_NUMPAD2) || (wParam == VK_UP))
		{
			pMediaFoundationCamDrv->IncExposure();
		}
		else if ((wParam == VK_NUMPAD4) || (wParam == VK_LEFT))
		{
			pMediaFoundationCamDrv->DecFocus();
		}
		else if ((wParam == VK_NUMPAD6) || (wParam == VK_RIGHT))
		{
			pMediaFoundationCamDrv->IncFocus();
		}

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
	MediaFoundationCamDrv* pMediaFoundationCamDrv = new MediaFoundationCamDrv(0, 320, 1920, 1080, 30000, 1000);
	if (!pMediaFoundationCamDrv->IsWorking())
		return E_FAIL;
	
	// Create window
	HINSTANCE hInst = hInstance;
	RECT rc = { 0, 0, 1920, 1080 };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	HWND hWnd = CreateWindow(L"FaceDetectionAppDX11Class", pMediaFoundationCamDrv->GetDevName()/*L"Sualab FaceDetectionApp(DX11)"*/,
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance,
		nullptr);
	if (!hWnd)
		return E_FAIL;

	SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pMediaFoundationCamDrv);
	
	// Alloc FaceDetectionAppDX11 obj
	FaceDetectionAppDX11* pFaceDetectionAppDX11 = new FaceDetectionAppDX11(hInst, pMediaFoundationCamDrv->GetWidth(), pMediaFoundationCamDrv->GetHeight(), 
		pMediaFoundationCamDrv->GetPixelType(), pMediaFoundationCamDrv->GetBytePerPixel());
	if (FAILED(pFaceDetectionAppDX11->Attach(hWnd)))
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

			continue;
		}
		pFaceDetectionAppDX11->Render(pMediaFoundationCamDrv->GetFrame());
	}

	// Dealloc FaceDetectionAppDX11 obj
	// CleanUp WebCam
	
	SAFE_DELETE(pFaceDetectionAppDX11);
	SAFE_DELETE(pMediaFoundationCamDrv);

	return (int)msg.wParam;
}