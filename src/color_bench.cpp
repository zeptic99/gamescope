#include <benchmark/benchmark.h>

#include "color_helpers.h"

const uint32_t nLutSize1d = 4096;
const uint32_t nLutEdgeSize3d = 17;

uint16_t lut1d[nLutSize1d*4];
uint16_t lut3d[nLutEdgeSize3d*nLutEdgeSize3d*nLutEdgeSize3d*4];

static void BenchmarkCalcColorTransform(EOTF inputEOTF, benchmark::State &state)
{
    const primaries_t primaries = { { 0.602f, 0.355f }, { 0.340f, 0.574f }, { 0.164f, 0.121f } };
    const glm::vec2 white = { 0.3070f, 0.3220f };

    displaycolorimetry_t inputColorimetry{};
    inputColorimetry.primaries = primaries;
    inputColorimetry.white = white;

    displaycolorimetry_t outputEncodingColorimetry{};
    outputEncodingColorimetry.primaries = primaries;
    outputEncodingColorimetry.white = white;

    colormapping_t colorMapping{};

    tonemapping_t tonemapping{};
    tonemapping.bUseShaper = true;

    nightmode_t nightmode{};
    float flGain = 1.0f;

    for (auto _ : state) {
        calcColorTransform( &lut1d[0], nLutSize1d, &lut3d[0], nLutEdgeSize3d, inputColorimetry, inputEOTF,
            outputEncodingColorimetry, EOTF_Gamma22,
            colorMapping, nightmode, tonemapping, nullptr, flGain );
    }
}

static void BenchmarkCalcColorTransforms_G22(benchmark::State &state)
{
    BenchmarkCalcColorTransform(EOTF_Gamma22, state);
}
BENCHMARK(BenchmarkCalcColorTransforms_G22);

static void BenchmarkCalcColorTransforms_PQ(benchmark::State &state)
{
    BenchmarkCalcColorTransform(EOTF_PQ, state);
}
BENCHMARK(BenchmarkCalcColorTransforms_PQ);

static void BenchmarkCalcColorTransforms(benchmark::State &state)
{
    for ( uint32_t nInputEOTF = 0; nInputEOTF < EOTF_Count; nInputEOTF++ )
        BenchmarkCalcColorTransform((EOTF)nInputEOTF, state);
}
BENCHMARK(BenchmarkCalcColorTransforms);

BENCHMARK_MAIN();
