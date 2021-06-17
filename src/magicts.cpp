#include <stdlib.h> // NULL, malloc, free

struct TouchData
{
};

struct TouchscreenContext
{
};

extern "C" void *
magicts_initialize()
{
	TouchscreenContext *ctx = (TouchscreenContext *)malloc(sizeof(TouchscreenContext));

	if(ctx)
	{
	}

	return ctx;
}

extern "C" TouchData
magicts_update(void *ctxPtr)
{
	TouchData touches = {};

	return touches;
}

extern "C" void
magicts_finalize(void *ctxPtr)
{
	free(ctxPtr);
}

