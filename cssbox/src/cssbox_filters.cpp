#include "cssbox_filters.h"
#include <nanovg_gl.h>
#include <map>
#include <string>
#include <cmath>
#include <glad/glad.h>
// Shader source code
static const char* vertex_shader_src = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 vTexCoord;

void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
)";

static const char* fragment_shader_base = R"(
#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform float uValue;
)";

// Filter-specific fragment shaders
static const char* blur_shader = R"(
void main() {
    vec2 texelSize = 1.0 / textureSize(uTexture, 0);
    vec4 color = vec4(0.0);
    float radius = uValue;
    int samples = int(radius * 2.0) + 1;
    float weight = 0.0;
    
    for (int x = -samples; x <= samples; x++) {
        for (int y = -samples; y <= samples; y++) {
            vec2 offset = vec2(x, y) * texelSize * radius;
            float dist = length(vec2(x, y));
            float w = exp(-dist * dist / (2.0 * radius * radius));
            color += texture(uTexture, vTexCoord + offset) * w;
            weight += w;
        }
    }
    FragColor = color / weight;
}
)";

static const char* brightness_shader = R"(
void main() {
    vec4 color = texture(uTexture, vTexCoord);
    FragColor = vec4(color.rgb * uValue, color.a);
}
)";

static const char* contrast_shader = R"(
void main() {
    vec4 color = texture(uTexture, vTexCoord);
    FragColor = vec4((color.rgb - 0.5) * uValue + 0.5, color.a);
}
)";

static const char* grayscale_shader = R"(
void main() {
    vec4 color = texture(uTexture, vTexCoord);
    float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));
    FragColor = vec4(mix(color.rgb, vec3(gray), uValue), color.a);
}
)";

static const char* hue_rotate_shader = R"(
vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main() {
    vec4 color = texture(uTexture, vTexCoord);
    vec3 hsv = rgb2hsv(color.rgb);
    hsv.x = fract(hsv.x + uValue / 360.0);
    FragColor = vec4(hsv2rgb(hsv), color.a);
}
)";

static const char* invert_shader = R"(
void main() {
    vec4 color = texture(uTexture, vTexCoord);
    FragColor = vec4(mix(color.rgb, 1.0 - color.rgb, uValue), color.a);
}
)";

static const char* saturate_shader = R"(
void main() {
    vec4 color = texture(uTexture, vTexCoord);
    float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));
    FragColor = vec4(mix(vec3(gray), color.rgb, uValue), color.a);
}
)";

static const char* sepia_shader = R"(
void main() {
    vec4 color = texture(uTexture, vTexCoord);
    vec3 sepia = vec3(
        dot(color.rgb, vec3(0.393, 0.769, 0.189)),
        dot(color.rgb, vec3(0.349, 0.686, 0.168)),
        dot(color.rgb, vec3(0.272, 0.534, 0.131))
    );
    FragColor = vec4(mix(color.rgb, sepia, uValue), color.a);
}
)";

static const char* opacity_shader = R"(
void main() {
    vec4 color = texture(uTexture, vTexCoord);
    FragColor = vec4(color.rgb, color.a * uValue);
}
)";

// Filter context structure
struct cssboxFilterContext {
    GLuint fbo;
    GLuint texture;
    GLuint vao, vbo;
    int width, height;
    std::map<cssboxFilterType, GLuint> shaders;
    
    cssboxFilterContext() : fbo(0), texture(0), vao(0), vbo(0), width(0), height(0) {}
};

// Compile shader
static GLuint compile_shader(const char* vertex_src, const char* fragment_src) {
    GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vertex_src, nullptr);
    glCompileShader(vertex);
    
    GLint success;
    glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(vertex, 512, nullptr, log);
        glDeleteShader(vertex);
        return 0;
    }
    
    GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fragment_src, nullptr);
    glCompileShader(fragment);
    
    glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(fragment, 512, nullptr, log);
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return 0;
    }
    
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(program, 512, nullptr, log);
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        glDeleteProgram(program);
        return 0;
    }
    
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    
    return program;
}

