#version 460 core

in vec2 vNDC;
out vec4 FragColor;

uniform mat4 uInvViewProj;
uniform vec3 uCameraPos;
uniform vec3 uSunDirection;

const float PI = 3.141592653589793;
const int   NUM_VIEW_SAMPLES  = 8;
const int   NUM_LIGHT_SAMPLES = 4;

const float PLANET_RADIUS         = 6371000.0;
const float ATMOSPHERE_RADIUS     = 6471000.0;
const vec3  RAYLEIGH_COEFF        = vec3(5.5e-6, 13.0e-6, 22.4e-6);
const float MIE_COEFF             = 21e-6;
const float RAYLEIGH_SCALE_HEIGHT = 8000.0;
const float MIE_SCALE_HEIGHT      = 1200.0;
const float MIE_G                 = 0.758;
const float SUN_INTENSITY         = 16.0; // was 22.0 — overall sky brightness pulled back

const vec3 OZONE_COEFF = vec3(0.650e-6, 1.881e-6, 0.085e-6);

float OzoneDensity(float height) {
    return max(0.0, 1.0 - abs(height - 25000.0) / 15000.0);
}

const vec3 GROUND_ALBEDO = vec3(0.3, 0.3, 0.28);
const float MULTI_SCATTER_FACTOR = 0.5;

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

vec3 ComputeAtmosphere(vec3 rayOrigin, vec3 rayDir, vec3 sunDir, out vec3 outTransmittance) {
    float t0, t1;
    if (!RaySphereIntersect(rayOrigin, rayDir, ATMOSPHERE_RADIUS, t0, t1) || t1 < 0.0) {
        outTransmittance = vec3(0.0);
        return vec3(0.0);
    }
    t0 = max(t0, 0.0);

    float segmentLength = (t1 - t0) / float(NUM_VIEW_SAMPLES);
    float tCurrent = t0;

    vec3 totalRayleigh = vec3(0.0);
    vec3 totalMie = vec3(0.0);
    float opticalDepthRayleigh = 0.0;
    float opticalDepthMie = 0.0;
    float opticalDepthOzone = 0.0;

    float mu = dot(rayDir, sunDir);
    float phaseRayleigh = 3.0 / (16.0 * PI) * (1.0 + mu * mu);
    float g2 = MIE_G * MIE_G;
    float phaseMie = 3.0 / (8.0 * PI) * ((1.0 - g2) * (1.0 + mu * mu)) /
                      ((2.0 + g2) * pow(1.0 + g2 - 2.0 * MIE_G * mu, 1.5));
    const float phaseIsotropic = 1.0 / (4.0 * PI);

    for (int i = 0; i < NUM_VIEW_SAMPLES; ++i) {
        vec3 samplePos = rayOrigin + rayDir * (tCurrent + segmentLength * 0.5);
        float sampleHeight = max(length(samplePos) - PLANET_RADIUS, 0.0);

        float stepOpticalDepthRayleigh = exp(-sampleHeight / RAYLEIGH_SCALE_HEIGHT) * segmentLength;
        float stepOpticalDepthMie = exp(-sampleHeight / MIE_SCALE_HEIGHT) * segmentLength;
        float stepOpticalDepthOzone = OzoneDensity(sampleHeight) * segmentLength;
        opticalDepthRayleigh += stepOpticalDepthRayleigh;
        opticalDepthMie += stepOpticalDepthMie;
        opticalDepthOzone += stepOpticalDepthOzone;

        float lt0, lt1;
        RaySphereIntersect(samplePos, sunDir, ATMOSPHERE_RADIUS, lt0, lt1);
        float lightSegmentLength = lt1 / float(NUM_LIGHT_SAMPLES);
        float lightT = 0.0;
        float lightOpticalDepthRayleigh = 0.0;
        float lightOpticalDepthMie = 0.0;
        float lightOpticalDepthOzone = 0.0;
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
            lightOpticalDepthOzone += OzoneDensity(max(lightSampleHeight, 0.0)) * lightSegmentLength;
            lightT += lightSegmentLength;
        }

        if (!hitsGround) {
            vec3 tau = RAYLEIGH_COEFF * (opticalDepthRayleigh + lightOpticalDepthRayleigh) +
                       MIE_COEFF * 1.1 * (opticalDepthMie + lightOpticalDepthMie) +
                       OZONE_COEFF * (opticalDepthOzone + lightOpticalDepthOzone);
            vec3 attenuation = exp(-tau);
            totalRayleigh += attenuation * stepOpticalDepthRayleigh;
            totalMie += attenuation * stepOpticalDepthMie;
        }

        tCurrent += segmentLength;
    }

    vec3 primaryTau = RAYLEIGH_COEFF * opticalDepthRayleigh +
                       MIE_COEFF * 1.1 * opticalDepthMie +
                       OZONE_COEFF * opticalDepthOzone;
    outTransmittance = exp(-primaryTau);

    vec3 singleScatter = totalRayleigh * RAYLEIGH_COEFF * phaseRayleigh +
                          totalMie * MIE_COEFF * phaseMie;

    vec3 multiScatter = (totalRayleigh * RAYLEIGH_COEFF + totalMie * MIE_COEFF) *
                         phaseIsotropic * MULTI_SCATTER_FACTOR;

    float sunElevation = clamp(sunDir.y, -1.0, 1.0);
    float groundLightFraction = clamp(sunElevation * 3.0, 0.0, 1.0);
    vec3 groundBounceAmbient = GROUND_ALBEDO * groundLightFraction * 0.02;

    return SUN_INTENSITY * (singleScatter + multiScatter) + SUN_INTENSITY * groundBounceAmbient;
}

