struct Particle {
    pos: vec4f,
    vel: vec4f,
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

const MAX_PER_CELL: u32 = 8u;

@group(0) @binding(0) var<storage, read> particles: array<Particle>;
@group(0) @binding(1) var<uniform> params: SimParams;
@group(0) @binding(2) var<storage, read_write> grid_counts: array<atomic<u32>>;
@group(0) @binding(3) var<storage, read_write> grid_entries: array<u32>;

fn pos_to_cell(pos: vec3f) -> u32 {
    let rel = (pos - params.bounds_min) / params.cell_size;
    let cx = clamp(u32(rel.x), 0u, params.grid_res - 1u);
    let cy = clamp(u32(rel.y), 0u, params.grid_res - 1u);
    let cz = clamp(u32(rel.z), 0u, params.grid_res - 1u);
    return cx + cy * params.grid_res + cz * params.grid_res * params.grid_res;
}

@compute @workgroup_size(64)
fn clear(@builtin(global_invocation_id) gid: vec3u) {
    let idx = gid.x;
    let total_cells = params.grid_res * params.grid_res * params.grid_res;
    if (idx >= total_cells) { return; }
    atomicStore(&grid_counts[idx], 0u);
    // Touch all bindings so wgpu auto-layout includes every binding in the group.
    _ = particles[0].pos;
    grid_entries[idx * MAX_PER_CELL] = 0u;
}

@compute @workgroup_size(64)
fn insert(@builtin(global_invocation_id) gid: vec3u) {
    let idx = gid.x;
    if (idx >= params.count) { return; }

    let cell = pos_to_cell(particles[idx].pos.xyz);
    let slot = atomicAdd(&grid_counts[cell], 1u);
    if (slot < MAX_PER_CELL) {
        grid_entries[cell * MAX_PER_CELL + slot] = idx;
    }
}
