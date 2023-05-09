#include "color_helpers.h"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <fstream>
#include <string>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat3x3.hpp>
#include <glm/gtx/matrix_operation.hpp>
#include <glm/gtx/string_cast.hpp>


glm::vec3 xyY_to_XYZ( const glm::vec2 & xy, float Y )
{
    if ( fabsf( xy.y ) < std::numeric_limits<float>::min() )
    {
        return glm::vec3( 0.f );
    }
    else
    {
        return glm::vec3( Y * xy.x / xy.y, Y, ( 1.f - xy.x - xy.y ) * Y / xy.y );
    }
}

glm::vec2 XYZ_to_xy( const glm::vec3 & XYZ )
{
    float sum = ( XYZ.x + XYZ.y + XYZ.z );
    if ( fabsf( sum ) < std::numeric_limits<float>::min() )
    {
        return glm::vec2( 0.f );
    }
    else
    {
        return glm::vec2( XYZ.x / sum, XYZ.y / sum );
    }
}

glm::vec3 xy_to_xyz( const glm::vec2 & xy )
{
    return glm::vec3( xy.x, xy.y, 1.f - xy.x - xy.y );
}

// Convert xy to CIE 1976 u'v'
glm::vec2 xy_to_uv( const glm::vec2 & xy )
{
    float denom = -2.f * xy.x + 12.f * xy.y + 3.f;
    if ( fabsf( denom ) < std::numeric_limits<float>::min() )
    {
        return glm::vec2( 0.f );
    }

    return glm::vec2( 4.f * xy.x / denom, 9.f * xy.y / denom );
}

// Convert CIE 1976 u'v' to xy
glm::vec2 uv_to_xy( const glm::vec2 & uv )
{
    float denom = 6.f * uv.x - 16.f * uv.y + 12.f;
    if ( fabsf( denom ) < std::numeric_limits<float>::min() )
    {
        return glm::vec2( 0.f );
    }

    return glm::vec2( 9.f * uv.x / denom, 4.f * uv.y / denom );
}

glm::mat3 normalised_primary_matrix( const primaries_t & rgbPrimaries, const glm::vec2 & whitePrimary, float whiteLuminance )
{
    glm::mat3 matPrimaries( xy_to_xyz( rgbPrimaries.r ), xy_to_xyz( rgbPrimaries.g ), xy_to_xyz( rgbPrimaries.b ) );
    glm::vec3 whiteXYZ = xyY_to_XYZ( whitePrimary, whiteLuminance );
    glm::mat3 whiteScale = glm::diagonal3x3( glm::inverse( matPrimaries ) * whiteXYZ );
    return matPrimaries * whiteScale;
}

enum EChromaticAdaptationMethod
{
    k_EChromaticAdapatationMethod_XYZ,
    k_EChromaticAdapatationMethod_Bradford,
};

glm::mat3 chromatic_adaptation_matrix( const glm::vec3 & sourceWhite, const glm::vec3 & destWhite, EChromaticAdaptationMethod eMethod )
{
    static const glm::mat3 k_matBradford( 0.8951f,-0.7502f, 0.0389f, 0.2664f,1.7135f,  -0.0685f, -0.1614f,  0.0367f, 1.0296f );
    glm::mat3 matAdaptation = eMethod == k_EChromaticAdapatationMethod_XYZ ? glm::diagonal3x3( glm::vec3(1,1,1) ) : k_matBradford;
    glm::vec3 coneResponseDest = matAdaptation * destWhite;
    glm::vec3 coneResponseSource = matAdaptation * sourceWhite;
    glm::vec3 scale = glm::vec3( coneResponseDest.x / coneResponseSource.x, coneResponseDest.y / coneResponseSource.y, coneResponseDest.z / coneResponseSource.z );
    return glm::inverse( matAdaptation ) * glm::diagonal3x3( scale ) * matAdaptation;
}

