#pragma once

#include <string>
#include <cstdint>
#include <algorithm>
#include <vector>
#include <functional>

#include "libraries/hershey_fonts/hershey_fonts.hpp"
#include "libraries/bitmap_fonts/bitmap_fonts.hpp"
#include "libraries/bitmap_fonts/font6_data.hpp"
#include "libraries/bitmap_fonts/font8_data.hpp"
#include "libraries/bitmap_fonts/font14_outline_data.hpp"

#include "common/pimoroni_common.hpp"

// A tiny graphics library for our Pico products
// supports:
//   - 16-bit (565) RGB
//   - 8-bit (332) RGB
//   - 8-bit with 16-bit 256 entry palette
//   - 4-bit with 16-bit 8 entry palette
namespace pimoroni {
  typedef uint8_t RGB332;
  typedef uint16_t RGB565;
  struct RGB {
    uint8_t r, g, b;

    constexpr RGB() : r(0), g(0), b(0) {}
    constexpr RGB(RGB332 c) :
      r((c & 0b11100000) >> 0),
      g((c & 0b00011100) << 3),
      b((c & 0b00000011) << 6) {}
    constexpr RGB(RGB565 c) :
      r((__builtin_bswap16(c) & 0b1111100000000000) >> 8),
      g((__builtin_bswap16(c) & 0b0000011111100000) >> 3),
      b((__builtin_bswap16(c) & 0b0000000000011111) << 3) {}
    constexpr RGB(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}

    // a rough approximation of how bright a colour is used to compare the
    // relative brightness of two colours
    int luminance() {
      // weights based on https://www.johndcook.com/blog/2009/08/24/algorithms-convert-color-grayscale/
      return r * 21 + g * 72 * b * 7;
    }

    // a relatively low cost approximation of how "different" two colours are
    // perceived which avoids expensive colour space conversions.
    // described in detail at https://www.compuphase.com/cmetric.htm
    int distance(RGB c) {
      int rmean = ((int)r + c.r) / 2;
      int rx = (int)r - c.r, gx = (int)g - c.g, bx = (int)b - c.b;
      return abs((int)(
        (((512 + rmean) * rx * rx) >> 8) + 4 * gx * gx + (((767 - rmean) * bx * bx) >> 8)
      ));
    }

    int closest(const RGB *palette, size_t len) {
      int d = INT_MAX, m = -1;
      for(size_t i = 0; i < len; i++) {
        int dc = distance(palette[i]);
        if(dc < d) {m = i; d = dc;}
      }
      return m;
    }

    constexpr RGB565 to_rgb565() {
      uint16_t p = ((r & 0b11111000) << 8) |
                   ((g & 0b11111100) << 3) |
                   ((b & 0b11111000) >> 3);

      return __builtin_bswap16(p);
    }

    constexpr RGB565 to_rgb332() {
      return (r & 0b11100000) | ((g & 0b11100000) >> 3) | ((b & 0b11000000) >> 6);
    }
  };

  typedef int Pen;

  struct Rect;

  struct Point {
    int32_t x = 0, y = 0;

    Point() = default;
    Point(int32_t x, int32_t y) : x(x), y(y) {}

    inline Point& operator-= (const Point &a) { x -= a.x; y -= a.y; return *this; }
    inline Point& operator+= (const Point &a) { x += a.x; y += a.y; return *this; }

    Point clamp(const Rect &r) const;
  };

  struct Rect {
    int32_t x = 0, y = 0, w = 0, h = 0;

    Rect() = default;
    Rect(int32_t x, int32_t y, int32_t w, int32_t h) : x(x), y(y), w(w), h(h) {}
    Rect(const Point &tl, const Point &br) : x(tl.x), y(tl.y), w(br.x - tl.x), h(br.y - tl.y) {}

    bool empty() const;
    bool contains(const Point &p) const;
    bool contains(const Rect &p) const;
    bool intersects(const Rect &r) const;
    Rect intersection(const Rect &r) const;

    void inflate(int32_t v);
    void deflate(int32_t v);
  };

