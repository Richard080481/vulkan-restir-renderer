// Build-pipeline smoke test. This will become the first compute pass when the
// renderer gains storage images in M1.
[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    // Keep the entry point intentionally resource-free for the platform milestone.
}
