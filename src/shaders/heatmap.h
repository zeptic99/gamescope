// Adapter for Gamescope from
// https://github.com/microsoft/Windows-universal-samples/blob/main/Samples/D2DAdvancedColorImages/cpp/D2DAdvancedColorImages/LuminanceHeatmapEffect.hlsl
// Licensed under MIT.

//////////////////////////////////////
// MS WCG Image Viewer Luminance Map
//////////////////////////////////////

// Nits to color mappings:
//     0.00 Black
//     3.16 Blue
//    10.0  Cyan
//    31.6  Green
//   100.0  Yellow
//   316.0  Orange
//  1000.0  Red
//  3160.0  Magenta
// 10000.0  White
// This approximates a logarithmic plot where two colors represent one order of magnitude in nits.

// Define constants based on above behavior: 9 "stops" for a piecewise linear gradient in scRGB space.
#define STOP0_NITS 0.00f
#define STOP1_NITS 3.16f
#define STOP2_NITS 10.0f
#define STOP3_NITS 31.6f
#define STOP4_NITS 100.f
#define STOP5_NITS 316.f
#define STOP6_NITS 1000.f
#define STOP7_NITS 3160.f
#define STOP8_NITS 10000.f

#define STOP0_COLOR vec3(0.0f, 0.0f, 0.0f) // Black
#define STOP1_COLOR vec3(0.0f, 0.0f, 1.0f) // Blue
#define STOP2_COLOR vec3(0.0f, 1.0f, 1.0f) // Cyan
#define STOP3_COLOR vec3(0.0f, 1.0f, 0.0f) // Green
#define STOP4_COLOR vec3(1.0f, 1.0f, 0.0f) // Yellow
#define STOP5_COLOR vec3(1.0f, 0.2f, 0.0f) // Orange
// Orange isn't a simple combination of primary colors but allows us to have 8 gradient segments,
// which gives us cleaner definitions for the nits --> color mappings.
#define STOP6_COLOR vec3(1.0f, 0.0f, 0.0f) // Red
#define STOP7_COLOR vec3(1.0f, 0.0f, 1.0f) // Magenta
#define STOP8_COLOR vec3(1.0f, 1.0f, 1.0f) // White

vec3 hdr_heatmap_impl_ms_wcg(float nits) {

  // 2: Determine which gradient segment will be used.
  // Only one of useSegmentN will be 1 (true) for a given nits value.
  float useSegment0 = sign(nits - STOP0_NITS) - sign(nits - STOP1_NITS);
  float useSegment1 = sign(nits - STOP1_NITS) - sign(nits - STOP2_NITS);
  float useSegment2 = sign(nits - STOP2_NITS) - sign(nits - STOP3_NITS);
  float useSegment3 = sign(nits - STOP3_NITS) - sign(nits - STOP4_NITS);
  float useSegment4 = sign(nits - STOP4_NITS) - sign(nits - STOP5_NITS);
  float useSegment5 = sign(nits - STOP5_NITS) - sign(nits - STOP6_NITS);
  float useSegment6 = sign(nits - STOP6_NITS) - sign(nits - STOP7_NITS);
  float useSegment7 = sign(nits - STOP7_NITS) - sign(nits - STOP8_NITS);

  // 3: Calculate the interpolated color.
  float lerpSegment0 = (nits - STOP0_NITS) / (STOP1_NITS - STOP0_NITS);
  float lerpSegment1 = (nits - STOP1_NITS) / (STOP2_NITS - STOP1_NITS);
  float lerpSegment2 = (nits - STOP2_NITS) / (STOP3_NITS - STOP2_NITS);
  float lerpSegment3 = (nits - STOP3_NITS) / (STOP4_NITS - STOP3_NITS);
  float lerpSegment4 = (nits - STOP4_NITS) / (STOP5_NITS - STOP4_NITS);
  float lerpSegment5 = (nits - STOP5_NITS) / (STOP6_NITS - STOP5_NITS);
  float lerpSegment6 = (nits - STOP6_NITS) / (STOP7_NITS - STOP6_NITS);
  float lerpSegment7 = (nits - STOP7_NITS) / (STOP8_NITS - STOP7_NITS);

  //  Only the "active" gradient segment contributes to the output color.
  return
    mix(STOP0_COLOR, STOP1_COLOR, lerpSegment0) * useSegment0 +
    mix(STOP1_COLOR, STOP2_COLOR, lerpSegment1) * useSegment1 +
    mix(STOP2_COLOR, STOP3_COLOR, lerpSegment2) * useSegment2 +
    mix(STOP3_COLOR, STOP4_COLOR, lerpSegment3) * useSegment3 +
    mix(STOP4_COLOR, STOP5_COLOR, lerpSegment4) * useSegment4 +
    mix(STOP5_COLOR, STOP6_COLOR, lerpSegment5) * useSegment5 +
    mix(STOP6_COLOR, STOP7_COLOR, lerpSegment6) * useSegment6 +
    mix(STOP7_COLOR, STOP8_COLOR, lerpSegment7) * useSegment7;
}

//////////////////////////////////////
// Nicer looking HDR heatmap by Lilium
// Has some cool greyscale stuff 8)
//////////////////////////////////////