displaycolorimetry_t lerp( const displaycolorimetry_t & a, const displaycolorimetry_t & b, float t )
{
    displaycolorimetry_t result;
    primaries_t a_uv, b_uv;
    a_uv.r = xy_to_uv( a.primaries.r );
    a_uv.g = xy_to_uv( a.primaries.g );
    a_uv.b = xy_to_uv( a.primaries.b );

    b_uv.r = xy_to_uv( b.primaries.r );
    b_uv.g = xy_to_uv( b.primaries.g );
    b_uv.b = xy_to_uv( b.primaries.b );

    glm::vec2 a_white = xy_to_uv( a.white );
    glm::vec2 b_white = xy_to_uv( b.white );

    result.primaries.r.x = flerp( a_uv.r.x, b_uv.r.x, t );
    result.primaries.r.y = flerp( a_uv.r.y, b_uv.r.y, t );
    result.primaries.g.x = flerp( a_uv.g.x, b_uv.g.x, t );
    result.primaries.g.y = flerp( a_uv.g.y, b_uv.g.y, t );
    result.primaries.b.x = flerp( a_uv.b.x, b_uv.b.x, t );
    result.primaries.b.y = flerp( a_uv.b.y, b_uv.b.y, t );
    result.white.x = flerp( a_white.x, b_white.x, t );
    result.white.y = flerp( a_white.y, b_white.y, t );

    result.primaries.r = uv_to_xy( result.primaries.r );
    result.primaries.g = uv_to_xy( result.primaries.g );
    result.primaries.b = uv_to_xy( result.primaries.b );
    result.white = uv_to_xy( result.white );

    return result;
}

colormapping_t lerp( const colormapping_t & a, const colormapping_t & b, float t )
{
    colormapping_t result;
    result.blendEnableMinSat = flerp( a.blendEnableMinSat, b.blendEnableMinSat, t );
    result.blendEnableMaxSat = flerp( a.blendEnableMaxSat, b.blendEnableMaxSat, t );
    result.blendAmountMin = flerp( a.blendAmountMin, b.blendAmountMin, t );
    result.blendAmountMax = flerp( a.blendAmountMax, b.blendAmountMax, t );
    return result;
}

bool LoadCubeLut( lut3d_t * lut3d, const char * filename )
{
    // R changes fastest
    // ...
    // LUT_3D_SIZE %d(lutEdgeSize)
    // %f %f %f
    // ...

    lut3d->lutEdgeSize = 0;
    lut3d->data.clear();

    std::ifstream lutfile( filename );
    if ( !lutfile.is_open() || lutfile.bad() )
        return false;

    std::string line;
    while ( std::getline( lutfile, line ) )
    {
        if ( lut3d->lutEdgeSize )
        {
            glm::vec3 val;
            if ( sscanf( line.c_str(), "%f %f %f", &val.r, &val.g, &val.b ) == 3 )
            {
                lut3d->data.push_back( val );
            }
        }
        else if ( sscanf( line.c_str(), "LUT_3D_SIZE %d", &lut3d->lutEdgeSize ) == 1 )
        {
            if ( lut3d->lutEdgeSize < 2 || lut3d->lutEdgeSize > 128 ) // sanity check
            {
                return false;
            }
            lut3d->data.reserve( lut3d->lutEdgeSize * lut3d->lutEdgeSize * lut3d->lutEdgeSize );
        }
    }

    int nExpectedElements = lut3d->lutEdgeSize * lut3d->lutEdgeSize * lut3d->lutEdgeSize;
    bool bValid = ( nExpectedElements > 0 && ( nExpectedElements == (int) lut3d->data.size() ) );
    if ( !bValid )
    {
        lut3d->lutEdgeSize = 0;
        lut3d->data.clear();
    }

    return bValid;
}

int GetLut3DIndexRedFastRGB(int indexR, int indexG, int indexB, int dim)
{
    return (indexR + (int)dim * (indexG + (int)dim * indexB));
}

// Linear
inline void lerp_rgb(float* out, const float* a, const float* b, const float* z)
{
    out[0] = (b[0] - a[0]) * z[0] + a[0];
    out[1] = (b[1] - a[1]) * z[1] + a[1];
    out[2] = (b[2] - a[2]) * z[2] + a[2];
}

// Bilinear
inline void lerp_rgb(float* out, const float* a, const float* b, const float* c,
                     const float* d, const float* y, const float* z)
{
    float v1[3];
    float v2[3];
    lerp_rgb(v1, a, b, z);
    lerp_rgb(v2, c, d, z);
    lerp_rgb(out, v1, v2, y);
}

