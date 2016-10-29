// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
    min16float4 pos   : SV_POSITION;
    min16float2 uv	  : TEXCOORD0;
};

Texture2D<float> heightmap : register(t0);
SamplerState hmsampler : register(s0) {
	Filter = MIN_MAG_MIP_LINEAR;
};

float3 estimateNormal(float2 texcoord) {
	float2 b = texcoord + float2(0.0f, -0.01f);
	float2 c = texcoord + float2(0.01f, -0.01f);
	float2 d = texcoord + float2(0.01f, 0.0f);
	float2 e = texcoord + float2(0.01f, 0.01f);
	float2 f = texcoord + float2(0.0f, 0.01f);
	float2 g = texcoord + float2(-0.01f, 0.01f);
	float2 h = texcoord + float2(-0.01f, 0.0f);
	float2 i = texcoord + float2(-0.01f, -0.01f);

	float zb = heightmap.SampleLevel(hmsampler, b, 0).x;
	float zc = heightmap.SampleLevel(hmsampler, c, 0).x;
	float zd = heightmap.SampleLevel(hmsampler, d, 0).x;
	float ze = heightmap.SampleLevel(hmsampler, e, 0).x;
	float zf = heightmap.SampleLevel(hmsampler, f, 0).x;
	float zg = heightmap.SampleLevel(hmsampler, g, 0).x;
	float zh = heightmap.SampleLevel(hmsampler, h, 0).x;
	float zi = heightmap.SampleLevel(hmsampler, i, 0).x;

	float x = zg + 2 * zh + zi - zc - 2 * zd - ze;
	float y = 2 * zb + zc + zi - ze - 2 * zf - zg;
	float z = 8.0f;

	return normalize(float3(x, y, z));
}

// The pixel shader passes through the color data. The color data from 
// is interpolated and assigned to a pixel at the rasterization step.
min16float4 main(PixelShaderInput input) : SV_TARGET {
	float3 color = { 0.0f, 1.0f, 0.0f };
	float3 norm = estimateNormal(input.uv);
	float3 light = normalize(float3(1.0f, 1.0f, -1.0f));
	float diff = saturate(dot(norm, -light));

	return min16float4(saturate(color * (diff + 0.1f)), 1.0f);
}
