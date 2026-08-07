/// @file image.h
/// @brief The Image — an RGBA pixel buffer with compositing and I/O operations.
///
/// scimesh uses its own image class so that rendering is self-contained:
/// no external image libraries are needed for basic operations.
/// Pixel data is stored as raw RGBA bytes in row-major order.

#pragma once

#include <scimesh/types.h>
#include <vector>
#include <cstdint>
#include <string>

namespace scimesh {

// ---------------------------------------------------------------------------
//  Enums for image operations
// ---------------------------------------------------------------------------

/// @brief Direction for the merge() operation.
///
/// Controls which side of the base image the other image is attached to.
/// @see Image::merge()
enum class MergeDirection {
    LEFT,   ///< Attach `other` to the left side.
    RIGHT,  ///< Attach `other` to the right side.
    TOP,    ///< Attach `other` above.
    BOTTOM  ///< Attach `other` below.
};

/// @brief Direction(s) for the crop_to_content() operation.
///
/// Specifies which edges to crop.  You can crop individual edges or
/// combinations.
/// @see Image::crop_to_content()
enum class CropContentDirection {
    LEFT,       ///< Crop left edge only.
    RIGHT,      ///< Crop right edge only.
    HORIZONTAL, ///< Crop both left and right edges.
    TOP,        ///< Crop top edge only.
    BOTTOM,     ///< Crop bottom edge only.
    VERTICAL,   ///< Crop both top and bottom edges.
    ALL         ///< Crop all four edges.
};

/// @brief Strategy for normalizing images to a common cell size in grid_arrange().
///
/// @see grid_arrange()
enum class FitMode {
    PAD,   ///< Pad smaller images with background color (content stays pixel-perfect).
    SCALE  ///< Scale all images to match the largest cell dimensions.
};

// ---------------------------------------------------------------------------
//  Image
// ---------------------------------------------------------------------------

/// @brief A 2D RGBA image buffer.
///
/// ## Overview
///
/// The `Image` class stores pixels as 4 bytes per pixel (red, green, blue,
/// alpha) in **row-major** order.  Pixel (0, 0) is the **bottom-left**
/// corner (matching OpenGL texture convention).
///
/// Images are used both as render targets (the output of the renderer)
/// and as texture sources for textured meshes.
///
/// ## Construction
///
/// @code{.cpp}
/// Image img(800, 600);              // 800×600 blank (transparent black)
/// img.clear(1.0f, 1.0f, 1.0f, 1.0f); // fill with opaque white
/// @endcode
///
/// ## Saving
///
/// @code{.cpp}
/// img.write_png("output.png");   // PNG (recommended)
/// img.write_ppm("output.ppm");   // PPM (simple text/binary format, for debugging)
/// img.write_bmp("output.bmp");   // BMP (for debugging)
/// img.write_tga("output.tga");   // TGA (uncompressed true-color, no dependencies)
/// @endcode
///
/// @see Mesh::texture, Renderer
struct Image {
    /// @brief Image width in pixels.
    int width = 0;

    /// @brief Image height in pixels.
    int height = 0;

    /// @brief Raw pixel data: RGBA bytes, row-major, bottom-left origin.
    ///
    /// Size is `width * height * 4` bytes.  Pixel at (x, y) is at offset
    /// `(y * width + x) * 4`.
    std::vector<uint8_t> pixels;

    /// @brief Construct an empty (0×0) image.
    Image() = default;

    /// @brief Construct an image of the given size, initialized to
    ///        transparent black (0, 0, 0, 0).
    /// @param w Width in pixels.
    /// @param h Height in pixels.
    Image(int w, int h);

    /// @brief Set a single pixel's RGBA value.
    /// @param x X coordinate (0 = left).
    /// @param y Y coordinate (0 = bottom).
    /// @param r Red channel   (0–255).
    /// @param g Green channel (0–255).
    /// @param b Blue channel  (0–255).
    /// @param a Alpha channel (0–255, 255 = fully opaque).
    void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

    /// @brief Get a single pixel's RGBA value.
    /// @param[in]  x X coordinate (0 = left).
    /// @param[in]  y Y coordinate (0 = bottom).
    /// @param[out] r Red channel   (0–255).
    /// @param[out] g Green channel (0–255).
    /// @param[out] b Blue channel  (0–255).
    /// @param[out] a Alpha channel (0–255).
    void get_pixel(int x, int y, uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &a) const;

