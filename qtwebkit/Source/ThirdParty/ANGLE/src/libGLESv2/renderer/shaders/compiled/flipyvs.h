#if 0
//
// Generated by Microsoft (R) HLSL Shader Compiler 9.29.952.3111
//
//   fxc /E flipyvs /T vs_2_0 /Fh compiled/flipyvs.h Blit.vs
//
//
// Parameters:
//
//   float4 halfPixelSize;
//
//
// Registers:
//
//   Name          Reg   Size
//   ------------- ----- ----
//   halfPixelSize c0       1
//

    vs_2_0
    def c1, 0.5, 1, 0, 0
    dcl_position v0
    add oPos, v0, c0
    mad oT0, v0, c1.xxyy, c1.xxzz

// approximately 2 instruction slots used
#endif

const BYTE g_vs20_flipyvs[] =
{
      0,   2, 254, 255, 254, 255, 
     35,   0,  67,  84,  65,  66, 
     28,   0,   0,   0,  87,   0, 
      0,   0,   0,   2, 254, 255, 
      1,   0,   0,   0,  28,   0, 
      0,   0,   0,   1,   0,   0, 
     80,   0,   0,   0,  48,   0, 
      0,   0,   2,   0,   0,   0, 
      1,   0,   0,   0,  64,   0, 
      0,   0,   0,   0,   0,   0, 
    104,  97, 108, 102,  80, 105, 
    120, 101, 108,  83, 105, 122, 
    101,   0, 171, 171,   1,   0, 
      3,   0,   1,   0,   4,   0, 
      1,   0,   0,   0,   0,   0, 
      0,   0, 118, 115,  95,  50, 
     95,  48,   0,  77, 105,  99, 
    114, 111, 115, 111, 102, 116, 
     32,  40,  82,  41,  32,  72, 
     76,  83,  76,  32,  83, 104, 
     97, 100, 101, 114,  32,  67, 
    111, 109, 112, 105, 108, 101, 
    114,  32,  57,  46,  50,  57, 
     46,  57,  53,  50,  46,  51, 
     49,  49,  49,   0,  81,   0, 
      0,   5,   1,   0,  15, 160, 
      0,   0,   0,  63,   0,   0, 
    128,  63,   0,   0,   0,   0, 
      0,   0,   0,   0,  31,   0, 
      0,   2,   0,   0,   0, 128, 
      0,   0,  15, 144,   2,   0, 
      0,   3,   0,   0,  15, 192, 
      0,   0, 228, 144,   0,   0, 
    228, 160,   4,   0,   0,   4, 
      0,   0,  15, 224,   0,   0, 
    228, 144,   1,   0,  80, 160, 
      1,   0, 160, 160, 255, 255, 
      0,   0
};
