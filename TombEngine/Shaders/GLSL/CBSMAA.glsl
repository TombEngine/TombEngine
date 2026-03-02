#ifndef CBSMAA
#define CBSMAA

layout(std140, binding = 13) uniform SMAABuffer
{
    vec4 subsampleIndices;
    float blendFactor;
    float threshld;
    float maxSearchSteps;
    float maxSearchStepsDiag;
    //--
    float cornerRounding;
    vec3 SMAABuffer_Padding0;
};

#endif
