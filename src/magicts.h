#ifndef MAGICTS_H_
#define MAGICTS_H_

#define NUM_TOUCHES 100
#define MAX_TOUCHSCREENS 16
#define SCREEN_ID_LENGTH 32

struct TouchData
{
    char screen[NUM_TOUCHES][SCREEN_ID_LENGTH];
    int id[NUM_TOUCHES];
    float x[NUM_TOUCHES];
    float y[NUM_TOUCHES];
};

struct MagicTouchScreenScreenIDList
{
    int count;
    char ids[MAX_TOUCHSCREENS * SCREEN_ID_LENGTH];
};

#ifdef __cplusplus
extern "C"
{
#endif
    /*
     * Initialize a touch screen, or all available touch screens
     * if `id` is null
     */
    void *magicts_initialize(const char *id);


    /*
     * Count the number of connected touch screens.
     * You can call this without calling magicts_initialize() first
     */
    int magicts_get_screencount(void);

    /*
     * Get a list of connected touch screens (as their IDs).
     * You can call this without calling magicts_initialize() first
     */
    struct MagicTouchScreenScreenIDList magicts_get_screenids(void);

    /*
     * Pump events for all initialized touch screens.
     */
    struct TouchData magicts_update(void *ctxPtr);

    /*
     * Deinitialize touch screens and free the resources used by this library.
     */
    void magicts_finalize(void *ctxPtr);
#ifdef __cplusplus
}
#endif
#endif /* MAGICTS_H_ */

