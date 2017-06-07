// A constant buffer that stores the model transform.
cbuffer ModelConstantBuffer : register(b0)
{
	float4x4 model;
};

// A constant buffer that stores each set of view and projection matrices in column-major format.
cbuffer ViewProjectionConstantBuffer : register(b1)
{
	float4   cameraPosition;
	float4   lightPosition;
	float4x4 viewProjection[2];
};

// Per-vertex data used as input to the vertex shader.
struct VertexShaderInput
{
	min16float3 pos     : POSITION;
	uint        instId  : SV_InstanceID;
};

// Per-vertex data passed to the geometry shader.
// Note that the render target array index is set here in the vertex shader.
struct VertexShaderOutput
{
	min16float4 pos     : SV_POSITION;
	uint        rtvId   : SV_RenderTargetArrayIndex; // SV_InstanceID % 2
};

// Simple shader to do vertex processing on the GPU.
VertexShaderOutput main(VertexShaderInput input)
{
	VertexShaderOutput output;
	float4 pos = float4(input.pos, 1.0f);

	// Transform the vertex position into world space.
	pos = mul(pos, model);

	// Note which view this vertex has been sent to. Used for matrix lookup.
	// Taking the modulo of the instance ID allows geometry instancing to be used
	// along with stereo instanced drawing; in that case, two copies of each 
	// instance would be drawn, one for left and one for right.
	int idx = input.instId % 2;

	// Correct for perspective and project the vertex position onto the screen.
	pos = mul(pos, viewProjection[idx]);

	output.pos = pos;

	// Set the render target array index.
	output.rtvId = idx;

	return output;
}
