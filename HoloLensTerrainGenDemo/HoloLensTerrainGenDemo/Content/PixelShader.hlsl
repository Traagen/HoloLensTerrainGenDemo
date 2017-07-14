// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
    min16float4 pos   : SV_POSITION;
    min16float2 uv	  : TEXCOORD0;
	float		height : TEXCOORD1;
};

Texture2D<float> heightmap : register(t0);
Texture2DArray<float4> diffuseMaps : register(t1);

SamplerState hmsampler : register(s0);
SamplerState diffsampler : register(s1);


float4 Blend(float4 tex1, float blend1, float4 tex2, float blend2) {
	float depth = 0.2f;
	
	float ma = max(tex1.a + blend1, tex2.a + blend2) - depth;

	float b1 = max(tex1.a + blend1 - ma, 0);
	float b2 = max(tex2.a + blend2 - ma, 0);

	return (tex1 * b1 + tex2 * b2) / (b1 + b2);
}

float4 GetTexByHeightPlanar(float height, float2 uv, float low, float med, float high) {
	float bounds = 0.05f;
	float transition = 0.2f;
	float lowBlendStart = transition - 2 * bounds;
	float highBlendEnd = transition + 2 * bounds;
	float4 c;

	if (height < lowBlendStart) {
		c = diffuseMaps.Sample(diffsampler, float3(uv, low));
	}
	else if (height < transition) {
		float4 c1 = diffuseMaps.Sample(diffsampler, float3(uv, low));
		float4 c2 = diffuseMaps.Sample(diffsampler, float3(uv, med));

		float blend = (height - lowBlendStart) * (1.0f / (transition - lowBlendStart));

		c = Blend(c1, 1 - blend, c2, blend);
	}
	else if (height < highBlendEnd) {
		float4 c1 = diffuseMaps.Sample(diffsampler, float3(uv, med));
		float4 c2 = diffuseMaps.Sample(diffsampler, float3(uv, high));

		float blend = (height - transition) * (1.0f / (highBlendEnd - transition));

		c = Blend(c1, 1 - blend, c2, blend);
	}
	else {
		c = diffuseMaps.Sample(diffsampler, float3(uv, high));
	}

	return c;
}

float3 GetTexBySlope(float slope, float height, float2 uv) {
	float4 c;
	float blend;
	if (slope < 0.6f) {
		blend = slope / 0.6f;
		float4 c1 = GetTexByHeightPlanar(height, uv, 0, 2, 3);
		float4 c2 = GetTexByHeightPlanar(height, uv, 1, 2, 3);

		c = Blend(c1, 1 - blend, c2, blend);
	}
	else if (slope < 0.7f) {
		blend = (slope - 0.6f) * (1.0f / (0.7f - 0.6f));
		float4 c1 = GetTexByHeightPlanar(height, uv, 1, 2, 3);
		float4 c2 = diffuseMaps.Sample(diffsampler, float3(uv, 2));

		c = Blend(c1, 1 - blend, c2, blend);
	}
	else {
		c = diffuseMaps.Sample(diffsampler, float3(uv, 2));
	}

	return c.rgb;
}

float3 estimateNormal(float2 texcoord) {
	float2 b = texcoord + float2(0.0f, -0.01f);
	float2 c = texcoord + float2(0.01f, -0.01f);
	float2 d = texcoord + float2(0.01f, 0.0f);
	float2 e = texcoord + float2(0.01f, 0.01f);
	float2 f = texcoord + float2(0.0f, 0.01f);
	float2 g = texcoord + float2(-0.01f, 0.01f);
	float2 h = texcoord + float2(-0.01f, 0.0f);
	float2 i = texcoord + float2(-0.01f, -0.01f);

	float zb = heightmap.SampleLevel(hmsampler, b, 0).x * 50;
	float zc = heightmap.SampleLevel(hmsampler, c, 0).x * 50;
	float zd = heightmap.SampleLevel(hmsampler, d, 0).x * 50;
	float ze = heightmap.SampleLevel(hmsampler, e, 0).x * 50;
	float zf = heightmap.SampleLevel(hmsampler, f, 0).x * 50;
	float zg = heightmap.SampleLevel(hmsampler, g, 0).x * 50;
	float zh = heightmap.SampleLevel(hmsampler, h, 0).x * 50;
	float zi = heightmap.SampleLevel(hmsampler, i, 0).x * 50;

	float x = zg + 2 * zh + zi - zc - 2 * zd - ze;
	float y = 2 * zb + zc + zi - ze - 2 * zf - zg;
	float z = 8.0f;

	return normalize(float3(x, y, z));
}

min16float4 main(PixelShaderInput input) : SV_TARGET {
	float3 norm = estimateNormal(input.uv);
	float3 color = GetTexBySlope(acos(norm.z), input.height, input.uv * 10);

	float3 light = normalize(float3(1.0f, -0.5f, -1.0f));
	float diff = saturate(dot(norm, -light));

	return min16float4(saturate(color * (diff + 0.1f)), 1.0f);
}
