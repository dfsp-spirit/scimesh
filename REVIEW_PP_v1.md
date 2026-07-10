Thoughts/Suggestions on current plan:

* **Return an in-memory `Image` object instead of writing PNGs** – The renderer produces an RGBA image in memory; file export becomes a separate utility layer.
* **Separate rendering from image export** – PNG (or other formats) are written by optional exporter functions or methods, not by the rendering algorithm itself.
* **Keep the renderer completely independent of R** – The C++ core should have no knowledge of R or Rcpp; the R package acts purely as a translation layer.
* **Replace the header-only architecture with a conventional multi-file C++ library** – Improves maintainability, compile times, and long-term extensibility while remaining dependency-free.
* **Cache computed normals** – Automatically compute missing vertex/face normals once and reuse them across multiple renderings of the same mesh.
* **Support both smooth (vertex) and flat (face) shading** – Smooth shading for publication-quality visualization; flat shading for mesh inspection and quality control.
* **Add near-plane clipping** – Prevents rendering artifacts and numerical instability when triangles intersect the camera's near plane.
* **Keep configurable backface culling** – Enabled by default for closed cortical meshes but optionally disabled for open surfaces.
* **Add optional wireframe rendering/overlay** – Useful for debugging, topology inspection, and quality-control visualizations.
* **Introduce a reusable `Image` abstraction** – A lightweight image class storing width, height, and RGBA pixels, with optional export methods.
* **Favor stateless rendering** – Each render depends only on the mesh, camera, lighting, and render options, making the renderer deterministic and easy to test.
* **Add image regression tests** – Validate renderer correctness by comparing generated images against known reference outputs.
* **Make the rendering pipeline explicit** – Model/View Transform → Projection → Near-Plane Clipping → Backface Culling → Rasterization → Z-Buffer → Shading → RGBA Image.
* **Keep multithreading out of the initial implementation** – Prioritize correctness and simplicity first; parallelization can be added later if needed.
