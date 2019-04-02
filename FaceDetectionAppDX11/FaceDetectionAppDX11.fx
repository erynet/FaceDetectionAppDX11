Texture2D txBuf0 : register(t0);
Texture2D txBuf1 : register(t1);
SamplerState samLinear : register(s0);

cbuffer cbTextureIndex : register(b0)
{
	float4 txIdx;
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

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
PS_INPUT VS(VS_INPUT input)
{
	PS_INPUT output = (PS_INPUT)0;
	output.Pos = input.Pos;
	output.Tex = input.Tex;

	return output;
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
	if (txIdx.x == 0.0)
		return txBuf0.Sample(samLinear, input.Tex);
	else
		return txBuf1.Sample(samLinear, input.Tex);
}