    /// @brief Fill the entire image with an RGBA color (byte values 0–255).
    /// @param r Red channel.
    /// @param g Green channel.
    /// @param b Blue channel.
    /// @param a Alpha channel.
    void clear(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

    /// @brief Fill the entire image with an RGBA color (float values 0.0–1.0).
    ///
    /// This is a convenience wrapper that converts floats to bytes internally.
    ///
    /// @param r Red channel   (0.0–1.0).
    /// @param g Green channel (0.0–1.0).
    /// @param b Blue channel  (0.0–1.0).
    /// @param a Alpha channel (0.0–1.0).
    void clear_float(float r, float g, float b, float a);

    /// @brief Sample the image at texture coordinates (u, v) using bilinear
    ///        interpolation.
    ///
    /// Bilinear sampling blends the four nearest pixels, producing a smooth
    /// result when texture coordinates fall between pixel centers.
    ///
    /// @param u Horizontal texture coordinate (0.0–1.0, 0 = left).
    /// @param v Vertical texture coordinate   (0.0–1.0, 0 = bottom).
    /// @return The interpolated Color.
    ///
    /// @par Example
    /// @code{.cpp}
    /// Color c = img.sample_bilinear(0.5f, 0.5f);  // center of texture
    /// @endcode
    Color sample_bilinear(float u, float v) const;

    /// @brief Downsample the image by a factor using box filtering.
    ///
    /// Reduces the image size.  Each output pixel is the average of a
    /// `factor × factor` block of input pixels.  The width and height
    /// must be divisible by `factor`.
    ///
    /// @param factor Downsampling factor (e.g., 2 halves both dimensions).
    /// @return A new, smaller Image.
    ///
    /// @par Example
    /// @code{.cpp}
    /// Image small = big.downsample_box(4);  // ¼ width, ¼ height
    /// @endcode
    Image downsample_box(int factor) const;

    /// @brief Crop the image to a sub-rectangle.
    ///
    /// The cropped region replaces the image in-place.
    ///
    /// @param x Left edge of the crop region.
    /// @param y Bottom edge of the crop region.
    /// @param w Width of the crop region.
    /// @param h Height of the crop region.
    ///
    /// @par Example
    /// @code{.cpp}
    /// img.crop(100, 50, 400, 300);  // keep a 400×300 region
    /// @endcode
    void crop(int x, int y, int w, int h);

    /// @brief Merge (concatenate) another image onto this one.
    ///
    /// The two images must have compatible dimensions for the merge direction.
    /// For LEFT/RIGHT merges, both images must have the same height.
    /// For TOP/BOTTOM merges, both images must have the same width.
    ///
    /// @param other     The image to attach.
    /// @param direction Which side to attach to.
    ///
    /// @par Example
    /// @code{.cpp}
    /// Image left(400, 300), right(400, 300);
    /// left.merge(right, MergeDirection::RIGHT);
    /// // left is now 800×300
    /// @endcode
    ///
    /// @see MergeDirection
    void merge(const Image &other, MergeDirection direction);

    /// @brief Grow (pad) the image by adding borders.
    ///
    /// Adds `top`, `bottom`, `left`, `right` rows/columns filled with
    /// the given background color.
    ///
    /// @param top        Pixels to add above the image.
    /// @param bottom     Pixels to add below the image.
    /// @param left       Pixels to add left of the image.
    /// @param right      Pixels to add right of the image.
    /// @param background Fill color for the new border.
    ///
    /// @par Example
    /// @code{.cpp}
    /// img.grow(10, 10, 20, 20, Color(1,1,1));  // add 10px top/bottom, 20px left/right
    /// @endcode
    void grow(int top, int bottom, int left, int right, const Color &background);

    /// @brief Rotate the image by 90 degrees in-place.
    ///
    /// @param clockwise If `true` (default), rotate clockwise.  Otherwise,
    ///                  counter-clockwise.
    ///
    /// @par Example
    /// @code{.cpp}
    /// img.rotate_90();          // clockwise
    /// img.rotate_90(false);     // counter-clockwise
    /// @endcode
    void rotate_90(bool clockwise = true);

    /// @brief Scale (resize) the image to new dimensions in-place.
    ///
    /// Uses bilinear interpolation for smooth results.
    ///
    /// @param new_width  Target width in pixels.
    /// @param new_height Target height in pixels.
    ///
    /// @par Example
    /// @code{.cpp}
    /// img.scale(1600, 1200);  // upscale to 1600×1200
    /// @endcode
    void scale(int new_width, int new_height);

    /// @brief Crop away uniform borders of a given background color.
    ///
    /// Removes rows/columns from the specified edges as long as every pixel
    /// in that row/column matches the background color (within tolerance).
    ///
    /// @param direction  Which edge(s) to crop.
    /// @param background The color to treat as "empty" border.
    ///
    /// @par Example
    /// @code{.cpp}
    /// img.crop_to_content(CropContentDirection::ALL, TRANSPARENT_BLACK);
    /// // removes all transparent borders from all edges
    /// @endcode
    ///
    /// @see CropContentDirection
    void crop_to_content(CropContentDirection direction, const Color &background);

    /// @brief Apply a contrast adjustment to the image.
    ///
    /// A value of 1.0 means no change.  Values > 1.0 increase contrast;
    /// values < 1.0 reduce it (0.0 = uniform gray).
    ///
    /// @param contrast Contrast multiplier (1.0 = unchanged).
    ///
    /// @par Example
    /// @code{.cpp}
    /// img.apply_contrast(1.5f);  // increase contrast
    /// @endcode
    void apply_contrast(float contrast);

    // ------------------------------------------------------------------
    //  File output
    // ------------------------------------------------------------------

    /// @brief Write the image as a PPM file (Portable Pixmap).
    ///
    /// PPM is a simple, uncompressed format useful for debugging and
    /// testing.  No external libraries are needed.
    ///
    /// @param filename Output file path (should end with `.ppm`).
    /// @return `true` on success.
    ///
    /// @see write_png(), write_bmp()
    bool write_ppm(const std::string &filename) const;

    /// @brief Write the image as a BMP file (Windows Bitmap).
    ///
    /// BMP is a simple, uncompressed format.  No external libraries needed.
    ///
    /// @param filename Output file path (should end with `.bmp`).
    /// @return `true` on success.
    ///
    /// @see write_png(), write_ppm()
    bool write_bmp(const std::string &filename) const;

    /// @brief Write the image as a TGA file (Truevision Targa).
    ///
    /// Writes an uncompressed true-color TGA file (image type 2) using
    /// scimesh's own implementation (no external libraries).  The header
    /// declares a top-left origin, matching the pixel layout produced by
    /// the renderer.  Pixel data is stored BGR(A) per the TGA spec, so
    /// red and blue channels are swapped on write.
    ///
    /// @param filename Output file path (should end with `.tga`).
    /// @param use24bit If `true`, write 24-bit RGB (no alpha channel).
    ///                 Default `false` writes 32-bit RGBA.
    /// @return `true` on success.
    ///
    /// @see write_png(), write_bmp()
    bool write_tga(const std::string &filename, bool use24bit = false) const;

    /// @brief Write the image as a PNG file.
    ///
    /// PNG is the recommended output format for publication-quality results.
    /// Uses stb_image_write internally.
    ///
    /// @param filename Output file path (should end with `.png`).
    /// @return `true` on success.
    ///
    /// @par Example
    /// @code{.cpp}
    /// Image result = renderer.render_mesh(mesh, camera, opts);
    /// result.write_png("rendering.png");
    /// @endcode
    ///
    /// @see write_ppm(), write_bmp()
    bool write_png(const std::string &filename) const;

    // ------------------------------------------------------------------
    //  File input (stb_image)
    // ------------------------------------------------------------------

    /// @brief Read an image from a file (PNG, BMP, TGA, JPEG, etc.).
    ///
    /// Uses stb_image internally, which auto-detects the format from the
    /// file header.  Returns an empty (0×0) image on failure.
    ///
    /// @param path Path to the image file.
    /// @return The loaded Image, or empty Image on failure.
    ///
    /// @par Example
    /// @code{.cpp}
    /// Image img = Image::read_image("colorbar.png");
    /// if (img.width == 0) { // handle error }
    /// @endcode
    static Image read_image(const std::string &path);

    // ------------------------------------------------------------------
    //  Size normalization
    // ------------------------------------------------------------------

    /// @brief Pad the image to a target size, centering the content.
    ///
    /// Adds borders filled with `background` so the image reaches
    /// `target_w × target_h`.  Content stays pixel-perfect — no scaling.
    /// If the image is already at or larger than the target size, this
    /// is a no-op.
    ///
    /// @param target_w   Desired width in pixels.
    /// @param target_h   Desired height in pixels.
    /// @param background Fill color for the added borders.
    ///
    /// @par Example
    /// @code{.cpp}
    /// Image small(400, 300);
    /// small.pad_to_size(800, 600, Color(1,1,1));
    /// // small is now 800×600, original content centered
    /// @endcode
    ///
    /// @see grow(), scale()
    void pad_to_size(int target_w, int target_h, const Color &background);
};

// ---------------------------------------------------------------------------
//  Free functions
// ---------------------------------------------------------------------------

/// @brief Arrange a list of images into a grid layout.
///
/// Images are placed left-to-right, top-to-bottom in an `ncol × nrow` grid.
/// If the number of images is less than `ncol * nrow`, remaining cells are
/// filled with `background`.  Before placement, all images are normalized
/// to the same cell size using `fit_mode`.
///
/// @param images     The list of images to arrange.
/// @param ncol       Number of columns (0 = auto-compute from nrow).
/// @param nrow       Number of rows (0 = auto-compute from ncol).
///                   If both are 0, a square-ish layout is chosen.
/// @param fit_mode   How to handle size mismatches (PAD or SCALE).
/// @param background Fill color for padding and empty cells.
/// @return A new Image containing the arranged grid.
///
/// @par Example
/// @code{.cpp}
/// Image grid = grid_arrange({view1, view2, view3, view4}, 2, 2,
///                            FitMode::PAD, Color(1,1,1));
/// @endcode
///
/// @see FitMode, Image::pad_to_size(), Image::scale()
Image grid_arrange(const std::vector<Image> &images,
                   int ncol = 0, int nrow = 0,
                   FitMode fit_mode = FitMode::PAD,
                   const Color &background = Color(1.0f, 1.0f, 1.0f, 1.0f));

} // namespace scimesh