void main() {
    vec4 nearPoint = uInvViewProj * vec4(vNDC, -1.0, 1.0);
    vec4 farPoint  = uInvViewProj * vec4(vNDC,  1.0, 1.0);
    nearPoint /= nearPoint.w;
    farPoint  /= farPoint.w;
    vec3 rayDir = normalize(farPoint.xyz - nearPoint.xyz);

    vec3 scatterCameraPos = vec3(0.0, PLANET_RADIUS + 1.0, 0.0);

    vec3 transmittance;
    vec3 color = ComputeAtmosphere(scatterCameraPos, rayDir, uSunDirection, transmittance);

    // --- Sun disk + corona ------------------------------------------------
    const float SUN_ANGULAR_RADIUS = radians(1.6); // stylized, ~6x real size for legibility at gameplay FOV
    const float SUN_DISK_INTENSITY = 120.0;

    float cosSunAngle = dot(rayDir, uSunDirection);
    float cosSunEdge = cos(SUN_ANGULAR_RADIUS);

    float edgeAA = fwidth(cosSunAngle);
    float sunMask = smoothstep(cosSunEdge - edgeAA, cosSunEdge + edgeAA, cosSunAngle);

    // Fade the disk (and corona) out smoothly as the sun crosses the
    // horizon — computed once, reused by both below.
    float sunHorizonFade = smoothstep(-0.02, 0.02, uSunDirection.y);

    // Corona: a soft falloff around the disk, giving a glowing halo instead
    // of a flat white circle. This was the actual source of the "foggy"
    // look reported — at the old 8.0 magnitude / 14.0 falloff exponent, it
    // stayed non-trivially bright out to ~15° from the sun, unscaled by any
    // of the physical extinction coefficients above, which is wide enough
    // to blanket a large swath of the horizon in bloom regardless of
    // downstream exposure/threshold tuning. Tightened and dimmed here at
    // the source instead of just compensating for it later.
    float angleFromSun = acos(clamp(cosSunAngle, -1.0, 1.0));
    float coronaFalloff = exp(-angleFromSun * 22.0); // was 14.0 — narrower halo
    vec3 coronaColor = vec3(1.0, 0.9, 0.75) * coronaFalloff * 3.0 * sunHorizonFade * transmittance; // was 8.0

    vec3 sunColor = vec3(1.0, 0.96, 0.9) * SUN_DISK_INTENSITY * sunMask * sunHorizonFade * transmittance;
    color += sunColor;
    color += coronaColor;

    // NOTE: no tonemap/gamma here anymore — this shader now outputs linear
    // HDR color into the scene's floating-point framebuffer. Tonemapping
    // happens exactly once, in bloom_composite.frag, after the bloom pass
    // has had a chance to sample the un-clipped bright values (sun disk,
    // corona, bright cloud edges). Tonemapping before that would crush
    // those values to ~1.0 and bloom would have nothing bright left to grab.
    FragColor = vec4(color, 1.0);
}
