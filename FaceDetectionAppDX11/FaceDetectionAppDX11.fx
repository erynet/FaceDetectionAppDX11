Texture2D Tx : register(t0);
SamplerState LinearSampler : register(s0);
cbuffer Metadata : register(b0)
{
	float4 RenderTargetRes;
	float4 TextureRes;
};

static const float3x3 YUV_COEF =
{
	1.164f, 0.000f, 1.596f,
	1.164f, -0.392f, -0.813f,
	1.164f, 2.017f, 0.000f
};

struct VS_INPUT
{
	float4 Pos : POSITION;
	float2 Tex : TEXCOORD0;
};
struct PS_INPUT
{
	float4 Pos : SV_POSITION;
	float2 Tex : TEXCOORD0;
};

PS_INPUT VS(VS_INPUT input)
{
	PS_INPUT output = (PS_INPUT)0;
	output.Pos = input.Pos;
	output.Tex = input.Tex;
	return output;
}

/*https://docs.microsoft.com/en-us/windows/desktop/api/dxgiformat/ne-dxgiformat-dxgi_format*/

float4 PS(PS_INPUT input) : SV_Target
{
	return Tx.Sample(LinearSampler, input.Tex);
}

float4 PS_ARGBtoRGBA(PS_INPUT input) : SV_Target
{
	/*DXGI_FORMAT_R8G8B8A8_UNORM -> DXGI_FORMAT_R8G8B8A8_UNORM[ARGB]*/

	float4 argb = Tx.Sample(LinearSampler, input.Tex);
	return float4(argb.yzw, argb.x);
}

float4 PS_XRGBtoRGBA(PS_INPUT input) : SV_Target
{
	/*DXGI_FORMAT_R8G8B8A8_UNORM -> DXGI_FORMAT_R8G8B8A8_UNORM[ARGB]*/

	float4 xrgb = Tx.Sample(LinearSampler, input.Tex);
	return float4(xrgb.yzw, 1.0);
}

float4 PS_YUY2toRGBA(PS_INPUT input) : SV_Target
{
	/*DXGI_FORMAT_YUY2 -> DXGI_FORMAT_R8G8_B8G8_UNORM[UYVY]*/

	float4 yuy2 = Tx.Sample(LinearSampler, input.Tex);
	float2 outTxRes = float2(1 / (RenderTargetRes.x * 2), 1 / RenderTargetRes.x);
	float3 yuv = float3((fmod(input.Tex.x, outTxRes.x) < outTxRes.y) ? yuy2.y : yuy2.w, yuy2.x, yuy2.z);
	float3 raw_rgb = mul(YUV_COEF, yuv - float3(0.0625f, 0.5f, 0.5f));

	/*float Y = yuv.x;
	float U = yuv.y;
	float V = yuv.z;
	float R = (1.164*(Y - 0.0625f)) + (1.596*(V - 0.5f));
	float G = (1.164*(Y - 0.0625f)) - (0.391*(U - 0.5f)) - (0.813*(V - 0.5f));
	float B = (1.164*(Y - 0.0625f)) + (2.018*(U - 0.5f));*/

	return float4(saturate(raw_rgb), 1.0f);
}

float4 PS_AYUVtoRGBA(PS_INPUT input) : SV_Target
{
	/*DXGI_FORMAT_AYUV -> DXGI_FORMAT_R8G8B8A8_UNORM[VUYA]*/

	float4 ayuv = Tx.Sample(LinearSampler, input.Tex);
	float3 yuv = float3(ayuv.z, ayuv.y, ayuv.x);
	float3 raw_rgb = mul(YUV_COEF, yuv - float3(0.0625f, 0.5f, 0.5f));
	return float4(saturate(raw_rgb), ayuv.w);
}

float4 PS_NV12toRGBA(PS_INPUT input) : SV_Target
{
	return Tx.Sample(LinearSampler, input.Tex);
}