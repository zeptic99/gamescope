[[vk::binding(0, 0)]] RWTexture2D<float4> outImage;
[[vk::binding(1, 0)]] Texture2D inLayerTex0;
[[vk::binding(2, 0)]] SamplerState sampler0;
[[vk::binding(3, 0)]] Texture2D inLayerTex1;
[[vk::binding(4, 0)]] SamplerState sampler1;
[[vk::binding(5, 0)]] Texture2D inLayerTex2;
[[vk::binding(6, 0)]] SamplerState sampler2;
[[vk::binding(7, 0)]] Texture2D inLayerTex3;
[[vk::binding(8, 0)]] SamplerState sampler3;

[numthreads(16, 16, 1)]
void main(
    uint3 groupId : SV_GroupID,
    uint3 groupThreadId : SV_GroupThreadID,
    uint3 dispatchThreadId : SV_DispatchThreadID,
    uint groupIndex : SV_GroupIndex)
{
	uint2 index = uint2(dispatchThreadId.x, dispatchThreadId.y);

	float4 outputValue = inLayerTex0.Sample( sampler0, float2( index ) );
	
    outImage [index] = outputValue;
}
