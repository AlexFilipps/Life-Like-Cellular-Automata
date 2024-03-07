#version 430 core
out vec4 FragColor;
  
in vec2 TexCoord;

uniform sampler2D densityTexture;

void main()
{
    //FragColor = vec4(1.0f, 0.0f, 1.0f, 1.0f);
    FragColor = texture(densityTexture, TexCoord);
}