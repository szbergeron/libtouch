#include <cstdint>
/**
 * This library serves as an event interpretation library.
 * To use, you will need to take the raw events you recieve
 * on your platform and adapt it to a compatible input
 * interface. You are expected to call get_pan() once on every frame.
 * It expects an estimation of the next frametime as well as how
 * long until the current frame will be rendered. This allows overshoot
 * calculation to take place.
 */

/**
 * Example usage:
 *
 * 1. Create some `struct scrollview` locally and pass geometry
 *      and expected behavior as specified in struct
 *
 * 2. Pass said struct by value to create_scrollview(), storing
 *      the returned scrollview handle for future use
 *      in conjunction with the associated UI scrollview
 *
 * 3. Use set_predict() with estimations of average frametimes
 *      and how far into a frame period each get_pos/get_pan call
 *      will occur
 *
 * 4. In event loop, recieve and pass any scroll events through
 *      add_scroll(), add_scroll_interrupt(), add_scroll_release()
 *      and related event signaling functions. Strict ordering
 *      or summation are not required here, just pass info as
 *      it comes in from the device
 *
 * 5. On each render loop iteration, use get_pan_[x/y]() or
 *      get_pos_[x/y]() to find where to transform the content to
 *      under the viewport, no intermediate processing required
 *
 * 6. Call destroy_scrollview(), passing the scrollview handle
 *      from earlier to clean up scrollview on exit
 */

/**
 * Any of these options can be logical or'ed together
 * and passed to set_options
 */
enum options {
    IMPRECISE_SCROLLS_SMOOTHLY = 0x1, // controls whether large jumps from imprecise devices (keyboard, clickwheel) should animate smoothly
};

struct pan_transform {
    int64_t x; // x axis pan amount in dp
    int64_t y; // y axis pan amount in dp
    bool panned : 1; // only true if a pan event has occurred.
                 // A transform can be skipped if this is false, otherwise assume
                 // that a pan has occurred and do a transform of the viewport by
                 // x and y
                 //
                 // also indicates that no further pan or state change
                 // will occur without adding another event to
                 // the queue, so any render loop can block safely

    double velocity_x; // gives current x axis velocity in dp, can be used for overscroll behavior
    double velocity_y; // gives y axis velocity
};

struct scrollview {
    uint64_t content_height; // height of scrollview content space in dp
    uint64_t content_width; // width of scrollview content space in dp
    uint64_t viewport_height; // height of viewport in dp
    uint64_t viewport_width; // width of viewport in dp

    // x and y axis offset of top-left corner of viewport relative to top-left corner of content
    // equivalent to using force_pan() after setting geometry
    int64_t viewport_initial_x;
    int64_t viewport_initial_y;

    // whether to enable overscroll bounce behavior on each edge
    bool bounce_bottom: 1;
    bool bounce_top: 1;
    bool bounce_left: 1;
    bool bounce_right: 1;

    //TODO: determine if additional "soft boundaries" would be beneficial for overscroll purposes
    //TODO: allow setting "magnetic" points in a scrollview


    // NOTE: any variables past this point should be considered opaque and implementation defined

    void* state; // handle used for internally tracking state of a given scrollview by libtouch
};

/**
 * Do initialization tasks for and return a handle to a new scrollview
 *
 * Default geometry will be used for this variant, and can be updated
 * with signal_geometry() on a modified scrollview
 */
struct scrollview* create_scrollview();

/**
 * Create a new scrollview and simultaneously default
 * initialize the geometry of the scrollview to that
 * of the passed scrollview
 */
struct scrollview* create_scrollview(struct scrollview view);

/**
 * Tears down and frees the referenced scrollview
 *
 * The handle passed here should be considered invalid
 * after this function has been called
 */
void destroy_scrollview(scrollview* view);

/**
 * Signals that geometry for the referenced scrollview
 * has changed and should be updated
 */
void signal_geometry(struct scrollview* view);

/**
 * Allows forcing a relative scroll by x, y dp in the current scrollview
 *
 * Example use case: user uses a keyboard shortcut to jump down by a page
 */
void force_pan(int64_t x_dp, int64_t y_dp);

/**
 * Allows forcing a scroll to position x, y dp in the current scrollview
 *
 * Example use case: user jumps to an absolute line number in a text editor
 */
void force_jump(int64_t x_dp, int64_t y_dp);

/**
 * Gets a pan event detailing how to transform
 * the current viewport
 */
struct pan_transform get_pan();

/**
 * Sets how long the average frame is as well as how far
 * in the future to predict a pan. This allows us to slightly
 * overshoot any pan to minimize percieved lag
 */
void set_predict(float ms_to_vsync, float ms_avg_frametime);

/**
 * Shorthand for calling set_predict followed by get_pan
 * Use this if frametimes or render latency
 * are highly variable to minimize jank or stutter.
 */
struct pan_transform get_pan_predict(float ms_to_vsync, float ms_avg_frametime);

int64_t get_pan_x(); // gets x component of current pan. WARN: mutates internal state: clears x axis event buffer
int64_t get_pan_y(); // gets y component of current pan. WARN: mutates internal state: clears y axis event buffer

int64_t get_pos_x(); // gets absolute x position of current viewport into content
int64_t get_pos_y(); // gets absolute y position of current viewport into content

/**
 * set_input_source should be properly used always, since
 * if input is assumed to be a touchpad and turns out to
 * be a touchscreen, an acceleration curve will be applied
 * which will desynchronize touch point and panning
 *
 * scroll_natural also only applies to 
 */

void set_scale_factor(float x_factor, float y_factor); // normalization factor for quirky devices

enum input_source_t {
    UNDEFINED, // acts identically to PASSTHROUGH_KINETIC,
               // only use when no hint is available as
               // to what input source is
    TOUCHSCREEN,
    TOUCHPAD,
    MOUSEWHEEL,
    MOUSEWHEEL_PRECISE,
    PASSTHROUGH, // use for inputs that have their own drivers
                 // handling any acceleration curves
                 // or overshoot; disables any
                 // input processing here, only sum pan distance
                 // Examples: TrackPoint, trackball, mousekeys
    PASSTHROUGH_KINETIC, // use as prior, but keep
                         // kinetic scrolling after scroll_release event
};

/**
 * can be called at any time between calls to get_pan_*
 * and is indipotent
 *
 * output of any get_pan_* call is interpreted
 * through the lens of the most recently set source
 */
void set_input_source(enum input_source_t input_source);

int add_scroll(
    int64_t motion_x, // x axis motion reported by device
    int64_t motion_y // y axis motion reported by device
);

/**
 * similar to add_scroll(motion_x, motion_y),
 * use for when input device delivers axes
 * as separate events
 */
int add_scroll_x(int64_t motion_x);
int add_scroll_y(int64_t motion_y);

/**
 * analogous to "was scrolling kinetically,
 * until user put two fingers on touchpad"
 */
void add_scroll_interrupt();

/**
 * triggers kinetic scrolling,
 * last event to be sent during
 * a "flick" action
 */
void add_scroll_release();