  class PicoGraphics {
  public:
    enum PenType {
      PEN_P2 = 0,
      PEN_P4,
      PEN_P8,
      PEN_RGB332,
      PEN_RGB565
    };

    void *frame_buffer;

    PenType pen_type;
    Rect bounds;
    Rect clip;

    typedef std::function<void(void *data, size_t length)> conversion_callback_func;

    const bitmap::font_t *bitmap_font;
    const hershey::font_t *hershey_font;

    static constexpr RGB332 rgb_to_rgb332(uint8_t r, uint8_t g, uint8_t b) {
      return RGB(r, g, b).to_rgb332();
    }

    static constexpr RGB565 rgb332_to_rgb565(RGB332 c) {
      uint16_t p = ((c & 0b11100000) << 8) |
                   ((c & 0b00011100) << 6) |
                   ((c & 0b00000011) << 3);
      return __builtin_bswap16(p);
    }

    static constexpr RGB565 rgb565_to_rgb332(RGB565 c) {
      c = __builtin_bswap16(c);
      return ((c & 0b1110000000000000) >> 8) |
             ((c & 0b0000011100000000) >> 6) |
             ((c & 0b0000000000011000) >> 3);
    }

    static constexpr RGB565 rgb_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
      return RGB(r, g, b).to_rgb565();
    }

    static constexpr RGB rgb332_to_rgb(RGB332 c) {
      return RGB((RGB332)c);
    };

    static constexpr RGB rgb565_to_rgb(RGB565 c) {
      return RGB((RGB565)c);
    };

    PicoGraphics(uint16_t width, uint16_t height, void *frame_buffer)
    : frame_buffer(frame_buffer), bounds(0, 0, width, height), clip(0, 0, width, height) {
      set_font(&font6);
    };

    virtual void set_pen(uint c);
    virtual void set_pen(uint8_t r, uint8_t g, uint8_t b);
    virtual int update_pen(uint8_t i, uint8_t r, uint8_t g, uint8_t b);
    virtual int reset_pen(uint8_t i);
    virtual int create_pen(uint8_t r, uint8_t g, uint8_t b);
    virtual void set_pixel(const Point &p);
    virtual void scanline_convert(PenType type, conversion_callback_func callback);

    void set_font(const bitmap::font_t *font);
    void set_font(const hershey::font_t *font);
    void set_font(std::string font);

    void set_dimensions(int width, int height);
    void set_framebuffer(void *frame_buffer);

    void *get_data();
    void get_data(PenType type, uint y, void *row_buf);

    void set_clip(const Rect &r);
    void remove_clip();

