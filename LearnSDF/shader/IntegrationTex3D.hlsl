
ByteAddressBuffer data; //Buffer<float>

RWTexture3D<float> dstTexture : register(u0);

float Decode(ByteAddressBuffer buf, uint InstanceId)
{
    uint val = buf.Load(InstanceId / 4);
    int offset = InstanceId % 4;
    if (offset == 0)
        val = val & 0x000000FF;
    else if (offset == 1)
        val = (val & 0x0000FF00) >> 8;
    else if (offset == 2)
        val = (val & 0x00FF0000) >> 16;
    else if (offset == 3)
        val = (val & 0xFF000000) >> 24;
    return val / 255.0f; //asfloat(RectCoord0);
}

inline void Write3D(RWTexture3D<float> tex, int3 p, float val)
{
    tex[p] = val;
}

[numthreads(1, 1, 1)]
void CS(int3 DTid : SV_DispatchThreadID)
{
    int idx = DTid.x * 1024 + DTid.y * 32 + DTid.z;
    Write3D(dstTexture, DTid.xyz, Decode(data, idx));
}
