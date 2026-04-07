struct Particle {
    pos: vec4f,  // xyz = position, w = radius
    vel: vec4f,  // xyz = velocity, w = inverse_mass
};

struct SimParams {
    dt: f32,
    gravity: f32,
    count: u32,
    grid_res: u32,
    bounds_min: vec3f,
    _pad0: f32,
    bounds_max: vec3f,
    damping: f32,
    cell_size: f32,
    _pad1: f32,
    _pad2: f32,
    _pad3: f32,
};

@group(0) @binding(0) var<storage, read_write> particles: array<Particle>;
@group(0) @binding(1) var<uniform> params: SimParams;

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) gid: vec3u) {
    let idx = gid.x;
    if (idx >= params.count) { return; }

    var p = particles[idx];

    // Apply gravity
    p.vel.y = p.vel.y + params.gravity * params.dt;

    // Integrate position
    p.pos = vec4f(p.pos.xyz + p.vel.xyz * params.dt, p.pos.w);

    // Boundary collision (reflect + damp)
    let radius = p.pos.w;
    for (var axis = 0u; axis < 3u; axis = axis + 1u) {
        if (p.pos[axis] - radius < params.bounds_min[axis]) {
            p.pos[axis] = params.bounds_min[axis] + radius;
            p.vel[axis] = -p.vel[axis] * params.damping;
        } else if (p.pos[axis] + radius > params.bounds_max[axis]) {
            p.pos[axis] = params.bounds_max[axis] - radius;
            p.vel[axis] = -p.vel[axis] * params.damping;
        }
    }

    particles[idx] = p;
}
