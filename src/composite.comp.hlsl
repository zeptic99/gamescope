RWTexture2D<float4> tex : register(u0);

[numthreads(32, 32, 1)]
void main(
    uint3 groupId : SV_GroupID,
    uint3 groupThreadId : SV_GroupThreadID,
    uint3 dispatchThreadId : SV_DispatchThreadID,
    uint groupIndex : SV_GroupIndex)
{
	uint2 index = uint2(dispatchThreadId.x, dispatchThreadId.y);
    tex [index] = float4(1.0f, 0.0f, 1.0f, 1.0f);
}