// Trilinear
inline void lerp_rgb(float* out, const float* a, const float* b, const float* c, const float* d,
                     const float* e, const float* f, const float* g, const float* h,
                     const float* x, const float* y, const float* z)
{
    float v1[3];
    float v2[3];
    lerp_rgb(v1, a,b,c,d,y,z);
    lerp_rgb(v2, e,f,g,h,y,z);
    lerp_rgb(out, v1, v2, x);
}

float ClampAndSanitize( float a, float min, float max )
{
    return std::isfinite( a ) ? std::min(std::max(min, a), max) : min;
}

// Adapted from:
// https://github.com/AcademySoftwareFoundation/OpenColorIO/ops/lut3d/Lut3DOpCPU.cpp
// License available in their repo and in our LICENSE file.

glm::vec3 ApplyLut3D_Trilinear( const lut3d_t & lut3d, const glm::vec3 & input )
{
    const float dimMinusOne = float(lut3d.lutEdgeSize) - 1.f;

    float idx[3];
    idx[0] = input.r * dimMinusOne;
    idx[1] = input.g * dimMinusOne;
    idx[2] = input.b * dimMinusOne;

    // NaNs become 0.
    idx[0] = ClampAndSanitize(idx[0], 0.f, dimMinusOne);
    idx[1] = ClampAndSanitize(idx[1], 0.f, dimMinusOne);
    idx[2] = ClampAndSanitize(idx[2], 0.f, dimMinusOne);

    int indexLow[3];
    indexLow[0] = static_cast<int>(std::floor(idx[0]));
    indexLow[1] = static_cast<int>(std::floor(idx[1]));
    indexLow[2] = static_cast<int>(std::floor(idx[2]));

    int indexHigh[3];
    // When the idx is exactly equal to an index (e.g. 0,1,2...)
    // then the computation of highIdx is wrong. However,
    // the delta is then equal to zero (e.g. idx-lowIdx),
    // so the highIdx has no impact.
    indexHigh[0] = static_cast<int>(std::ceil(idx[0]));
    indexHigh[1] = static_cast<int>(std::ceil(idx[1]));
    indexHigh[2] = static_cast<int>(std::ceil(idx[2]));

    float delta[3];
    delta[0] = idx[0] - static_cast<float>(indexLow[0]);
    delta[1] = idx[1] - static_cast<float>(indexLow[1]);
    delta[2] = idx[2] - static_cast<float>(indexLow[2]);

    // Compute index into LUT for surrounding corners
    const int n000 =
        GetLut3DIndexRedFastRGB(indexLow[0], indexLow[1], indexLow[2], lut3d.lutEdgeSize);
    const int n100 =
        GetLut3DIndexRedFastRGB(indexHigh[0], indexLow[1], indexLow[2], lut3d.lutEdgeSize);
    const int n010 =
        GetLut3DIndexRedFastRGB(indexLow[0], indexHigh[1], indexLow[2], lut3d.lutEdgeSize);
    const int n001 =
        GetLut3DIndexRedFastRGB(indexLow[0], indexLow[1], indexHigh[2], lut3d.lutEdgeSize);
    const int n110 =
        GetLut3DIndexRedFastRGB(indexHigh[0], indexHigh[1], indexLow[2], lut3d.lutEdgeSize);
    const int n101 =
        GetLut3DIndexRedFastRGB(indexHigh[0], indexLow[1], indexHigh[2], lut3d.lutEdgeSize);
    const int n011 =
        GetLut3DIndexRedFastRGB(indexLow[0], indexHigh[1], indexHigh[2], lut3d.lutEdgeSize);
    const int n111 =
        GetLut3DIndexRedFastRGB(indexHigh[0], indexHigh[1], indexHigh[2], lut3d.lutEdgeSize);

    float x[3], y[3], z[3];
    x[0] = delta[0]; x[1] = delta[0]; x[2] = delta[0];
    y[0] = delta[1]; y[1] = delta[1]; y[2] = delta[1];
    z[0] = delta[2]; z[1] = delta[2]; z[2] = delta[2];

    glm::vec3 out;
    lerp_rgb((float *) &out,
        (float *) &lut3d.data[n000].r, (float *) &lut3d.data[n001].r,
        (float *) &lut3d.data[n010].r, (float *) &lut3d.data[n011].r,
        (float *) &lut3d.data[n100].r, (float *) &lut3d.data[n101].r,
        (float *) &lut3d.data[n110].r, (float *) &lut3d.data[n111].r,
        x, y, z);

    return out;
}

