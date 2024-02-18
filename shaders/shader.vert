#version 450

layout(binding = 0) uniform ViewData {
  mat4 model;
  mat4 view;
  mat4 proj;
} vd;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexPos;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexPos;

void main() {
    gl_Position = vd.proj * vd.view * vd.model * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexPos = inTexPos;
}
