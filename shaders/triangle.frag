#version 460 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

uniform sampler2D uAlbedoMap;
uniform int uHasAlbedoMap;
uniform vec3 uAlbedoTint;
uniform vec3 uViewPos;

// directional light
uniform vec3 uDirLightDirection;
uniform vec3 uDirLightColor;

// point light
uniform vec3 uPointLightPos;
uniform vec3 uPointLightColor;

// Local world units are small (50-unit chunks) compared to the planet-scale
// meters sky.frag's extinction coefficients were tuned for, so raw distance
// here would put aerialTransmittance at ~1.0 everywhere and never show any
// haze. This scales distance up into a range where falloff is actually
// visible within normal draw distances — tune to taste against your
// far-plane/chunk-load-radius settings.
const float kAerialDistanceScale = 400.0;

void main() {
    vec3 norm = normalize(Normal);
    vec3 albedo = uAlbedoTint;

    if (uHasAlbedoMap == 1) {
        albedo *= texture(uAlbedoMap, TexCoord).rgb;
    } else {
        // No texture assigned — render the same pink/gray checkerboard
        // convention used for failed texture loads (see Texture.cpp's
        // GenerateCheckerboardFallback), computed procedurally here from UVs
        // so "no material" and "missing texture" read as visually
        // consistent, Unreal/Source-style, with zero extra texture cost.
        vec2 checkerCell = floor(TexCoord * 8.0);
        float checkerParity = mod(checkerCell.x + checkerCell.y, 2.0);
        vec3 checkerPink = vec3(1.0, 0.0, 0.78);
        vec3 checkerGray = vec3(0.35, 0.35, 0.35);
        albedo *= mix(checkerGray, checkerPink, checkerParity);
    }

    // ambient
    vec3 ambient = 0.15 * albedo;

    // directional light (diffuse)
    vec3 lightDir = normalize(-uDirLightDirection);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 dirDiffuse = diff * uDirLightColor * albedo;

    // point light (diffuse + attenuation)
    vec3 pointDir = normalize(uPointLightPos - FragPos);
    float pointDiff = max(dot(norm, pointDir), 0.0);
    float distance = length(uPointLightPos - FragPos);
    float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
    vec3 pointDiffuse = pointDiff * uPointLightColor * albedo * attenuation;

    vec3 result = ambient + dirDiffuse + pointDiffuse;

    // --- Aerial perspective (cheap single-sample approximation) -------------
    // A fully correct version would re-run the same multi-sample integral
    // sky.frag uses, but for the segment from the camera to this fragment —
    // too expensive per scene fragment at full resolution every frame.
    // Instead: approximate extinction using near-ground air density (valid
    // since scene geometry sits close to y=0, tiny compared to the
    // 8km/1.2km scale heights), and tint the fog color using the sun
    // direction so it warms up toward sunset instead of staying flat gray haze.
    const vec3 kRayleighCoeff = vec3(5.5e-6, 13.0e-6, 22.4e-6);
    const float kMieCoeff = 21e-6 * 1.1;
    vec3 groundExtinction = kRayleighCoeff + vec3(kMieCoeff);

    float distanceToCamera = length(uViewPos - FragPos) * kAerialDistanceScale;
    vec3 aerialTransmittance = exp(-groundExtinction * distanceToCamera);

    vec3 viewDir = normalize(FragPos - uViewPos);
    float sunFacing = clamp(dot(viewDir, -uDirLightDirection), 0.0, 1.0);
    float sunLowInSky = clamp(-uDirLightDirection.y, 0.0, 1.0);
    vec3 hazeColor = mix(vec3(0.45, 0.5, 0.6), vec3(0.9, 0.55, 0.35), pow(sunFacing, 4.0) * sunLowInSky);
    // NOTE: hazeColor here is a hand-tuned stand-in for what sky.frag would
    // compute in that direction. For real consistency (haze color that
    // exactly matches the actual sky), sample the same atmosphere function
    // or a baked sky LUT instead — that's the "further out" precomputed-LUT
    // territory mentioned separately.

    result = mix(hazeColor, result, aerialTransmittance);

    FragColor = vec4(result, 1.0);
}