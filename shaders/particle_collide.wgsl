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

@group(0) @binding(0) var<storage, read_write> particles: array<Particle>;
@group(0) @binding(1) var<uniform> params: SimParams;
@group(0) @binding(2) var<storage, read> grid_counts: array<u32>;
@group(0) @binding(3) var<storage, read> grid_entries: array<u32>;

fn pos_to_cell_3d(pos: vec3f) -> vec3i {
    let rel = (pos - params.bounds_min) / params.cell_size;
    return vec3i(
        clamp(i32(rel.x), 0, i32(params.grid_res) - 1),
        clamp(i32(rel.y), 0, i32(params.grid_res) - 1),
        clamp(i32(rel.z), 0, i32(params.grid_res) - 1),
    );
}

fn cell_index(cx: i32, cy: i32, cz: i32) -> u32 {
    let res = i32(params.grid_res);
    return u32(cx + cy * res + cz * res * res);
}

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) gid: vec3u) {
    let idx = gid.x;
    if (idx >= params.count) { return; }

    var p = particles[idx];
    let my_cell = pos_to_cell_3d(p.pos.xyz);
    let my_radius = p.pos.w;
    let res = i32(params.grid_res);

    // Check 27 neighboring cells
    for (var dz = -1; dz <= 1; dz = dz + 1) {
        for (var dy = -1; dy <= 1; dy = dy + 1) {
            for (var dx = -1; dx <= 1; dx = dx + 1) {
                let nx = my_cell.x + dx;
                let ny = my_cell.y + dy;
                let nz = my_cell.z + dz;

                if (nx < 0 || nx >= res || ny < 0 || ny >= res || nz < 0 || nz >= res) {
                    continue;
                }

                let cell = cell_index(nx, ny, nz);
                let count = min(grid_counts[cell], MAX_PER_CELL);

                for (var s = 0u; s < count; s = s + 1u) {
                    let other_idx = grid_entries[cell * MAX_PER_CELL + s];
                    if (other_idx == idx) { continue; }

                    let other = particles[other_idx];
                    let diff = p.pos.xyz - other.pos.xyz;
                    let dist = length(diff);
                    let min_dist = my_radius + other.pos.w;

                    if (dist < min_dist && dist > 0.0001) {
                        let normal = diff / dist;
                        let overlap = min_dist - dist;

                        // Position correction (push apart)
                        p.pos = vec4f(p.pos.xyz + normal * overlap * 0.5, p.pos.w);

                        // Velocity response (elastic collision)
                        let rel_vel = p.vel.xyz - other.vel.xyz;
                        let vel_along_normal = dot(rel_vel, normal);
                        if (vel_along_normal < 0.0) {
                            p.vel = vec4f(
                                p.vel.xyz - normal * vel_along_normal,
                                p.vel.w
                            );
                        }
                    }
                }
            }
        }
    }

    particles[idx] = p;
}
