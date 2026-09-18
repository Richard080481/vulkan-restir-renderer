# Milestones and acceptance criteria

## M0 — Platform foundation

- Configure and build through CMake presets.
- Open, resize, minimize, restore, and close without validation errors.
- Select a present-capable Vulkan 1.3 device.
- Present a cleared swapchain image with explicit Synchronization2 barriers.

## M1 — Observable renderer foundation

- Add debug labels for every GPU scope.
- Add timestamp queries and report per-pass GPU milliseconds.
- Compile a minimal Slang shader to SPIR-V from the build system.
- Add a deterministic benchmark command-line mode.

## M2 — Scene and G-buffer

- Load one documented glTF test scene.
- Render depth, world-space normal, base color, material parameters, IDs, and motion vectors.
- Provide debug views for every attachment.
- Render Cook-Torrance direct lighting without ray-traced visibility.

## M3 — Ray-query visibility

- Build one BLAS per unique static mesh and one TLAS for scene instances.
- Evaluate hard-shadow visibility from a compute shader.
- Update the TLAS when instance transforms change.
- Document acceleration-structure memory, build time, and barriers.

## M4 — Reference estimators

- Implement brute-force all-light direct illumination.
- Implement uniform next-event estimation.
- Produce deterministic reference captures for 1, 100, 1,000, and 10,000 lights.

## M5 — ReSTIR initial sampling

- Unit-test weighted reservoir update and normalization on the CPU.
- Match the shader reservoir layout with static size/offset checks.
- Visualize selected light ID, sample count, weight sum, and invalid reservoirs.
- Demonstrate convergence toward the reference estimator without reuse.

## M6 — Temporal reuse

- Reproject with motion vectors.
- Reject history using depth, normal, material/instance identity, and camera-cut rules.
- Clamp history length and expose acceptance rate and average reservoir age.
- Demonstrate static noise reduction without persistent disocclusion trails.

## M7 — Spatial reuse and final visibility

- Support configurable neighbor count and sampling radius.
- Reject incompatible neighbors using documented thresholds.
- Expose rejection and reuse debug views.
- Compare biased and unbiased visibility modes if both are implemented.

## M8 — Portfolio release

- Publish fixed-camera and moving-camera benchmark results.
- Report GPU, driver, resolution, light distribution, candidate count, and quality metric.
- Include a two-minute demo, architecture diagram, four-to-six-page report, and known limitations.
- Reproduce all headline numbers from checked-in benchmark configuration files.
