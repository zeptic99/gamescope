/////////////////////////////
// SRGB Encoding Helpers
/////////////////////////////

// Go from sRGB encoding -> linear
vec3 srgbToLinear(vec3 color) {
    bvec3 isLo = lessThanEqual(color, vec3(0.04045f));

    vec3 loPart = color / 12.92f;
    vec3 hiPart = pow((color + 0.055f) / 1.055f, vec3(12.0f / 5.0f));
    return mix(hiPart, loPart, isLo);
}

vec4 srgbToLinear(vec4 color) {
  return vec4(srgbToLinear(color.rgb), color.a);
}

// Go from linear -> sRGB encoding.
vec3 linearToSrgb(vec3 color) {
    bvec3 isLo = lessThanEqual(color, vec3(0.0031308f));

    vec3 loPart = color * 12.92f;
    vec3 hiPart = pow(color, vec3(5.0f / 12.0f)) * 1.055f - 0.055f;
    return mix(hiPart, loPart, isLo);
}

vec4 linearToSrgb(vec4 color) {
  return vec4(linearToSrgb(color.rgb), color.a);
}

/////////////////////////////
// PQ Encoding Helpers
/////////////////////////////

// Converts nits -> pq and pq -> nits
// Does NOT affect primaries at all.
vec3 nitsToPq(vec3 nits) {
    vec3 y = clamp(nits / 10000.0, vec3(0.0), vec3(1.0));
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    const float m1 = 0.1593017578125;
    const float m2 = 78.84375;
    vec3 num = c1 + c2 * pow(y, vec3(m1));
    vec3 den = 1.0 + c3 * pow(y, vec3(m1));
    vec3 n = pow(num / den, vec3(m2));
    return n;
}

vec3 pqToNits(vec3 pq) {
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;

    const float oo_m1 = 1.0 / 0.1593017578125;
    const float oo_m2 = 1.0 / 78.84375;

    vec3 num = max(pow(pq, vec3(oo_m2)) - c1, vec3(0.0));
    vec3 den = c2 - c3 * pow(pq, vec3(oo_m2));

    return 10000.0 * pow(num / den, vec3(oo_m1));
}

// pq -> nits -> linear (nits / 80)
const float c_nitsToLinearLightScale = 80.0f;
const float c_scRGBLightScale = 80.0f;
vec3 nitsToLinear(vec3 nits) {
    // This is typical, but we might want to make this customizable.
    return nits / c_nitsToLinearLightScale;
}

vec3 linearToNits(vec3 linear) {
    return linear * c_nitsToLinearLightScale;
}

/////////////////////////////
// Primary Conversion Helpers
/////////////////////////////

struct PrimaryInfo {
    vec2 displayPrimaryRed;
    vec2 displayPrimaryGreen;
    vec2 displayPrimaryBlue;
    vec2 whitePoint;
};

vec3 convert_primary(vec2 xy) {
	float X = xy.x / xy.y;
	float Y = 1.0f;
	float Z = (1.0f - xy.x - xy.y) / xy.y;
	return vec3(X, Y, Z);
}

mat3 compute_xyz_matrix(PrimaryInfo metadata) {
	vec3 red = convert_primary(metadata.displayPrimaryRed);
	vec3 green = convert_primary(metadata.displayPrimaryGreen);
	vec3 blue = convert_primary(metadata.displayPrimaryBlue);
	vec3 white = convert_primary(metadata.whitePoint);

	vec3 component_scale = inverse(mat3(red, green, blue)) * white;
	return transpose(mat3(red * component_scale.x, green * component_scale.y, blue * component_scale.z));
}

const PrimaryInfo rec709_primaries = {
    vec2(0.640f, 0.330f),     // red
    vec2(0.300f, 0.600f),     // green
    vec2(0.150f, 0.060f),     // blue
    vec2(0.3127f, 0.3290f),   // whitepoint
};
/*const*/ mat3 rec709_to_xyz = compute_xyz_matrix(rec709_primaries);
/*const*/ mat3 xyz_to_rec709 = inverse(rec709_to_xyz);

const PrimaryInfo rec2020_primaries = {
    vec2(0.708f, 0.292f),     // red
    vec2(0.170f, 0.797f),     // green
    vec2(0.131f, 0.046f),     // blue
    vec2(0.3127f, 0.3290f),   // whitepoint
};
/*const*/ mat3 rec2020_to_xyz = compute_xyz_matrix(rec2020_primaries);
/*const*/ mat3 xyz_to_rec2020 = inverse(rec2020_to_xyz);

vec3 convert_primaries(vec3 color, mat3 src_to_xyz, mat3 xyz_to_dst) {
    return color * mat3(src_to_xyz * xyz_to_dst);
}

#include "heatmap.h"