glm::vec3 ApplyLut3D_Tetrahedral( const lut3d_t & lut3d, const glm::vec3 & input )
{
    const float dimMinusOne = float(lut3d.lutEdgeSize) - 1.f;

    float idx[3];
    idx[0] = input.r * dimMinusOne;
    idx[1] = input.g * dimMinusOne;
    idx[2] = input.b * dimMinusOne;

    // NaNs become 0.
    idx[0] = ClampAndSanitize(idx[0], 0.f, dimMinusOne);
    idx[1] = ClampAndSanitize(idx[1], 0.f, dimMinusOne);
    idx[2] = ClampAndSanitize(idx[2], 0.f, dimMinusOne);

    int indexLow[3];
    indexLow[0] = static_cast<int>(std::floor(idx[0]));
    indexLow[1] = static_cast<int>(std::floor(idx[1]));
    indexLow[2] = static_cast<int>(std::floor(idx[2]));

    int indexHigh[3];
    // When the idx is exactly equal to an index (e.g. 0,1,2...)
    // then the computation of highIdx is wrong. However,
    // the delta is then equal to zero (e.g. idx-lowIdx),
    // so the highIdx has no impact.
    indexHigh[0] = static_cast<int>(std::ceil(idx[0]));
    indexHigh[1] = static_cast<int>(std::ceil(idx[1]));
    indexHigh[2] = static_cast<int>(std::ceil(idx[2]));

    float fx = idx[0] - static_cast<float>(indexLow[0]);
    float fy = idx[1] - static_cast<float>(indexLow[1]);
    float fz = idx[2] - static_cast<float>(indexLow[2]);

    // Compute index into LUT for surrounding corners
    const int n000 =
        GetLut3DIndexRedFastRGB(indexLow[0], indexLow[1], indexLow[2], lut3d.lutEdgeSize);
    const int n100 =
        GetLut3DIndexRedFastRGB(indexHigh[0], indexLow[1], indexLow[2], lut3d.lutEdgeSize);
    const int n010 =
        GetLut3DIndexRedFastRGB(indexLow[0], indexHigh[1], indexLow[2], lut3d.lutEdgeSize);
    const int n001 =
        GetLut3DIndexRedFastRGB(indexLow[0], indexLow[1], indexHigh[2], lut3d.lutEdgeSize);
    const int n110 =
        GetLut3DIndexRedFastRGB(indexHigh[0], indexHigh[1], indexLow[2], lut3d.lutEdgeSize);
    const int n101 =
        GetLut3DIndexRedFastRGB(indexHigh[0], indexLow[1], indexHigh[2], lut3d.lutEdgeSize);
    const int n011 =
        GetLut3DIndexRedFastRGB(indexLow[0], indexHigh[1], indexHigh[2], lut3d.lutEdgeSize);
    const int n111 =
        GetLut3DIndexRedFastRGB(indexHigh[0], indexHigh[1], indexHigh[2], lut3d.lutEdgeSize);

    glm::vec3 out;
    if (fx > fy) {
        if (fy > fz) {
            out =
                (1 - fx)  * lut3d.data[n000] +
                (fx - fy) * lut3d.data[n100] +
                (fy - fz) * lut3d.data[n110] +
                (fz)      * lut3d.data[n111];
        }
        else if (fx > fz)
        {
            out =
                (1 - fx)  * lut3d.data[n000] +
                (fx - fz) * lut3d.data[n100] +
                (fz - fy) * lut3d.data[n101] +
                (fy)      * lut3d.data[n111];
        }
        else
        {
            out =
                (1 - fz)  * lut3d.data[n000] +
                (fz - fx) * lut3d.data[n001] +
                (fx - fy) * lut3d.data[n101] +
                (fy)      * lut3d.data[n111];
        }
    }
    else
    {
        if (fz > fy)
        {
            out =
                (1 - fz)  * lut3d.data[n000] +
                (fz - fy) * lut3d.data[n001] +
                (fy - fx) * lut3d.data[n011] +
                (fx)      * lut3d.data[n111];
        }
        else if (fz > fx)
        {
            out =
                (1 - fy)  * lut3d.data[n000] +
                (fy - fz) * lut3d.data[n010] +
                (fz - fx) * lut3d.data[n011] +
                (fx)      * lut3d.data[n111];
        }
        else
        {
            out =
                (1 - fy)  * lut3d.data[n000] +
                (fy - fx) * lut3d.data[n010] +
                (fx - fz) * lut3d.data[n110] +
                (fz)      * lut3d.data[n111];
        }
    }

    return out;
}

