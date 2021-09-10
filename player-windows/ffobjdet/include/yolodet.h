#ifndef __YOLODET_H__
#define __YOLODET_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DEFINE_BBOX_TYPE
#define DEFINE_BBOX_TYPE
typedef struct {
    float score, x1, y1, x2, y2;
    int   category;
} BBOX;
#endif

void* yolodet_init  (char *paramfile, char *binfile);
void  yolodet_free  (void *ctxt);
int   yolodet_detect(void *ctxt, BBOX *bboxlist, int n, uint8_t *bitmap, int w, int h);
const char* yolodet_category2str(int category);

#ifdef __cplusplus
}
#endif

#endif
