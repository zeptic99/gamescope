[[vk::binding(0, 0)]] RWTexture2D<float4> outImage;

[[vk::binding(1, 0)]] cbuffer compositeDesc
{
	float flLayerCount;

    float2 flScale0;
	float2 flOffset0;
	float flOpacity0;

    float2 flScale1;
	float2 flOffset1;
	float flOpacity1;

    float2 flScale2;
	float2 flOffset2;
	float flOpacity2;

    float2 flScale3;
	float2 flOffset3;
	float flOpacity3;
}

[[vk::binding(2, 0)]] Texture2D inLayerTex0;
[[vk::binding(3, 0)]] SamplerState sampler0;

[[vk::binding(4, 0)]] Texture2D inLayerTex1;
[[vk::binding(5, 0)]] SamplerState sampler1;

[[vk::binding(6, 0)]] Texture2D inLayerTex2;
[[vk::binding(7, 0)]] SamplerState sampler2;

[[vk::binding(8, 0)]] Texture2D inLayerTex3;
[[vk::binding(9, 0)]] SamplerState sampler3;

[numthreads(16, 16, 1)]
void main(
    uint3 groupId : SV_GroupID,
    uint3 groupThreadId : SV_GroupThreadID,
    uint3 dispatchThreadId : SV_DispatchThreadID,
    uint groupIndex : SV_GroupIndex)
{
	uint2 index = uint2(dispatchThreadId.x, dispatchThreadId.y);

	float4 outputValue;

	if ( flLayerCount >= 1.0f )
	{
		outputValue = inLayerTex0.Sample( sampler0, float2( index ) * flScale0 + flOffset0 );
	}
	
	if ( flLayerCount >= 2.0f )
	{
		float4 layerSample = inLayerTex1.Sample( sampler1, float2( index ) * flScale1 + flOffset1 );
		float layerAlpha = flOpacity1 * layerSample.a;
		outputValue = layerSample * layerAlpha + outputValue * ( 1 - layerAlpha );
	}
	
    outImage [index] = outputValue;
}