glm::vec3 hsv_to_rgb( const glm::vec3 & hsv )
{
    if ( fabsf( hsv.y ) < std::numeric_limits<float>::min() )
    {
       return glm::vec3( hsv.z ) ;
    }

    float flHue = positive_mod( hsv.x, 1.f );
    flHue *= 6.f;

    int i = flHue;        // integer part
	float f = flHue - i;    // fractional part

	float p = hsv.z * ( 1.f - hsv.y );
	float q = hsv.z * ( 1.f - hsv.y * f );
	float t = hsv.z * ( 1.f - hsv.y * ( 1.f - f ) );

	switch(i)
	{
	case 0: return glm::vec3( hsv.z, t, p ); break;
	case 1: return glm::vec3( q, hsv.z, p ); break;
	case 2: return glm::vec3( p, hsv.z, t ); break;
	case 3: return glm::vec3( p, q, hsv.z ); break;
	case 4: return glm::vec3( t, p, hsv.z ); break;
	case 5: return glm::vec3( hsv.z, p, q ); break;
	}

    return glm::vec3( 0 );
}


glm::vec3 rgb_to_hsv( const glm::vec3 & rgb )
{
    float flMax = std::max( std::max( rgb.x, rgb.y ), rgb.z );
    float flMin = std::min( std::min( rgb.x, rgb.y ), rgb.z );
    float flDelta = flMax - flMin;

    glm::vec3 hsv;
    hsv.y = ( fabsf( flMax ) < std::numeric_limits<float>::min() ) ? 0.f : flDelta / flMax;
    hsv.z = flMax;

    if (hsv.y == 0.f)
    {
        hsv.x = -1.0f;
    }
    else
    {
        if ( rgb.x == flMax )
        {
            hsv.x = (rgb.y - rgb.z) / flDelta;
        }
        else if ( rgb.y == flMax )
        {
            hsv.x = 2.f + ( rgb.z - rgb.x ) / flDelta;
        }
        else
        {
            hsv.x = 4.f + ( rgb.x - rgb.y ) / flDelta;
        }

        hsv.x /= 6.f;

        if ( hsv.x < 0.f ) 
        {
            hsv.x += 1.f;
        }
    }

    return hsv;
}

bool BOutOfGamut( const glm::vec3 & color )
{
    return ( color.x<0.f || color.x > 1.f || color.y<0.f || color.y > 1.f || color.z<0.f || color.z > 1.f );
}

glm::vec3 calcEOTFToLinear( const glm::vec3 & input, EOTF eotf, const tonemapping_t & tonemapping )
{
    if ( eotf == EOTF_Gamma22 )
    {
        return glm::pow( input, glm::vec3( 2.2f ) ) * tonemapping.g22_luminance;
    }
    else if ( eotf == EOTF_PQ )
    {
        return pq_to_nits( input );
    }

    return glm::vec3(0);
}

glm::vec3 calcLinearToEOTF( const glm::vec3 & input, EOTF eotf, const tonemapping_t & tonemapping )
{
    if ( eotf == EOTF_Gamma22 )
    {
        glm::vec3 val = input;
        if ( tonemapping.g22_luminance > 0.f )
        {
            val = glm::clamp( input / tonemapping.g22_luminance, glm::vec3( 0.f ), glm::vec3( 1.f ) );
        }
        return glm::pow( val, glm::vec3( 1.f/2.2f ) );
    }
    else if ( eotf == EOTF_PQ )
    {
        return glm::vec3( nits_to_pq(input) );
    }

    return glm::vec3(0);
}

// input is from 0->1
// TODO: use tone-mapping for white, black, contrast ratio
glm::vec3 applyShaper( const glm::vec3 & input, EOTF source, EOTF dest, const tonemapping_t & tonemapping, float flGain, bool bInvert )
{
    if ( source == dest || !tonemapping.bUseShaper )
    {
        return input;
    }

    if ( bInvert )
    {
        // (once we do tone mapping this gets more complicated
        std::swap( source, dest );
        flGain = 1.f / flGain;
    }

    return calcLinearToEOTF( flGain * calcEOTFToLinear( input, source, tonemapping ), dest, tonemapping );
}

