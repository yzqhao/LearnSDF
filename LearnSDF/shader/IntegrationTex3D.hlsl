
ByteAddressBuffer data; //Buffer<float>

RWTexture3D<float> dstTexture : register(u0);

float Decode(ByteAddressBuffer buf, uint InstanceId)
{
    // ByteAddressBuffer.Load() requires 4-byte aligned addresses
    // Each texel is stored as a single byte, so we need to calculate the correct 4-byte aligned offset
    uint alignedOffset = (InstanceId / 4) * 4;
    uint val = buf.Load(alignedOffset);

    // Extract the specific byte from the 4-byte word
    uint byteOffset = InstanceId % 4;
    if (byteOffset == 0)
        val = val & 0x000000FF;
    else if (byteOffset == 1)
        val = (val & 0x0000FF00) >> 8;
    else if (byteOffset == 2)
        val = (val & 0x00FF0000) >> 16;
    else if (byteOffset == 3)
        val = (val & 0xFF000000) >> 24;

    return val / 255.0f;
}

inline void Write3D(RWTexture3D<float> tex, int3 p, float val)
{
    tex[p] = val;
}

[numthreads(1, 1, 1)]
void CS(int3 DTid : SV_DispatchThreadID)
{
    int idx = DTid.x + DTid.y * 32 + DTid.z * 32 * 32;
    Write3D(dstTexture, DTid.xyz, Decode(data, idx));
}
