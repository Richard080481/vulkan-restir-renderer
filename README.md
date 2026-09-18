# Vulkan ReSTIR Direct Lighting Renderer

A from-scratch Vulkan renderer focused on measurable, debuggable ReSTIR direct lighting. The project is being built as a graphics-engineering portfolio piece: correctness, synchronization, reproducible benchmarks, and clear technical documentation are first-class features.

> Status: **Milestone 1 in progress — observable renderer foundation.** The application creates a native Win32 window, selects a suitable GPU, creates a Vulkan 1.3 swapchain, presents a cleared frame using Synchronization2, compiles Slang to SPIR-V, and reports GPU timestamps. ReSTIR is not implemented yet.

## Current features

- Native Win32 surface with no windowing-library dependency
- Vulkan 1.3 device selection, preferring a discrete GPU
- Optional `VK_LAYER_KHRONOS_validation` integration
- Resize-safe swapchain recreation
- Two frames in flight with per-image fence tracking
- Explicit `VkImageMemoryBarrier2` layout transitions
- GPU timestamp query for the current frame workload
- RenderDoc/Nsight-compatible command-buffer debug label
- Build-time Slang to SPIR-V compilation
- Runtime report of `VK_KHR_acceleration_structure` and `VK_KHR_ray_query` availability

## Build

Requirements:

- Windows 10 or 11
- Visual Studio 2022 with the Desktop development with C++ workload
- CMake 3.25 or newer
- Vulkan SDK 1.3 or newer

From PowerShell:

```powershell
cmake --preset windows-debug
cmake --build --preset debug
./build/windows-debug/Debug/vulkan-restir.exe
```

Validation is enabled by default. It can be disabled at configure time with `-DVRR_ENABLE_VALIDATION=OFF`.
For a finite smoke test, pass `--frames=120`; the application exits after presenting that many frames.

## Roadmap

- [x] Native window, Vulkan instance/device, swapchain, and Synchronization2
- [x] GPU timestamp query profiler and debug labels
- [ ] Slang build pipeline and shader hot reload (build pipeline complete)
- [ ] Rasterized PBR G-buffer and camera motion vectors
- [ ] glTF scene and material loading
- [ ] Static BLAS/TLAS and ray-query shadow visibility
- [ ] Brute-force and uniform-NEE reference renderers
- [ ] Initial ReSTIR reservoir sampling
- [ ] Temporal reuse with disocclusion rejection
- [ ] Spatial reuse and visibility evaluation
- [ ] Reproducible quality and performance benchmark suite

See [the architecture notes](docs/architecture.md) and [milestone acceptance criteria](docs/milestones.md) for the technical plan.

## Scope

The initial target is Windows and Vulkan ray queries. ReSTIR DI is the focus; global illumination, denoising, animated skinned meshes, and neural materials are intentionally out of scope until the direct-lighting implementation is correct and measured.

## License

[MIT](LICENSE)
