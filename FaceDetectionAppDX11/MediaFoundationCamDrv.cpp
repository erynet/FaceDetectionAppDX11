#include "MediaFoundationCamDrv.h"

#include <cstdio>
#include <vector>

int ____GCD(int n1, int n2) { return (n2 == 0) ? n1 : ____GCD(n2, n1 % n2); }

struct _NativeMediaTypeTag
{
	DWORD32 width, height;
	UINT32 numerator, denominator;
	UINT32 stride;
	int pixelType;
	GUID subtype;

	float CalcScore(int tWidth, int tHeight, int tFpsNumerator, int tFpsDenominator)
	{
		int tResGCD = ____GCD(tWidth, tHeight);
		int tWidthR = tWidth / tResGCD;
		int tHeightR = tHeight / tResGCD;

		int resGCD = ____GCD(width, height);
		int widthR = width / resGCD;
		int heightR = height / resGCD;

		if ((tWidthR != widthR) || (tHeightR != heightR))
			return 3.402822e+38;

		float tFPS = ((float)tFpsNumerator) / ((float)tFpsDenominator);
		float fps = ((float)numerator) / ((float)denominator);

		float resScale = ((float)resGCD) / ((float)tResGCD);
		float fpsScale = (fps / tFPS) * 1.33333334f;

		float score = 1.0f;
		score /= resScale;
		score /= fpsScale;

		return score;
	}
};

//From IUnknown 
STDMETHODIMP MediaFoundationCamDrv::QueryInterface(REFIID riid, void** ppvObject)
{
	static const QITAB qit[] = { QITABENT(MediaFoundationCamDrv, IMFSourceReaderCallback),{ 0 }, };
	return QISearch(this, qit, riid, ppvObject);
}
//From IUnknown
ULONG MediaFoundationCamDrv::Release()
{
	ULONG count = InterlockedDecrement(&m_refCount);
	if (count == 0)
		delete this;
	// For thread safety
	return count;
}
//From IUnknown
ULONG MediaFoundationCamDrv::AddRef()
{
	return InterlockedIncrement(&m_refCount);
}

////

//Method from IMFSourceReaderCallback
//HRESULT MediaFoundationCamDrv::OnReadSample(HRESULT status, DWORD streamIndex, DWORD streamFlags, LONGLONG timeStamp, IMFSample *sample)
HRESULT MediaFoundationCamDrv::OnReadSample(HRESULT status, DWORD, DWORD, LONGLONG, IMFSample *sample)
{
	HRESULT hr = S_OK;
	IMFMediaBuffer *mediaBuffer = NULL;

	if (FAILED(status))
		hr = status;

	EnterCriticalSection(&m_cs);
	if (SUCCEEDED(hr))
	{
		if (sample)
		{// Get the video frame buffer from the sample.
			hr = sample->GetBufferByIndex(0, &mediaBuffer);
			// Draw the frame.
			if (SUCCEEDED(hr))
			{
				//This is a good place to perform color conversion and drawing
				//Instead we're copying the data to a buffer
				BYTE* data;
				mediaBuffer->Lock(&data, NULL, NULL);
				CopyMemory(m_frameLayout.pData, data, m_width*m_height*m_bytePerPixel);
				/*if (m_frameLayout.idx == 0)
				{
					FILE* fp = fopen("sample.yuy2", "wb");
					fwrite(m_frameLayout.pData, m_width*m_height*m_bytePerPixel, 1, fp);
					fflush(fp);
					fclose(fp);
				}*/
				InterlockedIncrement(&m_frameLayout.idx);
				mediaBuffer->Release();
			}
		}
		// Request the next frame.
		hr = m_pSourceReader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, NULL, NULL, NULL, NULL);
	}
	LeaveCriticalSection(&m_cs);

	if (FAILED(hr))
	{
		//Notify there was an error
		printf("Error HRESULT = 0x%d", hr);
		PostMessage(NULL, 1, (WPARAM)hr, 0L);
	}
	
	return hr;
}
//Method from IMFSourceReaderCallback 
STDMETHODIMP MediaFoundationCamDrv::OnEvent(DWORD, IMFMediaEvent *) { return S_OK; }
//Method from IMFSourceReaderCallback 
STDMETHODIMP MediaFoundationCamDrv::OnFlush(DWORD) { return S_OK; }

