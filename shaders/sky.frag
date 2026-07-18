#version 460 core

in vec2 vNDC;
out vec4 FragColor;

uniform mat4 uInvViewProj;
uniform vec3 uCameraPos;   // reserved for a future ground-fog/aerial-perspective pass; unused for now
uniform vec3 uSunDirection; // normalized, points FROM the scene TOWARD the sun

// ---------------------------------------------------------------------------
// Physically-based single-scattering atmosphere (Rayleigh + Mie), integrated
// analytically per pixel — no precomputed lookup textures needed. Based on
// the well-known real-time atmospheric scattering technique originated by
// Sean O'Neil (GPU Gems 2) and widely used in its simplified GLSL form
// (e.g. the public-domain reference implementation by wwwtyro/Dimas
// Leenman). This traces actual optical depth along the view ray and, at
// each sample, a second ray toward the sun — which is why sunset colors,
// horizon haze, and sky brightness at different sun angles all emerge from
// the physics instead of being hand-picked colors like the previous
// gradient version.
// ---------------------------------------------------------------------------

const float PI = 3.141592653589793;
const int   NUM_VIEW_SAMPLES  = 8; // steps along the primary (view) ray
const int   NUM_LIGHT_SAMPLES = 4; // steps along the secondary (sun) ray per view sample

const float PLANET_RADIUS         = 6371000.0; // meters, Earth-scale
const float ATMOSPHERE_RADIUS     = 6471000.0; // 100km of atmosphere above the surface
const vec3  RAYLEIGH_COEFF        = vec3(5.5e-6, 13.0e-6, 22.4e-6); // wavelength-dependent — this is *why* the sky is blue
const float MIE_COEFF             = 21e-6;
const float RAYLEIGH_SCALE_HEIGHT = 8000.0; // meters — how fast Rayleigh density falls off with altitude
const float MIE_SCALE_HEIGHT      = 1200.0; // meters — Mie (haze/dust) falls off much faster than Rayleigh
const float MIE_G                 = 0.758;  // forward-scattering anisotropy — shapes the sun's halo
const float SUN_INTENSITY         = 22.0;   // tune this to brighten/dim the whole sky

// Ray-sphere intersection; returns false if the ray misses the sphere entirely.
bool RaySphereIntersect(vec3 rayOrigin, vec3 rayDir, float sphereRadius, out float t0, out float t1) {
    float a = dot(rayDir, rayDir);
    float b = 2.0 * dot(rayDir, rayOrigin);
    float c = dot(rayOrigin, rayOrigin) - sphereRadius * sphereRadius;
    float discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0.0) {
        return false;
    }
    float sqrtDiscriminant = sqrt(discriminant);
    t0 = (-b - sqrtDiscriminant) / (2.0 * a);
    t1 = (-b + sqrtDiscriminant) / (2.0 * a);
    return true;
}

