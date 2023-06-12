#ifndef MAGICTS_H_
#define MAGICTS_H_

#define NUM_TOUCHES 100

struct TouchData
{
    char screen[NUM_TOUCHES][32];
    int id[NUM_TOUCHES];
    float x[NUM_TOUCHES];
    float y[NUM_TOUCHES];
};

#ifdef __cplusplus
extern "C"
{
#endif
    void *magicts_initialize(void);
    int magicts_get_screencount(void *ctxPtr);
    char **magicts_get_screenids(void *ctxPtr);
    struct TouchData magicts_update(void *ctxPtr);
    void magicts_finalize(void *ctxPtr);
#ifdef __cplusplus
}
#endif
#endif /* MAGICTS_H_ */