void calcColorTransform( uint16_t * pRgbxData1d, int nLutSize1d,
    uint16_t * pRgbxData3d, int nLutEdgeSize3d,
	const displaycolorimetry_t & source, EOTF sourceEOTF,
	const displaycolorimetry_t & dest,  EOTF destEOTF,
    const colormapping_t & mapping, const nightmode_t & nightmode, const tonemapping_t & tonemapping,
    const lut3d_t * pLook, float flGain )
{
    glm::mat3 xyz_from_dest = normalised_primary_matrix( dest.primaries, dest.white, 1.f );
    glm::mat3 dest_from_xyz = glm::inverse( xyz_from_dest );
    glm::mat3 xyz_from_source = normalised_primary_matrix( source.primaries, source.white, 1.f );
    glm::mat3 dest_from_source = dest_from_xyz * xyz_from_source; // Absolute colorimetric mapping

    // Generate shaper lut
    // Note: while this is typically a 1D approximation of our end to end transform,
    // it need not be! Conceptually this is just to determine the interpolation properties...
    // The 3d lut should be considered a 'matched' pair where the transform is only complete
    // when applying both.  I.e., you can put ANY transform in here, and it should work.

    if ( pRgbxData1d )
    {
        float flScale = 1.f / ( (float) nLutSize1d - 1.f );

        for ( int nVal=0; nVal<nLutSize1d; ++nVal )
        {
            glm::vec3 sourceColorEOTFEncoded = { nVal * flScale, nVal * flScale, nVal * flScale };
            glm::vec3 shapedSourceColor = applyShaper( sourceColorEOTFEncoded, sourceEOTF, destEOTF, tonemapping, flGain, false );
            pRgbxData1d[nVal * 4 + 0] = drm_quantize_lut_value( shapedSourceColor.x );
            pRgbxData1d[nVal * 4 + 1] = drm_quantize_lut_value( shapedSourceColor.y );
            pRgbxData1d[nVal * 4 + 2] = drm_quantize_lut_value( shapedSourceColor.z );
            pRgbxData1d[nVal * 4 + 3] = 0;
        }
    }

    if ( pRgbxData3d )
    {
        float flScale = 1.f / ( (float) nLutEdgeSize3d - 1.f );

        // Precalc night mode scalars
        // amount and saturation are overdetermined but we separate the two as they conceptually represent
        // different quantities, and this preserves forwards algorithmic compatibility
        glm::vec3 nightModeMultHSV( nightmode.hue, clamp01( nightmode.saturation * nightmode.amount ), 1.f );
        glm::vec3 vNightModeMultLinear = glm::pow( hsv_to_rgb( nightModeMultHSV ), glm::vec3( 2.2f ) );

        for ( int nBlue=0; nBlue<nLutEdgeSize3d; ++nBlue )
        {
            for ( int nGreen=0; nGreen<nLutEdgeSize3d; ++nGreen )
            {
                for ( int nRed=0; nRed<nLutEdgeSize3d; ++nRed )
                {
                    glm::vec3 shapedSourceColor = { nRed * flScale, nGreen * flScale, nBlue * flScale };
                    glm::vec3 sourceColorEOTFEncoded = applyShaper( shapedSourceColor, sourceEOTF, destEOTF, tonemapping, flGain, true );

                    if ( pLook && !pLook->data.empty() )
                    {
                        sourceColorEOTFEncoded = ApplyLut3D_Tetrahedral( *pLook, sourceColorEOTFEncoded );
                    }

                    // Convert to linearized display referred for source colorimetry
                    glm::vec3 sourceColorLinear = calcEOTFToLinear( sourceColorEOTFEncoded, sourceEOTF, tonemapping );

                    // Convert to dest colorimetry (linearized display referred)
                    glm::vec3 destColorLinear = dest_from_source * sourceColorLinear;

                    // Do a naive blending with native gamut based on saturation
                    // ( A very simplified form of gamut mapping )
                    // float colorSaturation = rgb_to_hsv( sourceColor ).y;
                    float colorSaturation = rgb_to_hsv( sourceColorLinear ).y;
                    float amount = cfit( colorSaturation, mapping.blendEnableMinSat, mapping.blendEnableMaxSat, mapping.blendAmountMin, mapping.blendAmountMax );
                    destColorLinear = glm::mix( destColorLinear, sourceColorLinear, amount );

                    // Apply night mode
                    destColorLinear = vNightModeMultLinear * destColorLinear * glm::vec3( flGain );

                    // Apply dest EOTF
                    glm::vec3 destColorEOTFEncoded = calcLinearToEOTF( destColorLinear, destEOTF, tonemapping );

                    // Write LUT
                    int nLutIndex = nBlue * nLutEdgeSize3d * nLutEdgeSize3d + nGreen * nLutEdgeSize3d + nRed;
                    pRgbxData3d[nLutIndex * 4 + 0] = drm_quantize_lut_value( destColorEOTFEncoded.x );
                    pRgbxData3d[nLutIndex * 4 + 1] = drm_quantize_lut_value( destColorEOTFEncoded.y );
                    pRgbxData3d[nLutIndex * 4 + 2] = drm_quantize_lut_value( destColorEOTFEncoded.z );
                    pRgbxData3d[nLutIndex * 4 + 3] = 0;
                }
            }
        }
    }
}

