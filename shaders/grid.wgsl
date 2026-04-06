struct GridUniforms {
    view_proj: mat4x4<f32>,
};
@group(0) @binding(0) var<uniform> grid: GridUniforms;

struct VertexOutput {
    @builtin(position) clip_position: vec4f,
    @location(0) world_pos: vec3f,
};

@vertex
fn vs_main(@builtin(vertex_index) vi: u32) -> VertexOutput {
    let half = 100.0;
    var positions = array<vec2f, 6>(
        vec2f(-half, -half),
        vec2f( half, -half),
        vec2f( half,  half),
        vec2f(-half, -half),
        vec2f( half,  half),
        vec2f(-half,  half),
    );
    let xz = positions[vi];

    var out: VertexOutput;
    let world = vec3f(xz.x, 0.0, xz.y);
    out.clip_position = grid.view_proj * vec4f(world, 1.0);
    out.world_pos = world;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let coord = in.world_pos.xz;

    let grid_size = 1.0;
    let derivative = fwidth(coord);
    let grid_line = abs(fract(coord / grid_size - 0.5) - 0.5) / derivative;
    let line = min(grid_line.x, grid_line.y);
    let grid_alpha = 1.0 - min(line, 1.0);

    let dist = length(coord);
    let fade = 1.0 - smoothstep(20.0, 50.0, dist);

    var color = vec3f(0.4, 0.4, 0.4);

    let z_line = abs(coord.y) / derivative.y;
    if z_line < 1.0 {
        color = vec3f(0.9, 0.2, 0.2);
    }

    let x_line = abs(coord.x) / derivative.x;
    if x_line < 1.0 {
        color = vec3f(0.2, 0.9, 0.2);
    }

    let alpha = grid_alpha * fade * 0.6;
    if alpha < 0.01 {
        discard;
    }
    return vec4f(color, alpha);
}
