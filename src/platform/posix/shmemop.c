#include <stdint.h>
#include "arcan_shmif.h"

static inline void* alignv(uint8_t* inptr, size_t align_sz)
{
	return (void*) (((uintptr_t)inptr % align_sz != 0) ?
		inptr + align_sz - ((uintptr_t) inptr % align_sz) : inptr);
}

uintptr_t arcan_shmif_mapav(
	struct arcan_shmif_page* addr,
	shmif_pixel* vbuf[], size_t vbufc, size_t vbuf_sz,
	shmif_asample* abuf[], size_t abufc, size_t abuf_sz)
{
/* now we are in bat county */
	uint8_t* wbuf = (uint8_t*)addr + sizeof(struct arcan_shmif_page);
	if (addr && vbuf)
		wbuf += addr->apad;

	for (size_t i = 0; i < abufc; i++){
			wbuf = alignv(wbuf, ARCAN_SHMPAGE_ALIGN);
			if (abuf)
				abuf[i] = abuf_sz ? (shmif_asample*) wbuf : NULL;
			wbuf += abuf_sz;
	}

	for (size_t i = 0; i < vbufc; i++){
			wbuf = alignv(wbuf, ARCAN_SHMPAGE_ALIGN);
			if (vbuf){
				vbuf[i] = vbuf_sz ?
					(shmif_pixel*) alignv(wbuf, ARCAN_SHMPAGE_ALIGN) : NULL;
			}
			wbuf += vbuf_sz;
		}

#ifdef ARCAN_SHMIF_OVERCOMMIT
	return ARCAN_SHMPAGE_MAX_SZ;
#else
	return (uintptr_t) wbuf - (uintptr_t) addr;
#endif
}