bool BIsWideGamut( const displaycolorimetry_t & nativeDisplayOutput )
{
    // Use red as a sentinal for a wide-gamut display
    return ( nativeDisplayOutput.primaries.r.x > 0.650f && nativeDisplayOutput.primaries.r.y < 0.320f );
}

void buildSDRColorimetry( displaycolorimetry_t * pColorimetry, colormapping_t *pMapping,
	float flSDRGamutWideness, const displaycolorimetry_t & nativeDisplayOutput  )
{
    if ( BIsWideGamut( nativeDisplayOutput) )
    {
        // 0.0: 709
        // 1.0: Native
        colormapping_t noRemap;
        noRemap.blendEnableMinSat = 0.7f;
        noRemap.blendEnableMaxSat = 1.0f;
        noRemap.blendAmountMin = 0.0f;
        noRemap.blendAmountMax = 0.0f;
        *pMapping = noRemap;
        *pColorimetry = lerp( displaycolorimetry_709, nativeDisplayOutput, flSDRGamutWideness );
    }
    else
    {
        // 0.0: Native
        // 0.5: Generic wide gamut display w/smooth mapping
        // 1.0: Generic wide gamut display w/harsh mapping

        // This is a full blending to the unit cube, starting at 70% max sat
        // Creates a smooth transition from in-gamut to out of gamut
        colormapping_t smoothRemap;
        smoothRemap.blendEnableMinSat = 0.7f;
        smoothRemap.blendEnableMaxSat = 1.0f;
        smoothRemap.blendAmountMin = 0.0f;
        smoothRemap.blendAmountMax = 1.f;

        // Assume linear saturation computation
        // This is a partial (25%) blending to the unit cube
        // Allows some (but not full) clipping
        colormapping_t partialRemap;
        partialRemap.blendEnableMinSat = 0.7f;
        partialRemap.blendEnableMaxSat = 1.0f;
        partialRemap.blendAmountMin = 0.0f;
        partialRemap.blendAmountMax = 0.25;

        if ( flSDRGamutWideness < 0.5f )
        {
            float t = cfit( flSDRGamutWideness, 0.f, 0.5f, 0.0f, 1.0f );
            *pColorimetry = lerp( nativeDisplayOutput, displaycolorimetry_widegamutgeneric, t );
            *pMapping = smoothRemap;
        }
        else
        {
            float t = cfit( flSDRGamutWideness, 0.5f, 1.0f, 0.0f, 1.0f );
            *pColorimetry = displaycolorimetry_widegamutgeneric;
            *pMapping = lerp( smoothRemap, partialRemap, t );
        }
    }
}

void buildPQColorimetry( displaycolorimetry_t * pColorimetry, colormapping_t *pMapping, const displaycolorimetry_t & nativeDisplayOutput )
{
    *pColorimetry = displaycolorimetry_2020;

    if ( BIsWideGamut( nativeDisplayOutput) )
    {
        colormapping_t smoothRemap;
        smoothRemap.blendEnableMinSat = 0.7f;
        smoothRemap.blendEnableMaxSat = 1.0f;
        smoothRemap.blendAmountMin = 0.0f;
        smoothRemap.blendAmountMax = 1.f;
        *pMapping = smoothRemap;
    }
    else
    {
        colormapping_t noRemap;
        noRemap.blendEnableMinSat = 0.7f;
        noRemap.blendEnableMaxSat = 1.0f;
        noRemap.blendAmountMin = 0.0f;
        noRemap.blendAmountMax = 0.0f;
        *pMapping = noRemap;
    }
}

