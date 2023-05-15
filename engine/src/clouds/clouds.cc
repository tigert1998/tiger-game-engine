#include "clouds/clouds.h"

#include <glad/glad.h>

#include "light_sources.h"
#include "texture_manager.h"
#include "utils.h"

void Clouds::Allocate(uint32_t width, uint32_t height) {
  width_ = width;
  height_ = height;
  frag_color_texture_id_ = TextureManager::AllocateTexture(
      width, height, GL_RGBA32F, GL_RGBA, GL_FLOAT, false);

  fbo_.reset(new FrameBufferObject(width, height));
}

void Clouds::Deallocate() { glDeleteTextures(1, &frag_color_texture_id_); }

Clouds::Clouds(uint32_t width, uint32_t height) {
  noise_texture_generator_.reset(new NoiseTextureGenerator(0.8));
  shader_.reset(new Shader({{GL_COMPUTE_SHADER, Clouds::kCsSource}}));
  screen_space_shader_ = ScreenSpaceShader(kFsSource);
  glGenVertexArrays(1, &vao_);
  Allocate(width, height);
}

void Clouds::Resize(uint32_t width, uint32_t height) {
  Deallocate();
  Allocate(width, height);
}

void Clouds::Draw(Camera *camera, LightSources *light_sources, double time) {
  shader_->Use();
  light_sources->Set(shader_.get(), true);
  glBindImageTexture(0, frag_color_texture_id_, 0, GL_FALSE, 0, GL_WRITE_ONLY,
                     GL_RGBA32F);
  shader_->SetUniform<int32_t>("uFragColor", 0);
  shader_->SetUniformSampler3D(
      "uPerlinWorleyTexture",
      noise_texture_generator_->perlin_worley_texture_id(), 0);
  shader_->SetUniformSampler3D(
      "uWorleyTexture", noise_texture_generator_->worley_texture_id(), 1);
  shader_->SetUniformSampler2D(
      "uWeatherTexture", noise_texture_generator_->weather_texture_id(), 2);
  shader_->SetUniform<glm::vec2>("uScreenSize", glm::vec2(width_, height_));
  shader_->SetUniform<glm::vec3>("uCameraPosition", camera->position());
  shader_->SetUniform<glm::mat4>(
      "uInvViewProjectionMatrix",
      glm::inverse(camera->projection_matrix() * camera->view_matrix()));
  shader_->SetUniform<float>("uTime", time);
  glDispatchCompute((width_ + 3) / 4, (height_ + 3) / 4, 1);
  glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT |
                  GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindTexture(GL_TEXTURE_3D, 0);

  glBindVertexArray(vao_);
  screen_space_shader_->Use();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, fbo_->color_texture_id());
  screen_space_shader_->SetUniform<int32_t>("uScreenTexture", 0);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, frag_color_texture_id_);
  screen_space_shader_->SetUniform<int32_t>("uCloudTexture", 1);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, fbo_->depth_texture_id());
  screen_space_shader_->SetUniform<int32_t>("uDepthTexture", 2);
  screen_space_shader_->SetUniform<glm::vec2>("uScreenSize",
                                              glm::vec2(width_, height_));
  glDrawArrays(GL_POINTS, 0, 1);
  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

const std::string Clouds::kCsSource = R"(
#version 430

