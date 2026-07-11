# Plan: Fix medial views showing wrong hemisphere

## Root cause

In `vis.subject.morph.native()`, every view renders the FULL scene (both lh and rh):
```r
img <- render_scene(meshes, cam, ropts)  # always passes all meshes
```

The C++ `camera_fit_mesh()` positions the camera at `center + dir * dist` to ensure
the entire bounding box of the fit mesh is visible.  For a hemisphere ~30 units
wide but ~80 units tall, `dist` can be ~130 units.

For `medial_lh` (dir = (1,0,0)): camera goes to x ≈ +100 looking left at lh (x ≈ -35).
The rh (x ≈ +35) sits **between** camera and lh → rh's lateral surface is in front.

For `medial_rh` (dir = (-1,0,0)): camera goes to x ≈ -100 looking right at rh (x ≈ +35).
The lh (x ≈ -35) sits **between** camera and rh → lh's lateral surface is in front.

Thus:
- `lateral_lh` ≈ `medial_rh` — both show lh lateral (in different positions → different cropped sizes)
- `lateral_rh` ≈ `medial_lh` — both show rh lateral

## Fix

### `R/vis.R` — Render only relevant hemisphere for hemi-specific views

In the render loop (around line 401-407), change:

```r
for (vn in view_names) {
    cam <- .view_to_camera(vn, meshes)
    img <- render_scene(meshes, cam, ropts)
    images <- c(images, list(img))
}
```

To:

```r
for (vn in view_names) {
    cam <- .view_to_camera(vn, meshes)
    # Determine which mesh(es) to render
    clean <- sub("^(sd_|si_|sr_)", "", vn)
    if (grepl("_lh$", clean) && !is.null(meshes$lh)) {
        scene <- meshes["lh"]
    } else if (grepl("_rh$", clean) && !is.null(meshes$rh)) {
        scene <- meshes["rh"]
    } else {
        scene <- meshes
    }
    img <- render_scene(scene, cam, ropts)
    images <- c(images, list(img))
}
```

But wait — `render_scene()` expects a list of mesh descriptors where each
element has `$vertices` and `$triangles`.  `meshes["lh"]` returns a list
with one element (named "lh"), which is correct.  But we need to re-fit the
camera AFTER deciding which mesh to render, because the camera was already
fit in `.view_to_camera()`.

Actually, the camera fitting is already done in `.view_to_camera()` and
targets the correct mesh (e.g., `meshes[["lh"]]` for lateral_lh).  The issue
is only about which meshes are in the scene.  So the camera is correct;
we just need to restrict the scene.

Better approach: make `render_scene()` the decision point, or pass the
selected subset from vis.R.

### Alternative approach: camera fit to full scene

Another option: for hemi-specific views, fit the camera to just that hemi
but render just that hemi.  The camera was already fit correctly (fit to
the relevant hemi via `.view_to_camera`).  We just need to pass only that
hemi's mesh to render_scene.

### Implementation

In `R/vis.R`, modify the render loop at lines 403-407:

```r
    # ---- Render each view ----
    images <- list()
    for (vn in view_names) {
        cam <- .view_to_camera(vn, meshes)
        clean <- sub("^(sd_|si_|sr_)", "", vn)
        if (grepl("_lh$", clean) && !is.null(meshes[["lh"]])) {
            scene <- list(meshes[["lh"]])
        } else if (grepl("_rh$", clean) && !is.null(meshes[["rh"]])) {
            scene <- list(meshes[["rh"]])
        } else {
            scene <- meshes
        }
        img <- render_scene(scene, cam, ropts)
        images <- c(images, list(img))
    }
```

Wait — but `render_scene` validates that each element is a mesh descriptor:
```r
for (i in seq_along(meshes)) {
    m <- meshes[[i]]
    if (!is.list(m) || is.null(m$vertices) || is.null(m$triangles)) {
        stop(...)
    }
}
```

`list(meshes[["lh"]])` is a list with one element, and that element is a
mesh descriptor with `$vertices`, `$triangles`, etc. So it should pass validation.

### Tests

Add a test that verifies medial views differ from lateral views:

```r
test_that("medial views differ from lateral views", {
    skip_if_not_installed("freesurferformats")
    # ... setup ...
    
    img_lat_lh <- vis.subject.morph.native(sjd, sj, "sulc",
        views = "lateral_lh", width = 200L, height = 150L)
    img_med_lh <- vis.subject.morph.native(sjd, sj, "sulc",
        views = "medial_lh", width = 200L, height = 150L)
    
    arr_lat <- image_to_array(img_lat_lh)
    arr_med <- image_to_array(img_med_lh)
    
    # The pixel content should differ (different views)
    diff <- mean(abs(arr_lat - arr_med))
    expect_true(diff > 0.01)
})
```

### Verify with example

Run the example after fix to confirm:
- t4 output has 4 distinct views (2 lateral, 2 medial)
- Medial views show the inner (medial) surface with the cut-out medial wall
