#version 460 core

out vec4 fragColor;

in vec3 vLocalPosition;

uniform samplerCube uTexture;

void main() {
    const float PI = radians(180);
    vec3 normal = normalize(vLocalPosition);

    vec3 irradiance = vec3(0.0);

    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, normal));
    up = normalize(cross(normal, right));

    int numSamplesPhi = 256;
    int numSamplesTheta = numSamplesPhi / 4; 

    for (int x = 0; x < numSamplesPhi; x++)
        for (int y = 0; y < numSamplesTheta; y++) {
            float phi = 2 * PI * x / numSamplesPhi;
            float theta = 0.5 * PI * y / numSamplesTheta;
            vec3 tangentSample = vec3(sin(theta) * cos(phi),  sin(theta) * sin(phi), cos(theta));
            vec3 sampleVector = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal; 
            irradiance += texture(uTexture, sampleVector).rgb * cos(theta) * sin(theta);
        }

    irradiance = PI * irradiance * (1.0 / (numSamplesPhi * numSamplesTheta));

    fragColor = vec4(irradiance, 1.0);
}
