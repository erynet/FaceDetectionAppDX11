#pragma once
#include <Windows.h>
#include <mfidl.h> 
#include <Mfapi.h>
#include <Mferror.h>
#include <Mfreadwrite.h>
#include <Shlwapi.h>

#pragma comment(lib,"Mfplat.lib")
#pragma comment(lib,"Mf.lib")
#pragma comment(lib,"Mfreadwrite.lib")
#pragma comment(lib,"mfuuid.lib")
#pragma comment(lib,"shlwapi.lib")

#define D3DFMT_A8R8G8B8     21
#define D3DFMT_X8R8G8B8     22

struct _FrameLayout
{
	long idx;
	void* pData;
};


class MediaFoundationCamDrv : public IMFSourceReaderCallback //this class inhertis from IMFSourceReaderCallback
{
public:
	// the class must implement the methods from IUnknown 
	STDMETHODIMP QueryInterface(REFIID iid, void** ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();
public:
	//  the class must implement the methods from IMFSourceReaderCallback 
	STDMETHODIMP OnReadSample(HRESULT status, DWORD streamIndex, DWORD streamFlags, LONGLONG timeStamp, IMFSample *sample);
	STDMETHODIMP OnEvent(DWORD, IMFMediaEvent *);
	STDMETHODIMP OnFlush(DWORD);
private:
	long					m_refCount;
	bool					m_isWorking;
	int						m_devIdx;
	UINT					m_width, m_height;
	UINT32					m_fpsNumerator, m_fpsDenominator;
	LONG					m_stride;
	int						m_bytePerPixel, m_pixelType;
	GUID					m_videoFormat;
	WCHAR					m_wDeviceNameString[2048];
	_FrameLayout			m_frameLayout;
	WCHAR*					m_pwSymbolicLink;
	UINT32                  m_cchSymbolicLink;
	IMFSourceReader*		m_pSourceReader;
	CRITICAL_SECTION		m_cs;
public:
	MediaFoundationCamDrv(int devIdx, int evalReqSide, UINT camMaxWidth, UINT camMaxHeight, UINT32 camFpsNumerator, UINT32 camFpsDenominator);
	~MediaFoundationCamDrv();
private:
	//HRESULT CreateCaptureDevice();
	HRESULT SetSourceReader(IMFActivate *device);
	bool IsFOURCCSupported(GUID* fourcc, int* dxgiFormat);
	//HRESULT IsMediaTypeSupported(IMFMediaType* type);
	//HRESULT GetSourceStride(IMFMediaType *pType, LONG *plStride);
	HRESULT Close();
public:
	bool IsWorking() const;
	int GetWidth() const;
	int GetHeight() const;
	double GetFPS() const;
	int GetPixelType() const;
	int GetBytePerPixel() const;
	const WCHAR* GetDevName() const;
	void IncFocus();
	void DecFocus();
	void IncExposure();
	void DecExposure();
	const _FrameLayout& GetFrame() const;

};
