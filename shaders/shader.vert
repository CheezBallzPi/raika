#version 450

layout(binding = 0) uniform ViewData {
  vec2 offset;
} vd;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexPos;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexPos;

void main() {
    gl_Position = vec4(vd.offset + inPosition, 0.0, 1.0);
    fragColor = inColor;
    fragTexPos = inTexPos;
}