// Create filter context
cssboxFilterContext* cssboxCreateFilterContext(void) {
    cssboxFilterContext* ctx = new cssboxFilterContext();
    
    // Create FBO and texture
    glGenFramebuffers(1, &ctx->fbo);
    glGenTextures(1, &ctx->texture);
    
    // Create fullscreen quad
    float quad[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    
    glGenVertexArrays(1, &ctx->vao);
    glGenBuffers(1, &ctx->vbo);
    glBindVertexArray(ctx->vao);
    glBindBuffer(GL_ARRAY_BUFFER, ctx->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    
    // Compile shaders
    std::string base(fragment_shader_base);
    ctx->shaders[cssbox_FILTER_BLUR] = compile_shader(vertex_shader_src, (base + blur_shader).c_str());
    ctx->shaders[cssbox_FILTER_BRIGHTNESS] = compile_shader(vertex_shader_src, (base + brightness_shader).c_str());
    ctx->shaders[cssbox_FILTER_CONTRAST] = compile_shader(vertex_shader_src, (base + contrast_shader).c_str());
    ctx->shaders[cssbox_FILTER_GRAYSCALE] = compile_shader(vertex_shader_src, (base + grayscale_shader).c_str());
    ctx->shaders[cssbox_FILTER_HUE_ROTATE] = compile_shader(vertex_shader_src, (base + hue_rotate_shader).c_str());
    ctx->shaders[cssbox_FILTER_INVERT] = compile_shader(vertex_shader_src, (base + invert_shader).c_str());
    ctx->shaders[cssbox_FILTER_SATURATE] = compile_shader(vertex_shader_src, (base + saturate_shader).c_str());
    ctx->shaders[cssbox_FILTER_SEPIA] = compile_shader(vertex_shader_src, (base + sepia_shader).c_str());
    ctx->shaders[cssbox_FILTER_OPACITY] = compile_shader(vertex_shader_src, (base + opacity_shader).c_str());
    
    return ctx;
}

// Delete filter context
void cssboxDeleteFilterContext(cssboxFilterContext* ctx) {
    if (!ctx) return;
    
    if (ctx->fbo) glDeleteFramebuffers(1, &ctx->fbo);
    if (ctx->texture) glDeleteTextures(1, &ctx->texture);
    if (ctx->vao) glDeleteVertexArrays(1, &ctx->vao);
    if (ctx->vbo) glDeleteBuffers(1, &ctx->vbo);
    
    for (auto& pair : ctx->shaders) {
        if (pair.second) glDeleteProgram(pair.second);
    }
    
    delete ctx;
}

// Resize FBO if needed
static void resize_fbo(cssboxFilterContext* ctx, int width, int height) {
    if (ctx->width == width && ctx->height == height) return;
    
    ctx->width = width;
    ctx->height = height;
    
    glBindTexture(GL_TEXTURE_2D, ctx->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    glBindFramebuffer(GL_FRAMEBUFFER, ctx->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ctx->texture, 0);
    
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Apply filters
int cssboxApplyFilters(
    cssboxFilterContext* ctx,
    NVGcontext* vg,
    float x, float y,
    float width, float height,
    const cssboxFilter* filters,
    int filter_count,
    void (*render_callback)(NVGcontext*, void*),
    void* user_data)
{
    if (!ctx || !vg || !filters || filter_count == 0) return 0;
    
    // Resize FBO if needed
    int w = (int)std::ceil(width);
    int h = (int)std::ceil(height);
    resize_fbo(ctx, w, h);
    
    // Save GL state
    GLint prev_fbo;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
    
    // Render to FBO
    glBindFramebuffer(GL_FRAMEBUFFER, ctx->fbo);
    glViewport(0, 0, w, h);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    
    if (render_callback) {
        render_callback(vg, user_data);
    }
    
    // Apply filters
    glBindVertexArray(ctx->vao);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    for (int i = 0; i < filter_count; i++) {
        const cssboxFilter& filter = filters[i];
        
        auto it = ctx->shaders.find(filter.type);
        if (it == ctx->shaders.end() || it->second == 0) continue;
        
        GLuint shader = it->second;
        glUseProgram(shader);
        glUniform1i(glGetUniformLocation(shader, "uTexture"), 0);
        glUniform1f(glGetUniformLocation(shader, "uValue"), filter.value);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ctx->texture);
        
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    
    // Restore GL state
    glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
    glBindVertexArray(0);
    glUseProgram(0);
    
    // Render filtered texture back to screen
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, ctx->texture);
    
    // Use NanoVG to render the texture
    NVGpaint paint = nvgImagePattern(vg, x, y, width, height, 0, ctx->texture, 1.0f);
    nvgBeginPath(vg);
    nvgRect(vg, x, y, width, height);
    nvgFillPaint(vg, paint);
    nvgFill(vg);
    
    return 1;
}
