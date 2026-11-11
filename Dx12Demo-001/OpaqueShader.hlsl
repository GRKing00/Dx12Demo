
cbuffer GlobalData : register(b0, space0)
{
    row_major float4x4 VP;
}

Texture2D m_texure : register(t0, space0);
SamplerState m_sampler : register(s0, space0);

struct VSInput      
{
    float4 position : POSITION; 
    float2 texcoordUV : TEXCOORD; 
    
    float4 Matrix_Row0 : MATRIX0;
    float4 Matrix_Row1 : MATRIX1;
    float4 Matrix_Row2 : MATRIX2;
    float4 Matrix_Row3 : MATRIX3;
};

struct VSOutput    
{
    float4 position : SV_Position; 
    float2 texcoordUV : TEXCOORD; 
};

VSOutput VSMain(VSInput input)
{
    float4x4 ModelMatrix;
    VSOutput output; 
    

    ModelMatrix[0] = input.Matrix_Row0;
    ModelMatrix[1] = input.Matrix_Row1;
    ModelMatrix[2] = input.Matrix_Row2;
    ModelMatrix[3] = input.Matrix_Row3;
    
    output.position = mul(input.position, ModelMatrix); 
    output.position = mul(output.position, VP); 
    output.texcoordUV = input.texcoordUV; 
    
    return output;
}


float4 PSMain(VSOutput input) : SV_Target 
{
    return m_texure.Sample(m_sampler, input.texcoordUV); 
}