#define LILIUM_STOP0_NITS 0.f
#define LILIUM_STOP1_NITS 100.f
#define LILIUM_STOP2_NITS 100.001f
#define LILIUM_STOP3_NITS 203.f
#define LILIUM_STOP4_NITS 400.f
#define LILIUM_STOP5_NITS 1000.f
#define LILIUM_STOP6_NITS 4000.f
#define LILIUM_STOP7_NITS 10000.f

#define LILIUM_STOP0_COLOR vec3(0.0f, 0.0f, 0.0f) // Black
#define LILIUM_STOP1_COLOR vec3(0.25f, 0.25f, 0.25f) // Black and White
#define LILIUM_STOP2_COLOR vec3(0.0f, 1.0f, 1.0f) // Cyan
#define LILIUM_STOP3_COLOR vec3(0.0f, 1.0f, 0.0f) // Green
#define LILIUM_STOP4_COLOR vec3(1.0f, 1.0f, 0.0f) // Yellow
#define LILIUM_STOP5_COLOR vec3(1.0f, 0.0f, 0.0f) // Red
#define LILIUM_STOP6_COLOR vec3(1.0f, 0.0f, 1.0f) // Pink
#define LILIUM_STOP7_COLOR vec3(0.0f, 0.0f, 1.0f) // Blue

vec3 hdr_heatmap_lilium_impl(float nits) {

    // 2: Determine which gradient segment will be used.
    // Only one of useSegmentN will be 1 (true) for a given nits value.
    float useSegment0 = sign(nits - LILIUM_STOP0_NITS) - sign(nits - LILIUM_STOP1_NITS);
    float useSegment1 = sign(nits - LILIUM_STOP1_NITS) - sign(nits - LILIUM_STOP2_NITS);
    float useSegment2 = sign(nits - LILIUM_STOP2_NITS) - sign(nits - LILIUM_STOP3_NITS);
    float useSegment3 = sign(nits - LILIUM_STOP3_NITS) - sign(nits - LILIUM_STOP4_NITS);
    float useSegment4 = sign(nits - LILIUM_STOP4_NITS) - sign(nits - LILIUM_STOP5_NITS);
    float useSegment5 = sign(nits - LILIUM_STOP5_NITS) - sign(nits - LILIUM_STOP6_NITS);
    float useSegment6 = sign(nits - LILIUM_STOP6_NITS) - sign(nits - LILIUM_STOP7_NITS);

    // 3: Calculate the interpolated color.
    float lerpSegment0 = (nits - LILIUM_STOP0_NITS) / (LILIUM_STOP1_NITS - LILIUM_STOP0_NITS);
    float lerpSegment1 = (nits - LILIUM_STOP1_NITS) / (LILIUM_STOP2_NITS - LILIUM_STOP1_NITS);
    float lerpSegment2 = (nits - LILIUM_STOP2_NITS) / (LILIUM_STOP3_NITS - LILIUM_STOP2_NITS);
    float lerpSegment3 = (nits - LILIUM_STOP3_NITS) / (LILIUM_STOP4_NITS - LILIUM_STOP3_NITS);
    float lerpSegment4 = (nits - LILIUM_STOP4_NITS) / (LILIUM_STOP5_NITS - LILIUM_STOP4_NITS);
    float lerpSegment5 = (nits - LILIUM_STOP5_NITS) / (LILIUM_STOP6_NITS - LILIUM_STOP5_NITS);
    float lerpSegment6 = (nits - LILIUM_STOP6_NITS) / (LILIUM_STOP7_NITS - LILIUM_STOP6_NITS);

    //  Only the "active" gradient segment contributes to the output color.
    return
        mix(LILIUM_STOP0_COLOR, LILIUM_STOP1_COLOR, lerpSegment0) * useSegment0 +
        mix(LILIUM_STOP1_COLOR, LILIUM_STOP2_COLOR, lerpSegment1) * useSegment1 +
        mix(LILIUM_STOP2_COLOR, LILIUM_STOP3_COLOR, lerpSegment2) * useSegment2 +
        mix(LILIUM_STOP3_COLOR, LILIUM_STOP4_COLOR, lerpSegment3) * useSegment3 +
        mix(LILIUM_STOP4_COLOR, LILIUM_STOP5_COLOR, lerpSegment4) * useSegment4 +
        mix(LILIUM_STOP5_COLOR, LILIUM_STOP6_COLOR, lerpSegment5) * useSegment5 +
        mix(LILIUM_STOP6_COLOR, LILIUM_STOP7_COLOR, lerpSegment6) * useSegment6;
}

vec3 hdr_heatmap(vec3 inputColor, bool in_2020, bool in_nits, bool out_nits) {
  vec3 xyz;

  if (!in_nits)
    inputColor *= c_nitsToLinearLightScale;

  if (in_2020)
    xyz = inputColor * rec2020_to_xyz;
  else
    xyz = inputColor * rec709_to_xyz;

  vec3 outputColor;
  if (checkDebugFlag(compositedebug_Heatmap_MSWCG))
    outputColor = hdr_heatmap_impl_ms_wcg(xyz.y); // MS WCG viewer heatmap
  else
    outputColor = hdr_heatmap_lilium_impl(xyz.y); // Lilium Heatmap

  if (c_st2084Output)
    outputColor = convert_primaries(outputColor, rec709_to_xyz, xyz_to_rec2020);

  if (out_nits)
    outputColor *= c_nitsToLinearLightScale;

  return outputColor;
}
