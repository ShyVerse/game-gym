struct Particle {
    pos: vec4f,
    vel: vec4f,
};

struct Params {
    dt: f32,
    count: u32,
    _pad0: u32,
    _pad1: u32,
};

@group(0) @binding(0) var<storage, read_write> particles: array<Particle>;
@group(0) @binding(1) var<uniform> params: Params;

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) gid: vec3u) {
    let idx = gid.x;
    if (idx >= params.count) { return; }
    particles[idx].pos = particles[idx].pos + particles[idx].vel * params.dt;
}
