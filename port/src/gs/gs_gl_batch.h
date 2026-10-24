#ifndef GS_GL_BATCH_H
#define GS_GL_BATCH_H
#include <limits.h>
#include <string.h>
#include "gs_draw.h"

/* Only adjacent, complete triangles with identical rendering state. Preserve
   index/vertex order and never cross copies, outlines, DATE or target barriers.
   The shaders do not use gl_PrimitiveID or draw IDs. */
static int GsGl_CanBatch(const GsDraw *a, const GsDraw *b, const Vu0Uniform *uniforms) {
    if(a->native || b->native || a->tex_is_target || b->tex_is_target || !a->count || !b->count || a->count%3 || b->count%3 ||
       (uint64_t)a->first+a->count!=b->first || b->count>(uint32_t)INT_MAX || a->count>(uint32_t)INT_MAX-b->count ||
       a->vu!=b->vu || a->target!=b->target || a->pipeline!=b->pipeline ||
       a->tex!=b->tex || a->sampler!=b->sampler || a->src!=b->src ||
       a->tex_is_target!=b->tex_is_target || a->wall!=b->wall ||
       memcmp(a->mode,b->mode,sizeof(a->mode)) || memcmp(a->misc,b->misc,sizeof(a->misc)) ||
       memcmp(a->rect,b->rect,sizeof(a->rect)) || memcmp(a->orig,b->orig,sizeof(a->orig)) ||
       memcmp(&a->blendc,&b->blendc,sizeof(a->blendc)) ||
       memcmp(a->scissor,b->scissor,sizeof(a->scissor))) return 0;
    return !a->vu || a->uniform==b->uniform ||
           !memcmp(&uniforms[a->uniform],&uniforms[b->uniform],sizeof(*uniforms));
}
#endif
