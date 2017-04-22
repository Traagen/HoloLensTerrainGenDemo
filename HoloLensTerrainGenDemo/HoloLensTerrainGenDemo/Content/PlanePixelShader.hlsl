// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
	min16float4 pos   : SV_POSITION;
};

min16float4 main(PixelShaderInput input) : SV_TARGET{
	return float4(0.0f, 0.0f, 1.0f, 1.0f);
}