////


MediaFoundationCamDrv::MediaFoundationCamDrv(int devIdx, int evalReqSide, UINT camMaxWidth = 1920, UINT camMaxHeight = 1080, UINT32 camFpsNumerator = 30000, UINT32 camFpsDenominator = 1000)
	: m_refCount(1)
	, m_isWorking(false)
	, m_devIdx(devIdx)
	, m_width(camMaxWidth)
	, m_height(camMaxHeight)
	, m_fpsNumerator(camFpsNumerator)
	, m_fpsDenominator(camFpsDenominator)
	, m_stride(0)
	, m_bytePerPixel(0)
	, m_pixelType(0)
	, m_videoFormat(GUID_NULL)
	, m_pwSymbolicLink(0)
	, m_cchSymbolicLink(0)
	, m_pSourceReader(NULL)
{
	ZeroMemory(m_wDeviceNameString, sizeof(WCHAR) * 2048);
	InitializeCriticalSection(&m_cs);
	
	HRESULT hr = S_OK;

	UINT32 count = 0;
	IMFAttributes *attributes = NULL;
	IMFActivate **devices = NULL;

	//this is important!!
	hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (FAILED(hr)) { goto CLEAN_ATTR; }
	// Create an attribute store to specify enumeration parameters.
	hr = MFCreateAttributes(&attributes, 1);
	if (FAILED(hr)) { goto CLEAN_ATTR; }

	//The attribute to be requested is devices that can capture video
	hr = attributes->SetGUID(
		MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
		MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
	);
	if (FAILED(hr)) { goto CLEAN_ATTR; }

	//Enummerate the video capture devices
	hr = MFEnumDeviceSources(attributes, &devices, &count);
	if (FAILED(hr)) { goto CLEAN_ATTR; }

	//if there are any available devices
	if (count > devIdx)
	{
		/*If you actually need to select one of the available devices
		this is the place to do it. For this example the first device
		is selected
		*/
		//Get a source reader from the first available device
		hr = SetSourceReader(devices[devIdx]);
		if (FAILED(hr)) { goto CLEAN_ATTR; }

		m_frameLayout.idx = 0;
		m_frameLayout.pData = new BYTE[m_width*m_height*m_bytePerPixel];

		WCHAR *nameString = NULL;
		// Get the human-friendly name of the device
		UINT32 cchName;
		if (SUCCEEDED(devices[devIdx]->GetAllocatedString(
			MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
			&nameString, &cchName)))
		{
			wcscpy_s(m_wDeviceNameString, nameString);
			CoTaskMemFree(nameString);
		}

		m_isWorking = true;
	}

CLEAN_ATTR:
	// clean
	if (attributes)
	{
		attributes->Release();
		attributes = NULL;
	}
	for (DWORD i = 0; i < count; i++)
	{
		if (&devices[i])
		{
			devices[i]->Release();
			devices[i] = NULL;
		}
	}
	CoTaskMemFree(devices);
}
MediaFoundationCamDrv::~MediaFoundationCamDrv()
{
	MediaFoundationCamDrv::Close();
	
	DeleteCriticalSection(&m_cs);
}