bool approxEqual( const glm::vec3 & a, const glm::vec3 & b, float flTolerance = 1e-5f )
{
    glm::vec3 v = glm::abs(a - b);
    return ( v.x < flTolerance && v.y < flTolerance && v.z < flTolerance );
}

int writeRawLut( const char * outputFilename, uint16_t * pData, size_t nSize )
{
    FILE *file = fopen( outputFilename, "wb" );
    if ( !file )
    {
        return 1;
    }
    for ( size_t i=0; i<nSize; ++i )
    {
        fwrite( pData + i, sizeof( uint16_t ), 1, file );
    }
    fclose( file );
    return 0;
}

int color_tests()
{
#if 0
    {
        // Test normalized primary matrix
        primaries_t primaries = { { 0.602f, 0.355f }, { 0.340f, 0.574f }, { 0.164f, 0.121f } };
        glm::vec2 white = { 0.3070f, 0.3220f };

        glm::mat3x3 rgb_to_xyz = normalised_primary_matrix( primaries, white, 1.f );
        printf("normalised_primary_matrix rgb_to_xyz %s\n", glm::to_string(rgb_to_xyz).c_str() );

        glm::vec3 redXYZ = rgb_to_xyz * glm::vec3(1,0,0);
        glm::vec2 redxy = XYZ_to_xy( redXYZ );
        glm::vec3 whiteXYZ = rgb_to_xyz * glm::vec3(1);
        glm::vec2 whitexy = XYZ_to_xy( whiteXYZ );

        printf("r xy %s == %s \n", glm::to_string(primaries.r).c_str(), glm::to_string(redxy).c_str() );
        printf("w xy %s == %s \n", glm::to_string(white).c_str(), glm::to_string(whitexy).c_str() );
    }
#endif


#if 0
    {
        // chromatic adapatation
        glm::vec3 d50XYZ = glm::vec3(0.96422f, 1.00000f, 0.82521f );
        glm::vec3 d65XYZ = glm::vec3(0.95047f, 1.00000f, 1.08883f );
        printf("d50XYZ %s\n", glm::to_string(d50XYZ).c_str() );
        printf("d65XYZ %s\n", glm::to_string(d65XYZ).c_str() );
        {
            
            glm::mat3x3 d65From50 = chromatic_adaptation_matrix( d50XYZ, d65XYZ, k_EChromaticAdapatationMethod_Bradford );
            printf("bradford d65From50 %s\n", glm::to_string(d65From50).c_str() );
            glm::vec3 d65_2 = d65From50 * d50XYZ;
            printf("bradford d65_2 %s\n", glm::to_string(d65_2).c_str() );
        }
        {
            
            glm::mat3x3 d65From50 = chromatic_adaptation_matrix( d50XYZ, d65XYZ, k_EChromaticAdapatationMethod_XYZ );
            printf("xyzscaling d65From50 %s\n", glm::to_string(d65From50).c_str() );
            glm::vec3 d65_2 = d65From50 * d50XYZ;
            printf("xyzscaling d65_2 %s\n", glm::to_string(d65_2).c_str() );
        }
    }
#endif

#if 0
    {

        int nLut3DSize = 4;
        float flScale = 1.f / ( (float) nLut3DSize - 1.f );
        for (int nBlue = 0; nBlue<nLut3DSize; ++nBlue )
        {
            for (int nGreen = 0; nGreen<nLut3DSize; ++nGreen )
            {
                for (int nRed = 0; nRed<nLut3DSize; ++nRed )
                {
                    glm::vec3 rgb1( nRed * flScale, nGreen * flScale, nBlue * flScale );
                    glm::vec3 hsv = rgb_to_hsv( rgb1 );
                    glm::vec3 rgb2 = hsv_to_rgb( hsv );
                    if ( !approxEqual(rgb1, rgb2 ) )
                    {
                        printf("****");
                    }
                    printf("%s %s %s\n", glm::to_string(rgb1).c_str(), glm::to_string(hsv).c_str(), glm::to_string(rgb2).c_str()  );
                }
            }
        }
    }
#endif

   return 0;
}
