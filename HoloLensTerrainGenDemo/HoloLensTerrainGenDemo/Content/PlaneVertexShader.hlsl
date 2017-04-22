// Per-vertex data used as input to the vertex shader.
struct VertexShaderInput
{
	min16float3 pos     : POSITION;
	uint        instId  : SV_InstanceID;
};

// Per-vertex data passed to the geometry shader.
// Note that the render target array index will be set by the geometry shader
// using the value of viewId.
struct VertexShaderOutput
{
	min16float4 pos     : SV_POSITION;
	uint        viewId  : TEXCOORD1;  // SV_InstanceID % 2
};

// Simple shader to do vertex processing on the GPU.
VertexShaderOutput main(VertexShaderInput input)
{
	VertexShaderOutput output;
	float4 pos = float4(input.pos, 1.0f);
	output.pos = pos;

	// Note which view this vertex has been sent to. Used for matrix lookup.
	// Taking the modulo of the instance ID allows geometry instancing to be used
	// along with stereo instanced drawing; in that case, two copies of each 
	// instance would be drawn, one for left and one for right.
	int idx = input.instId % 2;

	// Set the instance ID. The pass-through geometry shader will set the
	// render target array index to whatever value is set here.
	output.viewId = idx;

	return output;
}