//HRESULT MediaFoundationCamDrv::CreateCaptureDevice()
//{
//	HRESULT hr = S_OK;
//
//	//this is important!!
//	hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
//
//	UINT32 count = 0;
//	IMFAttributes *attributes = NULL;
//	IMFActivate **devices = NULL;
//
//	goto CLEAN_ATTR;
//
//	if (FAILED(hr)) { goto CLEAN_ATTR; }
//	// Create an attribute store to specify enumeration parameters.
//	hr = MFCreateAttributes(&attributes, 1);
//
//	if (FAILED(hr)) { goto CLEAN_ATTR; }
//
//	//The attribute to be requested is devices that can capture video
//	hr = attributes->SetGUID(
//		MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
//		MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
//	);
//	if (FAILED(hr)) { goto CLEAN_ATTR; }
//	//Enummerate the video capture devices
//	hr = MFEnumDeviceSources(attributes, &devices, &count);
//
//	if (FAILED(hr)) { goto CLEAN_ATTR; }
//	//if there are any available devices
//	if (count > 0)
//	{
//		/*If you actually need to select one of the available devices
//		this is the place to do it. For this example the first device
//		is selected
//		*/
//		//Get a source reader from the first available device
//		SetSourceReader(devices[0]);
//
//		WCHAR *nameString = NULL;
//		// Get the human-friendly name of the device
//		UINT32 cchName;
//		hr = devices[0]->GetAllocatedString(
//			MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
//			&nameString, &cchName);
//
//		if (SUCCEEDED(hr))
//		{
//			//allocate a byte buffer for the raw pixel data
//			bytesPerPixel = abs(stride) / width;
//			rawData = new BYTE[width*height * bytesPerPixel];
//			wcscpy(deviceNameString, nameString);
//		}
//		CoTaskMemFree(nameString);
//	}
//
//CLEAN_ATTR:
//	// clean
//	if (attributes)
//	{
//		attributes->Release();
//		attributes = NULL;
//	}
//	for (DWORD i = 0; i < count; i++)
//	{
//		if (&devices[i])
//		{
//			devices[i]->Release();
//			devices[i] = NULL;
//		}
//	}
//	CoTaskMemFree(devices);
//	return hr;
//}


HRESULT MediaFoundationCamDrv::SetSourceReader(IMFActivate *device)
{
	HRESULT hr = S_OK;

	IMFMediaSource *source = NULL;
	IMFAttributes *attributes = NULL;
	IMFMediaType *mediaType = NULL;

	DWORD bestIdx = 0;
	std::vector<_NativeMediaTypeTag> vNativeMediaTypeTag;

	EnterCriticalSection(&m_cs);

	hr = device->ActivateObject(__uuidof(IMFMediaSource), (void**)&source);

	//get symbolic link for the device
	if (SUCCEEDED(hr))
		hr = device->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &m_pwSymbolicLink, &m_cchSymbolicLink);
	//Allocate attributes
	if (SUCCEEDED(hr))
		hr = MFCreateAttributes(&attributes, 2);
	//get attributes
	if (SUCCEEDED(hr))
		hr = attributes->SetUINT32(MF_READWRITE_DISABLE_CONVERTERS, TRUE);
	// Set the callback pointer.
	if (SUCCEEDED(hr))
		hr = attributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, this);
	//Create the source reader
	if (SUCCEEDED(hr))
		hr = MFCreateSourceReaderFromMediaSource(source, attributes, &m_pSourceReader);
	// Try to find a suitable output type.
	if (SUCCEEDED(hr))
	{
		float bestScore = 3.402821e+38;

		for (DWORD i = 0; ; i++)
		{
			_NativeMediaTypeTag tag;

			if (mediaType != NULL)
				mediaType->Release();

			hr = m_pSourceReader->GetNativeMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, i, &mediaType);
			if (hr == MF_E_NO_MORE_TYPES)
				break;

			if (FAILED(mediaType->GetGUID(MF_MT_SUBTYPE, &(tag.subtype))))
				continue;
			if (!MediaFoundationCamDrv::IsFOURCCSupported(&(tag.subtype), &(tag.pixelType)))
				continue;
			if (FAILED(MFGetAttributeSize(mediaType, MF_MT_FRAME_SIZE, &(tag.width), &(tag.height))))
				continue;
			if (FAILED(MFGetAttributeRatio(mediaType, MF_MT_FRAME_RATE, &(tag.numerator), &(tag.denominator))))
				continue;

			if (FAILED(mediaType->GetUINT32(MF_MT_DEFAULT_STRIDE, &(tag.stride))))
			{
				LONG tempStride;
				if (FAILED(MFGetStrideForBitmapInfoHeader(tag.subtype.Data1, tag.width, &tempStride)))
					continue;
				if (FAILED(mediaType->SetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32)tempStride)))
					continue;
				tag.stride = tempStride;
			}

			mediaType->Release();
			mediaType = NULL;

			vNativeMediaTypeTag.push_back(tag);
		}

		for (int i = 0; i < vNativeMediaTypeTag.size(); i++)
		{
			float currScore = vNativeMediaTypeTag[i].CalcScore(m_width, m_height, m_fpsNumerator, m_fpsDenominator);
			if (currScore < bestScore)
			{
				bestIdx = i;
				bestScore = currScore;
			}
		}
	}

	hr = m_pSourceReader->GetNativeMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, bestIdx, &mediaType);
	if (FAILED(hr))
		return hr;

	hr = m_pSourceReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, mediaType);
	if (FAILED(hr))
		return hr;

	mediaType->Release();
	mediaType = NULL;

	if (SUCCEEDED(m_pSourceReader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, NULL, NULL, NULL, NULL)))
	{
		m_width = vNativeMediaTypeTag[bestIdx].width;
		m_height = vNativeMediaTypeTag[bestIdx].height;
		m_fpsNumerator = vNativeMediaTypeTag[bestIdx].numerator;
		m_fpsDenominator = vNativeMediaTypeTag[bestIdx].denominator;
		m_stride = vNativeMediaTypeTag[bestIdx].stride;
		m_bytePerPixel = vNativeMediaTypeTag[bestIdx].stride / m_width;
		m_pixelType = vNativeMediaTypeTag[bestIdx].pixelType;
		m_videoFormat = vNativeMediaTypeTag[bestIdx].subtype;

		hr = S_OK;
	}
	else
	{
		if (source)
			source->Shutdown();
		Close();

		hr = S_FALSE;
	}

	if (source) { source->Release(); source = NULL; }
	if (attributes) { attributes->Release(); attributes = NULL; }
	//if (mediaType) { mediaType->Release(); mediaType = NULL; }

	LeaveCriticalSection(&m_cs);

	return hr;
}

