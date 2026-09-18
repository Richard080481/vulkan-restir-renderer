# Architecture

## Target frame graph

```text
Scene update
    |
    +--> TLAS update (only when instance transforms change)
    |
    v
Raster G-buffer
    |
    +--> depth / normal / material / motion vectors
    |
    v
Initial candidates --> Temporal reuse --> Spatial reuse
                                             |
                                             v
                                  Ray-query visibility
                                             |
                                             v
                                    HDR composition
                                             |
                                             v
                                        Tone mapping
```

The first implementation uses a raster G-buffer followed by compute passes. Visibility is evaluated with `VK_KHR_ray_query`. This keeps reservoir logic in ordinary compute shaders and avoids coupling the first ReSTIR implementation to shader binding table management.

## Synchronization model

Milestone 0 uses two frames in flight. Every frame owns one command buffer, image-available semaphore, and fence. Every swapchain image owns its presentation-wait semaphore because presentation completion is not covered by a frame's graphics fence. Swapchain images additionally track the fence of the last frame that rendered to them.

Later passes will be ordered with Synchronization2 barriers. Resource ownership and intended transitions will be documented next to the frame graph rather than hidden in a generic barrier helper.

Expected reservoir dependencies:

```text
previous-frame reservoir (shader read)
             +
current initial reservoir (shader write)
             |
             v
temporal output (shader write)
             |
      compute barrier
             v
spatial input/output (shader read/write, separate images or buffers)
             |
      compute barrier
             v
visibility and composition
```

## Correctness strategy

The optimized renderer will not be treated as its own reference. A brute-force direct-lighting mode and a high-sample uniform-NEE mode will provide comparison images. Benchmark scenes, camera paths, random seeds, and renderer settings will be serializable so results can be reproduced.