    void clear();
    void pixel(const Point &p);
    void pixel_span(const Point &p, int32_t l);
    void rectangle(const Rect &r);
    void circle(const Point &p, int32_t r);
    void character(const char c, const Point &p, float s = 2.0f, float a = 0.0f);
    void text(const std::string &t, const Point &p, int32_t wrap, float s = 2.0f, float a = 0.0f, uint8_t letter_spacing = 1);
    int32_t measure_text(const std::string &t, float s = 2.0f, uint8_t letter_spacing = 1);
    void polygon(const std::vector<Point> &points);
    void triangle(Point p1, Point p2, Point p3);
    void line(Point p1, Point p2);
  };


  class PicoGraphics_PenP4 : public PicoGraphics {
    public:
      uint8_t color;
      RGB palette[16];

      PicoGraphics_PenP4(uint16_t width, uint16_t height, void *frame_buffer)
      : PicoGraphics(width, height, frame_buffer) {
        this->pen_type = PEN_P4;
        if(this->frame_buffer == nullptr) {
          this->frame_buffer = (void *)(new uint8_t[buffer_size(width, height)]);
        }
        for(auto i = 0u; i < 16; i++) {
          palette[i] = {
            uint8_t(i << 4),
            uint8_t(i << 4),
            uint8_t(i << 4)
          };
        }
      }
      void set_pen(uint c) {
        color = c & 0xf;
      }
      void set_pen(uint8_t r, uint8_t g, uint8_t b) override {
        int pen = RGB(r, g, b).closest(palette, 16);
        if(pen != -1) color = pen;
      }
      int update_pen(uint8_t i, uint8_t r, uint8_t g, uint8_t b) override {
        i &= 0xf;
        palette[i] = {r, g, b};
        return i;
      }
      void set_pixel(const Point &p) override {
        // pointer to byte in framebuffer that contains this pixel
        uint8_t *buf = (uint8_t *)frame_buffer;
        uint8_t *f = &buf[(p.x / 2) + (p.y * bounds.w / 2)];

        uint8_t  o = (~p.x & 0b1) * 4; // bit offset within byte
        uint8_t  m = ~(0b1111 << o);   // bit mask for byte
        uint8_t  b = color << o;       // bit value shifted to position

        *f &= m; // clear bits
        *f |= b; // set value
      }
      void scanline_convert(PenType type, conversion_callback_func callback) override {
        if(type == PEN_RGB565) {
          // Cache the RGB888 palette as RGB565
          RGB565 cache[16];
          for(auto i = 0u; i < 16; i++) {
            cache[i] = palette[i].to_rgb565();
          }

          // Treat our void* frame_buffer as uint8_t
          uint8_t *src = (uint8_t *)frame_buffer;

          // Allocate a per-row temporary buffer
          uint16_t row_buf[bounds.w];
          for(auto y = 0; y < bounds.h; y++) {
            for(auto x = 0; x < bounds.w; x++) {
              uint8_t c = src[(bounds.w * y / 2) + (x / 2)];
              uint8_t  o = (~x & 0b1) * 4; // bit offset within byte
              uint8_t  b = (c >> o) & 0xf; // bit value shifted to position
              row_buf[x] = cache[b];
            }
            // Callback to the driver with the row data
            callback(row_buf, bounds.w * sizeof(RGB565));
          }
        }
      }
      static size_t buffer_size(uint w, uint h) {
        return w * h / 2;
      }
  };

  class PicoGraphics_PenP8 : public PicoGraphics {
    public:
      uint8_t color;
      RGB palette[256];
      bool used[256];
      PicoGraphics_PenP8(uint16_t width, uint16_t height, void *frame_buffer)
      : PicoGraphics(width, height, frame_buffer) {
        this->pen_type = PEN_P8;
        if(this->frame_buffer == nullptr) {
          this->frame_buffer = (void *)(new uint8_t[buffer_size(width, height)]);
        }
        for(auto i = 0u; i < 256; i++) {
          palette[i] = {0, 0, 0};
          used[i] = false;
        }
      }
      void set_pen(uint c) override {
        color = c;
      }
      void set_pen(uint8_t r, uint8_t g, uint8_t b) override {
        int pen = RGB(r, g, b).closest(palette, 16);
        if(pen != -1) color = pen;
      }
      int update_pen(uint8_t i, uint8_t r, uint8_t g, uint8_t b) override {
        i &= 0xff;
        palette[i] = {r, g, b};
        return i;
      }
      int create_pen(uint8_t r, uint8_t g, uint8_t b) override {
        // Create a colour and place it in the palette if there's space
        for(auto i = 0u; i < 256u; i++) {
          if(!used[i]) {
            palette[i] = {r, g, b};
            used[i] = true;
            return i;
          }
        }
        return -1;
      }
      int reset_pen(uint8_t i) {
        palette[i] = {0, 0, 0};
        used[i] = false;
        return i;
      }
      void set_pixel(const Point &p) override {
        uint8_t *buf = (uint8_t *)frame_buffer;
        buf[p.y * bounds.w + p.x] = color;
      }
      void scanline_convert(PenType type, conversion_callback_func callback) override {
        if(type == PEN_RGB565) {
          // Cache the RGB888 palette as RGB565
          RGB565 cache[256];
          for(auto i = 0u; i < 256; i++) {
            cache[i] = palette[i].to_rgb565();
          }

          // Treat our void* frame_buffer as uint8_t
          uint8_t *src = (uint8_t *)frame_buffer;

          // Allocate a per-row temporary buffer
          uint16_t row_buf[bounds.w];
          for(auto y = 0; y < bounds.h; y++) {
            for(auto x = 0; x < bounds.w; x++) {
              row_buf[x] = cache[src[bounds.w * y + x]];
            }
            // Callback to the driver with the row data
            callback(row_buf, bounds.w * sizeof(RGB565));
          }
        }
      }
      static size_t buffer_size(uint w, uint h) {
        return w * h;
      }
  };

  class PicoGraphics_PenRGB332 : public PicoGraphics {
    public:
      RGB332 color;
      RGB palette[256];
      PicoGraphics_PenRGB332(uint16_t width, uint16_t height, void *frame_buffer)
      : PicoGraphics(width, height, frame_buffer) {
        this->pen_type = PEN_RGB332;
        if(this->frame_buffer == nullptr) {
          this->frame_buffer = (void *)(new uint8_t[buffer_size(width, height)]);
        }
        for(auto i = 0u; i < 256; i++) {
          palette[i] = RGB((RGB332)i);
        }
      }
      void set_pen(uint c) override {
        color = c;
      }
      void set_pen(uint8_t r, uint8_t g, uint8_t b) override {
        color = rgb_to_rgb332(r, g, b);
      }
      int create_pen(uint8_t r, uint8_t g, uint8_t b) override {
        return rgb_to_rgb332(r, g, b);
      }
      void set_pixel(const Point &p) override {
        uint8_t *buf = (uint8_t *)frame_buffer;
        buf[p.y * bounds.w + p.x] = color;
      }
      void scanline_convert(PenType type, conversion_callback_func callback) override {
        if(type == PEN_RGB565) {
          // Cache the RGB888 palette as RGB565
          RGB565 cache[256];
          for(auto i = 0u; i < 256; i++) {
            cache[i] = palette[i].to_rgb565();
          }

          // Treat our void* frame_buffer as uint8_t
          uint8_t *src = (uint8_t *)frame_buffer;

          // Allocate a per-row temporary buffer
          uint16_t row_buf[bounds.w];
          for(auto y = 0; y < bounds.h; y++) {
            for(auto x = 0; x < bounds.w; x++) {
              row_buf[x] = cache[src[bounds.w * y + x]];
            }
            // Callback to the driver with the row data
            callback(row_buf, bounds.w * sizeof(RGB565));
          }
        }
      }
      static size_t buffer_size(uint w, uint h) {
        return w * h;
      }
  };

  class PicoGraphics_PenRGB565 : public PicoGraphics {
    public:
      RGB src_color;
      RGB565 color;
      PicoGraphics_PenRGB565(uint16_t width, uint16_t height, void *frame_buffer)
      : PicoGraphics(width, height, frame_buffer) {
        this->pen_type = PEN_RGB565;
        if(this->frame_buffer == nullptr) {
          this->frame_buffer = (void *)(new uint8_t[buffer_size(width, height)]);
        }
      }
      void set_pen(uint c) override {
        color = c;
      }
      void set_pen(uint8_t r, uint8_t g, uint8_t b) override {
        src_color = {r, g, b};
        color = src_color.to_rgb565();
      }
      int create_pen(uint8_t r, uint8_t g, uint8_t b) override {
        return rgb_to_rgb565(r, g, b);
      }
      void set_pixel(const Point &p) override {
        uint16_t *buf = (uint16_t *)frame_buffer;
        buf[p.y * bounds.w + p.x] = color;
      }
      static size_t buffer_size(uint w, uint h) {
        return w * h * sizeof(RGB565);
      }
  };

  class DisplayDriver {
    public:
      uint16_t width;
      uint16_t height;
      Rotation rotation;

      DisplayDriver(uint16_t width, uint16_t height, Rotation rotation)
       : width(width), height(height), rotation(rotation) {};

      virtual void update(PicoGraphics *display) {};
      virtual void set_backlight(uint8_t brightness) {};
  };

}
