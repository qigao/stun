#version 450

layout(location = 0) in vec2 uv_varying;
layout(location = 1) in vec2 uv_tex_varying;
layout(location = 2) flat in int instance_index;

layout(location = 0) out vec4 outColor;

struct RoundedRectData
{
    vec4 color;
    vec4 vertices[2];
    vec4 border_radius;
    vec4 sampler_index1_use_tint1_resolution2;
    vec4 uvs;
    vec4 line_width;
    vec4 shadow_blur2_is_msdf1;
};

layout(std430, set = 0, binding = 0) readonly buffer SSBO
{
    RoundedRectData payload[];
} ssbo;

layout(set = 0, binding = 1) uniform sampler2D textures[10];

const int MORPH_KERNEL_RADIUS = 2;
const int EFFECT_ANALYTIC_ELLIPSE_FILL = 5;
const int EFFECT_ANALYTIC_ELLIPSE_STROKE = 6;

float sdRoundedBox(vec2 p, vec2 b, vec4 r)
{
    r.xy = (p.x > 0.0) ? r.xy : r.zw;
    r.x = (p.y > 0.0) ? r.x : r.y;
    vec2 q = abs(p) - b + r.x;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r.x;
}

float sdEllipse(vec2 p, vec2 radii)
{
    radii = max(radii, vec2(0.0001));
    float k0 = length(p / radii);
    float k1 = length(p / (radii * radii));
    return k0 * (k0 - 1.0) / max(k1, 0.0001);
}

float boundedAlpha(int index, vec2 uv, vec2 uv_minimum, vec2 uv_maximum)
{
    float in_bounds = step(uv_minimum.x, uv.x) * step(uv_minimum.y, uv.y) *
                      step(uv.x, uv_maximum.x) * step(uv.y, uv_maximum.y);
    return texture(textures[index], uv).a * in_bounds;
}

float morphAlpha(int index, vec2 uv, vec2 basis_x, vec2 basis_y,
                 vec2 uv_minimum, vec2 uv_maximum, bool dilate)
{
    float result = boundedAlpha(index, uv, uv_minimum, uv_maximum);
    for (int y = -MORPH_KERNEL_RADIUS; y <= MORPH_KERNEL_RADIUS; ++y)
    {
        for (int x = -MORPH_KERNEL_RADIUS; x <= MORPH_KERNEL_RADIUS; ++x)
        {
            if (x * x + y * y > MORPH_KERNEL_RADIUS * MORPH_KERNEL_RADIUS)
                continue;
            vec2 sample_uv = uv + float(x) * basis_x + float(y) * basis_y;
            float sample_alpha = boundedAlpha(index, sample_uv, uv_minimum, uv_maximum);
            result = dilate ? max(result, sample_alpha) : min(result, sample_alpha);
        }
    }
    return result;
}