bool MediaFoundationCamDrv::IsFOURCCSupported(GUID* fourcc, int* mediaType)
{
	if (*fourcc == MFVideoFormat_YUY2)
		*mediaType = FCC('YUY2');
	else if (*fourcc == MFVideoFormat_AYUV)
		*mediaType = FCC('AYUV');
	else if (*fourcc == MFVideoFormat_NV12)
		*mediaType = FCC('NV12');
	else if (*fourcc == MFVideoFormat_RGB32)
		*mediaType = D3DFMT_X8R8G8B8;
	else if (*fourcc == MFVideoFormat_ARGB32)
		*mediaType = D3DFMT_A8R8G8B8;
	/*else if (*fourcc == MFVideoFormat_Y210)
		*mediaType = FCC('Y210');
	else if (*fourcc == MFVideoFormat_Y216)
		*mediaType = FCC('Y216');
	else if (*fourcc == MFVideoFormat_Y410)
		*mediaType = FCC('Y410');
	else if (*fourcc == MFVideoFormat_Y416)
		*mediaType = FCC('Y416');*/
	else
		return false;
	return true;
}

//HRESULT MediaFoundationCamDrv::IsMediaTypeSupported(IMFMediaType *pType)
//{
//	HRESULT hr = S_OK;
//
//	BOOL bFound = FALSE;
//	GUID subtype = { 0 };
//
//	//Get the stride for this format so we can calculate the number of bytes per pixel
//	MediaFoundationCamDrv::GetSourceStride(pType, &m_stride);
//
//	if (FAILED(hr)) { return hr; }
//	hr = pType->GetGUID(MF_MT_SUBTYPE, &subtype);
//
//	m_videoFormat = subtype;
//
//	if (FAILED(hr)) { return hr; }
//
//	// DXGI_FORMAT_YUY2 == MFVideoFormat_YUY2, 107
//	// DXGI_FORMAT_AYUV == MFVideoFormat_AYUV, 100
//	// DXGI_FORMAT_NV12 == MFVideoFormat_NV12, 103
//	// DXGI_FORMAT_R8G8B8A8_UNORM == MFVideoFormat_ARGB32, 28
//	// DXGI_FORMAT_B8G8R8A8_UNORM == MFVideoFormat_ARGB32, 87
//	// DXGI_FORMAT_B8G8R8X8_UNORM == MFVideoFormat_RGB32, 88
//	// DXGI_FORMAT_Y210 = MFVideoFormat_Y210, 108
//	// DXGI_FORMAT_Y216 = MFVideoFormat_Y216, 109
//	// DXGI_FORMAT_Y410 = MFVideoFormat_Y410, 101
//	// DXGI_FORMAT_Y416 = MFVideoFormat_Y416, 102
//
//	if (subtype == MFVideoFormat_YUY2)
//		m_dxgiFormat = 107;
//	else if (subtype == MFVideoFormat_AYUV)
//		m_dxgiFormat = 100;
//	else if (subtype == MFVideoFormat_NV12)
//		m_dxgiFormat = 103;
//	else if (subtype == MFVideoFormat_RGB32)
//		m_dxgiFormat = 88;
//	else if (subtype == MFVideoFormat_ARGB32)
//		m_dxgiFormat = 87;
//	else if (subtype == MFVideoFormat_Y210)
//		m_dxgiFormat = 108;
//	else if (subtype == MFVideoFormat_Y216)
//		m_dxgiFormat = 109;
//	else if (subtype == MFVideoFormat_Y410)
//		m_dxgiFormat = 101;
//	else if (subtype == MFVideoFormat_Y416)
//		m_dxgiFormat = 102;
//	else
//		return hr;
//
//	return S_OK;
//}
//
////Calculates the default stride based on the format and size of the frames
//HRESULT MediaFoundationCamDrv::GetSourceStride(IMFMediaType *type, LONG *stride)
//{
//	LONG tempStride = 0;
//
//	// Try to get the default stride from the media type.
//	HRESULT hr = type->GetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32*)&tempStride);
//	if (FAILED(hr))
//	{
//		//Setting this atribute to NULL we can obtain the default stride
//		GUID subtype = GUID_NULL;
//
//		UINT32 width = 0;
//		UINT32 height = 0;
//
//		// Obtain the subtype
//		hr = type->GetGUID(MF_MT_SUBTYPE, &subtype);
//		//obtain the width and height
//		if (SUCCEEDED(hr))
//			hr = MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &width, &height);
//		//Calculate the stride based on the subtype and width
//		if (SUCCEEDED(hr))
//			hr = MFGetStrideForBitmapInfoHeader(subtype.Data1, width, &tempStride);
//		// set the attribute so it can be read
//		if (SUCCEEDED(hr))
//			(void)type->SetUINT32(MF_MT_DEFAULT_STRIDE, UINT32(tempStride));
//	}
//
//	if (SUCCEEDED(hr))
//		*stride = tempStride;
//	return hr;
//}