vec3 ComputeAtmosphere(vec3 rayOrigin, vec3 rayDir, vec3 sunDir) {
    float t0, t1;
    if (!RaySphereIntersect(rayOrigin, rayDir, ATMOSPHERE_RADIUS, t0, t1) || t1 < 0.0) {
        return vec3(0.0);
    }
    t0 = max(t0, 0.0);

    // Intentionally NOT clipping the primary ray at the virtual planet's
    // surface here (an earlier version did, and it caused a harsh black
    // band right at the horizon: the virtual camera sits only ~1m above
    // the virtual ground, so any real-camera downward tilt immediately hit
    // that virtual ground and collapsed the sampled segment to ~0 length,
    // i.e. ~0 light). The engine already renders real ground geometry over
    // this sky pass afterward (depth-tested), so the sky itself doesn't
    // need to self-occlude — letting the ray keep integrating through the
    // full atmosphere gives a smooth haze that blends naturally with
    // whatever real ground ends up drawn on top of it.

    float segmentLength = (t1 - t0) / float(NUM_VIEW_SAMPLES);
    float tCurrent = t0;

    vec3 totalRayleigh = vec3(0.0);
    vec3 totalMie = vec3(0.0);
    float opticalDepthRayleigh = 0.0;
    float opticalDepthMie = 0.0;

    float mu = dot(rayDir, sunDir);
    float phaseRayleigh = 3.0 / (16.0 * PI) * (1.0 + mu * mu);
    float g2 = MIE_G * MIE_G;
    float phaseMie = 3.0 / (8.0 * PI) * ((1.0 - g2) * (1.0 + mu * mu)) /
                      ((2.0 + g2) * pow(1.0 + g2 - 2.0 * MIE_G * mu, 1.5));

    for (int i = 0; i < NUM_VIEW_SAMPLES; ++i) {
        vec3 samplePos = rayOrigin + rayDir * (tCurrent + segmentLength * 0.5);
        float sampleHeight = max(length(samplePos) - PLANET_RADIUS, 0.0);

        float stepOpticalDepthRayleigh = exp(-sampleHeight / RAYLEIGH_SCALE_HEIGHT) * segmentLength;
        float stepOpticalDepthMie = exp(-sampleHeight / MIE_SCALE_HEIGHT) * segmentLength;
        opticalDepthRayleigh += stepOpticalDepthRayleigh;
        opticalDepthMie += stepOpticalDepthMie;

        // Secondary ray toward the sun: how much light survives from the
        // top of the atmosphere down to this sample point.
        float lt0, lt1;
        RaySphereIntersect(samplePos, sunDir, ATMOSPHERE_RADIUS, lt0, lt1);
        float lightSegmentLength = lt1 / float(NUM_LIGHT_SAMPLES);
        float lightT = 0.0;
        float lightOpticalDepthRayleigh = 0.0;
        float lightOpticalDepthMie = 0.0;
        bool hitsGround = false;

        for (int j = 0; j < NUM_LIGHT_SAMPLES; ++j) {
            vec3 lightSamplePos = samplePos + sunDir * (lightT + lightSegmentLength * 0.5);
            float lightSampleHeight = length(lightSamplePos) - PLANET_RADIUS;
            if (lightSampleHeight < 0.0) {
                hitsGround = true;
                break;
            }
            lightOpticalDepthRayleigh += exp(-lightSampleHeight / RAYLEIGH_SCALE_HEIGHT) * lightSegmentLength;
            lightOpticalDepthMie += exp(-lightSampleHeight / MIE_SCALE_HEIGHT) * lightSegmentLength;
            lightT += lightSegmentLength;
        }

        if (!hitsGround) {
            vec3 tau = RAYLEIGH_COEFF * (opticalDepthRayleigh + lightOpticalDepthRayleigh) +
                       MIE_COEFF * 1.1 * (opticalDepthMie + lightOpticalDepthMie);
            vec3 attenuation = exp(-tau);
            totalRayleigh += attenuation * stepOpticalDepthRayleigh;
            totalMie += attenuation * stepOpticalDepthMie;
        }

        tCurrent += segmentLength;
    }

    return SUN_INTENSITY * (totalRayleigh * RAYLEIGH_COEFF * phaseRayleigh +
                             totalMie * MIE_COEFF * phaseMie);
}

void main() {
    vec4 nearPoint = uInvViewProj * vec4(vNDC, -1.0, 1.0);
    vec4 farPoint  = uInvViewProj * vec4(vNDC,  1.0, 1.0);
    nearPoint /= nearPoint.w;
    farPoint  /= farPoint.w;
    vec3 rayDir = normalize(farPoint.xyz - nearPoint.xyz);

    // The scattering model works in planet-scale coordinates (meters, world
    // origin at the planet's surface) — completely decoupled from this
    // engine's small local world-space units. The virtual camera always
    // sits at a fixed small altitude above a virtual planet, regardless of
    // where the real camera is in your scene; a few meters of real-world
    // camera movement is negligible at atmosphere scale anyway.
    vec3 scatterCameraPos = vec3(0.0, PLANET_RADIUS + 1.0, 0.0);

    vec3 color = ComputeAtmosphere(scatterCameraPos, rayDir, uSunDirection);

    // Scattering output is HDR (can exceed 1.0 right around the sun), so it
    // needs compressing before display — simple Reinhard tonemap + gamma.
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