void main()
{
    RoundedRectData data = ssbo.payload[instance_index];
    vec4 border_radius = data.border_radius * 2.0;
    vec4 color = data.color;
    vec4 effect = data.line_width;
    int effect_mode = int(data.shadow_blur2_is_msdf1.w);
    float line_width = effect_mode == EFFECT_ANALYTIC_ELLIPSE_STROKE
        ? effect.x * 2.0
        : (effect_mode > 0 ? 0.0 : effect.x * 2.0);
    int index = int(data.sampler_index1_use_tint1_resolution2.x);
    int use_tint = int(data.sampler_index1_use_tint1_resolution2.y);
    vec2 resolution = data.sampler_index1_use_tint1_resolution2.zw;
    vec2 shadow_blur = vec2(data.shadow_blur2_is_msdf1.x,
                            data.shadow_blur2_is_msdf1.z);
    int is_msdf = int(data.shadow_blur2_is_msdf1.y);

    float dominant_axis = (resolution.x / resolution.y > 0.0) ? resolution.x : resolution.y;
    vec4 border_radius_norm = border_radius / dominant_axis;
    vec2 resolution_norm = resolution / dominant_axis;
    float line_width_norm = line_width / dominant_axis;
    float shadow_blur_norm = shadow_blur.x / dominant_axis;

    float dist = 1.0 -
        (sdRoundedBox((uv_varying - 0.5) * 2.0 * (resolution_norm + shadow_blur_norm),
                      resolution_norm, border_radius_norm) -
         shadow_blur_norm);
    float bias = min(fwidth(dist), line_width);
    float shape = smoothstep(1.0, 1.0 + bias, dist);

    if (effect_mode == EFFECT_ANALYTIC_ELLIPSE_FILL ||
        effect_mode == EFFECT_ANALYTIC_ELLIPSE_STROKE)
    {
        vec2 position = (uv_varying - 0.5) * resolution;
        vec2 radii = resolution * 0.5;
        float distance = sdEllipse(position, radii);
        float antialias = max(fwidth(distance), 0.0001);
        shape = 1.0 - smoothstep(-antialias, antialias, distance);
        if (effect_mode == EFFECT_ANALYTIC_ELLIPSE_STROKE)
        {
            vec2 inner_radii = max(radii - vec2(effect.x), vec2(0.0001));
            float inner_distance = sdEllipse(position, inner_radii);
            shape *= smoothstep(-antialias, antialias, inner_distance);
        }
    }
    else if (effect_mode == 1 || effect_mode == 2)
    {
        vec2 position = (uv_varying - 0.5) * resolution;
        float original_distance = effect_mode == 2
            ? sdEllipse(position, resolution * 0.5)
            : sdRoundedBox(position, resolution * 0.5, data.border_radius);
        vec2 shifted_radii = max(resolution * 0.5 - vec2(effect.z), vec2(0.0001));
        float shifted_distance = effect_mode == 2
            ? sdEllipse(position - effect.xy, shifted_radii)
            : sdRoundedBox(position - effect.xy, shifted_radii,
                           max(data.border_radius - vec4(effect.z), vec4(0.0)));
        float antialias = max(fwidth(original_distance), 0.0001);
        float inside = 1.0 - smoothstep(-antialias, antialias, original_distance);
        float softness = max(shadow_blur.x, antialias);
        shape = inside * smoothstep(-softness, 0.0, shifted_distance);
    }
    else if (shadow_blur.x > 0.0 || shadow_blur.y > 0.0)
    {
        vec2 shadow_position =
            (uv_varying - 0.5) * (resolution + shadow_blur * 2.0);
        float shadow_distance =
            sdRoundedBox(shadow_position, resolution * 0.5, data.border_radius);
        float blur_distance = max(shadow_blur.x, shadow_blur.y);
        shape = 1.0 - smoothstep(0.0, blur_distance, max(shadow_distance, 0.0));
    }
    else if (line_width > 0.0)
    {
        float inner = 1.0 - smoothstep(1.0 + line_width_norm,
                                       1.0 + line_width_norm + bias, dist);
        shape = min(inner, shape);
    }

    if (index <= -1)
    {
        outColor = vec4(color.rgb, shape * color.a);
        return;
    }

    vec4 image = texture(textures[index], uv_tex_varying.xy);
    if (effect_mode == 3)
    {
        vec2 shifted_uv = uv_tex_varying.xy - effect.xy;
        vec2 uv_minimum = data.uvs.xy;
        vec2 uv_maximum = data.uvs.zw;
        float shifted_alpha = morphAlpha(
            index, shifted_uv, data.border_radius.xy, data.border_radius.zw,
            uv_minimum, uv_maximum, true);
        outColor = vec4(color.rgb, shape * image.a * (1.0 - shifted_alpha) * color.a);
    }
    else if (effect_mode == 4)
    {
        float eroded_alpha = morphAlpha(
            index, uv_tex_varying.xy, data.border_radius.xy, data.border_radius.zw,
            data.uvs.xy, data.uvs.zw, false);
        outColor = vec4(color.rgb, shape * eroded_alpha * color.a);
    }
    else if (is_msdf > 0)
    {
        float distance = image.r;
        float antialias = fwidth(distance);
        float coverage = smoothstep(0.5 - antialias, 0.5 + antialias, distance);
        outColor = vec4(color.rgb, shape * coverage * color.a);
    }
    else if (use_tint == 1)
    {
        outColor = vec4(image.rgb * color.rgb, shape * image.a * color.a);
    }
    else if (use_tint == 2)
    {
        outColor = vec4(image.rgb, shape * image.a * color.a);
    }
    else if (use_tint == 3)
    {
        outColor = vec4(color.rgb, shape * image.a * color.a);
    }
    else
    {
        outColor = vec4(image.rgb, shape * image.a);
    }
}