HRESULT MediaFoundationCamDrv::Close()
{
	EnterCriticalSection(&m_cs);
	if (m_pSourceReader)
	{
		m_pSourceReader->Release();
		m_pSourceReader = NULL;
	}

	if (m_frameLayout.pData)
	{
		delete m_frameLayout.pData;
		m_frameLayout.pData = NULL;
	}

	CoTaskMemFree(m_pwSymbolicLink);
	m_pwSymbolicLink = NULL;
	m_cchSymbolicLink = 0;

	m_isWorking = false;

	LeaveCriticalSection(&m_cs);
	return S_OK;
}

////

bool MediaFoundationCamDrv::IsWorking() const
{
	return m_isWorking;
}

int MediaFoundationCamDrv::GetWidth() const
{
	return m_width;
}

int MediaFoundationCamDrv::GetHeight() const
{
	return m_height;
}

double MediaFoundationCamDrv::GetFPS() const
{
	return (double)m_fpsNumerator / (double)m_fpsDenominator;
}

int MediaFoundationCamDrv::GetPixelType() const
{
	return m_pixelType;
}

int MediaFoundationCamDrv::GetBytePerPixel() const
{
	return m_bytePerPixel;
}

const WCHAR* MediaFoundationCamDrv::GetDevName() const
{
	return m_wDeviceNameString;
}

void MediaFoundationCamDrv::IncFocus()
{
}

void MediaFoundationCamDrv::DecFocus()
{
}

void MediaFoundationCamDrv::IncExposure()
{
}

void MediaFoundationCamDrv::DecExposure()
{
}

const _FrameLayout & MediaFoundationCamDrv::GetFrame() const
{
	return m_frameLayout;
}




