#pragma once

namespace HoloLensTerrainGenDemo
{
    // Constant buffer used to send hologram position transform to the shader pipeline.
    struct ModelConstantBuffer
    {
		DirectX::XMFLOAT4X4 modelToWorld;
    };

    // Assert that the constant buffer remains 16-byte aligned (best practice).
    static_assert((sizeof(ModelConstantBuffer) % (sizeof(float) * 4)) == 0, "Model constant buffer size must be 16-byte aligned (16 bytes is the length of four floats).");


    // Used to send per-vertex data to the vertex shader. Used by Terrain.
    struct Vertex {
        DirectX::XMFLOAT3 pos;
        DirectX::XMFLOAT2 uv;
    };

	// Assert that the constant buffer remains 16-byte aligned (best practice).
	// If shader structure members are not aligned to a 4-float boundary, data may
	// not show up where it is expected by the time it is read by the shader.
	static_assert((sizeof(ModelConstantBuffer) % (sizeof(float) * 4)) == 0, "Model constant buffer size must be 16-byte aligned (16 bytes is the length of four floats).");

	// Constant buffer used to send hologram position and normal transforms to the shader pipeline.
	struct ModelNormalConstantBuffer
	{
		DirectX::XMFLOAT4X4 modelToWorld;
		DirectX::XMFLOAT4X4 normalToWorld;
		DirectX::XMFLOAT4   colorFadeFactor;
	};

	// Assert that the constant buffer remains 16-byte aligned (best practice).
	// If shader structure members are not aligned to a 4-float boundary, data may
	// not show up where it is expected by the time it is read by the shader.
	static_assert((sizeof(ModelNormalConstantBuffer) % (sizeof(float) * 4)) == 0, "Model/normal constant buffer size must be 16-byte aligned (16 bytes is the length of four floats).");

	// Constant buffer used to send the view-projection matrices to the shader pipeline.
	struct ViewProjectionConstantBuffer
	{
		DirectX::XMFLOAT4   cameraPosition;
		DirectX::XMFLOAT4   lightPosition;
		DirectX::XMFLOAT4X4 viewProjection[2];
	};

	// Assert that the constant buffer remains 16-byte aligned (best practice).
	static_assert((sizeof(ViewProjectionConstantBuffer) % (sizeof(float) * 4)) == 0, "View/projection constant buffer size must be 16-byte aligned (16 bytes is the length of four floats).");
}