layout (local_size_x = 4, local_size_y = 4, local_size_z = 1) in;
)" + LightSources::FsSource() + R"(

layout (rgba32f) uniform image2D uFragColor;

uniform sampler3D uPerlinWorleyTexture;
uniform sampler3D uWorleyTexture;
uniform sampler2D uWeatherTexture;

uniform vec2 uScreenSize;
uniform vec3 uCameraPosition;
uniform mat4 uInvViewProjectionMatrix;

uniform float uSphereRadius = 600000.0;
uniform float uInnerHeight = 5000.0;
uniform float uOuterHeight = 17000.0;
uniform vec3 uSphereCenter = vec3(0, -600000.0, 0);

uniform vec3 uWindDirection = normalize(vec3(0.5, 0, 0.1));
uniform float uCloudSpeed = 450;
uniform float uCloudTopOffset = 750;
uniform float uCoverageMultiplier = 0.45;
uniform float uCrispiness = 40.;
uniform float uCurliness = 0.1;
uniform float uAbsorption = 0.0035;
uniform float uDensityFactor = 0.02;
uniform vec3 uCloudBottomColor = vec3(0.38235, 0.41176, 0.47058);
uniform bool uEnablePowder = false;

uniform float uTime = 0;

const vec3 NOISE_KERNEL[6u] = vec3[](
	vec3( 0.38051305,  0.92453449, -0.02111345),
	vec3(-0.50625799, -0.03590792, -0.86163418),
	vec3(-0.32509218, -0.94557439,  0.01428793),
	vec3( 0.09026238, -0.27376545,  0.95755165),
	vec3( 0.28128598,  0.42443639, -0.86065785),
	vec3(-0.16852403,  0.14748697,  0.97460106)
);

const float BAYER_FILTER[16u] = float[](
	0.0 / 16.0, 8.0 / 16.0, 2.0 / 16.0, 10.0 / 16.0,
	12.0 / 16.0, 4.0 / 16.0, 14.0 / 16.0, 6.0 / 16.0,
	3.0 / 16.0, 11.0 / 16.0, 1.0 / 16.0, 9.0 / 16.0,
	15.0 / 16.0, 7.0 / 16.0, 13.0 / 16.0, 5.0 / 16.0
);

DirectionalLight GetSunLight() {
    for (int i = 0; i < uDirectionalLights.n; i++) {
        return uDirectionalLights.l[i];
    }
}

float HG(float lDotV, float g) {
	float gg = g * g;
	return (1. - gg) / pow(1. + gg - 2. * g * lDotV, 1.5);
}

float Powder(float d) {
	return 1. - exp(-2 * d);
}

bool RaySphereIntersection(
    vec3 rayOrigin, vec3 rayDirection,
    vec3 sphereCenter, float radius, out vec3 position
) {
    float a = dot(rayDirection, rayDirection);
    float b = 2 * dot(rayDirection, rayOrigin - sphereCenter);
    float c = dot(rayOrigin - sphereCenter, rayOrigin - sphereCenter) - radius * radius;

    float discr = b * b - 4 * a * c;
    if (discr < 0) return false;
    float t0 = (-b - sqrt(discr)) / (2 * a);
    if (t0 > 0) {
        position = rayOrigin + t0 * rayDirection;
        return true;
    }
    float t1 = (-b + sqrt(discr)) / (2 * a);
    if (t1 > 0) {
        position = rayOrigin + t1 * rayDirection;
        return true;
    }
    return false;
}

float GetHeightFractionForPoint(vec3 position) {
    return (length(position - uSphereCenter) - (uSphereRadius + uInnerHeight)) / uOuterHeight;
}

float Remap(float originalValue, float originalMin, float originalMax, float newMin, float newMax) {
    return newMin + (((originalValue - originalMin) / (originalMax - originalMin)) * (newMax - newMin));
}

vec2 GetTexCoord(vec3 position) {
	return position.xz / (uSphereRadius + uInnerHeight) + 0.5;
}

float GetDensityHeightGradientForPoint(float heightFraction, float cloudType) {
    const vec4 STRATUS_GRADIENT = vec4(0.0, 0.1, 0.2, 0.3);
    const vec4 STRATOCUMULUS_GRADIENT = vec4(0.02, 0.2, 0.48, 0.625);
    const vec4 CUMULUS_GRADIENT = vec4(0.00, 0.1625, 0.88, 0.98);

	float stratusFactor = 1.0 - clamp(cloudType * 2.0, 0.0, 1.0);
	float stratoCumulusFactor = 1.0 - abs(cloudType - 0.5) * 2.0;
	float cumulusFactor = clamp(cloudType - 0.5, 0.0, 1.0) * 2.0;

    vec4 baseGradient = stratusFactor * STRATUS_GRADIENT + stratoCumulusFactor * STRATOCUMULUS_GRADIENT + cumulusFactor * CUMULUS_GRADIENT;

    // gradient computation (see Siggraph 2017 Nubis-Decima talk)
	return smoothstep(baseGradient.x, baseGradient.y, heightFraction) - smoothstep(baseGradient.z, baseGradient.w, heightFraction);
}

float SampleCloudDensity(vec3 position, bool cheap, float lod) {
    float heightFraction = GetHeightFractionForPoint(position);
    if (heightFraction <= 0.0 || heightFraction >= 1.0) {
		return 0.0;
	}
    vec3 animation = heightFraction * uWindDirection * uCloudTopOffset + uWindDirection * uTime * uCloudSpeed;
    vec2 texCoord = GetTexCoord(position);
    vec2 movingTexCoord = GetTexCoord(position + animation);

    vec4 lowFrequencyNoises = textureLod(uPerlinWorleyTexture, vec3(texCoord * uCrispiness, heightFraction), lod);
    float lowFrequencyFBM = dot(lowFrequencyNoises.gba, vec3(0.625, 0.25, 0.125));
    float baseCloud = Remap(lowFrequencyNoises.r, -(1.0 - lowFrequencyFBM), 1., 0.0 , 1.0);

    float densityHeightGradient = GetDensityHeightGradientForPoint(heightFraction, 1.0);
    baseCloud *= (densityHeightGradient / heightFraction); // important modification

    vec3 weather = texture(uWeatherTexture, movingTexCoord).rgb;
    float cloudCoverage = weather.r * uCoverageMultiplier;
    float baseCloudWithCoverage = Remap(baseCloud, cloudCoverage, 1.0, 0.0, 1.0);
    baseCloudWithCoverage *= cloudCoverage;

    if (cheap) return baseCloudWithCoverage;

    vec3 highFrequencyNoises = textureLod(uWorleyTexture, vec3(movingTexCoord * uCrispiness, heightFraction) * uCurliness, lod).rgb;
    float highFrequencyFBM = dot(highFrequencyNoises.rgb, vec3(0.625, 0.25, 0.125));
    float highFrequencyNoiseModifier = mix(highFrequencyFBM, 1.0 - highFrequencyFBM, clamp(heightFraction * 10.0, 0, 1));
    baseCloudWithCoverage = baseCloudWithCoverage - highFrequencyNoiseModifier * (1.0 - baseCloudWithCoverage);
	baseCloudWithCoverage = Remap(baseCloudWithCoverage * 2.0, highFrequencyNoiseModifier * 0.2, 1.0, 0.0, 1.0);
    return clamp(baseCloudWithCoverage, 0, 1);
}

float RayMarchToLight(vec3 o, float stepSize) {
	vec3 startPos = o;
	float ds = stepSize * 6.0;
	vec3 rayStep = normalize(-GetSunLight().dir) * ds;
	const float CONE_STEP = 1.0 / 6.0;
	float coneRadius = 1.0; 
	float density = 0.0;
	float coneDensity = 0.0;
	float invDepth = 1.0 / ds;
	float sigma_ds = -ds * uAbsorption;
	vec3 pos;

	float T = 1.0;

	for (int i = 0; i < 6; i++) {
		pos = startPos + coneRadius * NOISE_KERNEL[i] * float(i);

		float heightFraction = GetHeightFractionForPoint(pos);
		if (heightFraction >= 0) {
			float cloudDensity = SampleCloudDensity(pos, false, i / 16);
			if (cloudDensity > 0.0) {
				float Ti = exp(cloudDensity*sigma_ds);
				T *= Ti;
				density += cloudDensity;
			}
		}
		startPos += rayStep;
		coneRadius += CONE_STEP;
	}

	return T;
}

vec4 RayMarchToCloud(vec3 startPos, vec3 endPos, vec3 bg, out vec4 cloudPos) {
	vec3 path = endPos - startPos;
	float len = length(path);

	const int nSteps = 64;

	float ds = len / nSteps;
	vec3 dir = path / len;
	dir *= ds;
	vec4 col = vec4(0.0);
	uvec2 fragCoord = gl_GlobalInvocationID.xy;
	int a = int(fragCoord.x) % 4;
	int b = int(fragCoord.y) % 4;
	startPos += dir * BAYER_FILTER[a * 4 + b];
	vec3 pos = startPos;

	float density = 0.0;

	float lightDotEye = dot(normalize(-GetSunLight().dir), normalize(dir));

	float T = 1.0;
	float sigma_ds = -ds * uDensityFactor;
	bool expensive = true;
	bool entered = false;

	int zero_density_sample = 0;

	for (int i = 0; i < nSteps; ++i) {	
		float density_sample = SampleCloudDensity(pos, false, i / 16);
		if (density_sample > 0.) {
			if (!entered){
				cloudPos = vec4(pos,1.0);
				entered = true;	
			}
			float height = GetHeightFractionForPoint(pos);
			vec3 ambientLight = uCloudBottomColor;
			float light_density = RayMarchToLight(pos, ds * 0.1);
			float scattering = mix(HG(lightDotEye, -0.08), HG(lightDotEye, 0.08), clamp(lightDotEye*0.5 + 0.5, 0.0, 1.0));
			scattering = max(scattering, 1.0);
			float powderTerm = Powder(density_sample);
            if (!uEnablePowder) powderTerm = 1.0;

			vec3 S = 0.6 * (mix(
                mix(ambientLight * 1.8, bg, 0.2), 
                scattering * GetSunLight().color,
                powderTerm * light_density
            )) * density_sample;
			float dTrans = exp(density_sample * sigma_ds);
			vec3 Sint = (S - S * dTrans) * (1. / density_sample);
			col.rgb += T * Sint;
			T *= dTrans;
		}

		if (T <= 1e-1) break;

		pos += dir;
	}
	col.a = 1.0 - T;

	return col;
}

float ComputeFogAmount(vec3 startPosition, float factor) {
	float dist = length(startPosition - uCameraPosition);
	float radius = (uCameraPosition.y - uSphereCenter.y) * 0.3;
	float alpha = dist / radius;
	return 1. - exp(-dist * alpha * factor);
}

vec4 PostProcess(vec4 color, vec4 background, vec3 rayDirection, float fogAmount) {
	float cloudAlpha = color.a > 0.2 ? color.a : 0;
	color.rgb = color.rgb * 1.8 - 0.1;
	vec3 ambientColor = background.rgb;
    color.rgb = mix(color.rgb, background.rgb * color.a, clamp(fogAmount, 0., 1.));
    background.rgb = background.rgb * (1.0 - color.a) + color.rgb;
	background.a = 1.0;
    vec4 fragColor = background;
	fragColor.a = cloudAlpha;
    return fragColor;
}

void main() {
    if (gl_GlobalInvocationID.x >= uScreenSize.x || gl_GlobalInvocationID.y >= uScreenSize.y) {
        return;
    }
    vec4 clipSpaceCoord = vec4(vec2(gl_GlobalInvocationID.xy) / uScreenSize * 2 - 1, 1, 1);
    vec4 worldCoord = uInvViewProjectionMatrix * clipSpaceCoord;
    worldCoord /= worldCoord.w;
    vec3 rayDirection = normalize(vec3(worldCoord) - uCameraPosition);

    vec3 startPosition, endPosition;
    RaySphereIntersection(
        uCameraPosition, rayDirection, 
        uSphereCenter, uSphereRadius + uInnerHeight, 
        startPosition
    );
	RaySphereIntersection(
        uCameraPosition, rayDirection,
        uSphereCenter, uSphereRadius + uInnerHeight + uOuterHeight,
        endPosition
    );	

    vec4 background = vec4(1);
    float fogAmount = ComputeFogAmount(startPosition, 0.00006);
    if (fogAmount > 0.965){
		imageStore(uFragColor, ivec2(gl_GlobalInvocationID.xy), background);
		return;
	}

    vec4 cloudPosition;
    vec4 color = RayMarchToCloud(startPosition, endPosition, background.rgb, cloudPosition);

    imageStore(
        uFragColor, ivec2(gl_GlobalInvocationID), 
        PostProcess(color, background, rayDirection, fogAmount)
    );
}
)";

const std::string Clouds::kFsSource = R"(
#version 410 core

uniform sampler2D uScreenTexture;
uniform sampler2D uCloudTexture;
uniform sampler2D uDepthTexture;
uniform vec2 uScreenSize;

out vec4 fragColor;

void main() {
    vec2 texCoord = gl_FragCoord.xy / uScreenSize;

    vec4 cloud = texture(uCloudTexture, texCoord);
    vec4 background = texture(uScreenTexture, texCoord);
    float a = texture(uDepthTexture, texCoord).r < 1.0 ? 0.0 : 1.0;
    fragColor = mix(background, cloud, a);
}
)";
