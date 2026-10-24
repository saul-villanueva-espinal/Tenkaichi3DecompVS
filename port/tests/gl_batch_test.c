#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/gs/gs_gl_batch.h"

static Vu0Uniform uniforms[3];
static GsDraw triangle(unsigned first) {
    GsDraw d={0};d.count=3;d.first=first;d.vu=1;d.target=2;d.tex=7;
    d.pipeline=3;d.scissor[2]=1536;d.scissor[3]=1344;
    return d;
}

int main(void) {
    GsDraw a=triangle(0),b=triangle(3),c;
    unsigned i,merged=0,submitted=0,vertices=0;
    uniforms[2].boneA[5]=1.0f;
    assert(GsGl_CanBatch(&a,&b,uniforms));
    b.uniform=1;assert(GsGl_CanBatch(&a,&b,uniforms));
    b.uniform=2;assert(!GsGl_CanBatch(&a,&b,uniforms));
    b=triangle(3);
#define BARRIER(field,value) c=b;c.field=value;assert(!GsGl_CanBatch(&a,&c,uniforms))
    BARRIER(native,1);BARRIER(native,4);BARRIER(native,5);BARRIER(native,100);
    BARRIER(target,9);BARRIER(tex,8);BARRIER(sampler,5);BARRIER(pipeline,4);
    BARRIER(vu,2);BARRIER(src,1);BARRIER(tex_is_target,1);BARRIER(wall,1);
    BARRIER(mode[1],7);BARRIER(misc[1],.5f);BARRIER(rect[1],.2f);
    BARRIER(orig[1],.7f);BARRIER(blendc,.5f);BARRIER(scissor[2],512);
    BARRIER(first,6);BARRIER(count,0);BARRIER(count,4);BARRIER(count,UINT32_MAX);
#undef BARRIER
    a.tex_is_target=b.tex_is_target=1;assert(!GsGl_CanBatch(&a,&b,uniforms));
    a=triangle(0);b=triangle(3);
    a.count=0;assert(!GsGl_CanBatch(&a,&b,uniforms));
    a=triangle(UINT32_MAX-2);b=triangle(0);assert(!GsGl_CanBatch(&a,&b,uniforms));

    /* Compare the original vertex stream with the batched one over a long
       command list. Include repeated bones, material changes and copy barriers. */
    GsDraw draws[4096];
    for(i=0;i<4096;i++) {
        draws[i]=triangle(i*3);
        draws[i].tex=7+(i/8)%3;
        draws[i].uniform=i%2;
        if(i%127==0)draws[i].native=5;
        if(i%31==0)draws[i].scissor[0]=1;
    }
    for(i=0;i<4096;i++) {
        GsDraw batch=draws[i];unsigned begin=i,k;
        while(i+1<4096 && GsGl_CanBatch(&batch,&draws[i+1],uniforms)) {
            batch.count+=draws[++i].count;merged++;
        }
        for(k=begin;k<=i;k++) {
            /* No skipped triangle, changed order, or barrier in a batch. */
            assert(draws[k].first==batch.first+(k-begin)*3);
            assert(k==begin || (!draws[k].native && !batch.native));
            assert(draws[k].tex==batch.tex && draws[k].target==batch.target);
            assert(!memcmp(&uniforms[draws[k].uniform],&uniforms[batch.uniform],sizeof(Vu0Uniform)));
        }
        assert(batch.count==(i-begin+1)*3);
        vertices+=batch.count;submitted++;
    }
    assert(vertices==4096*3 && submitted+merged==4096 && submitted<1024);
    printf("gl_batch_test: identical ordered triangles; all material/effect/target barriers; %u -> %u submissions PASS\n",4096u,submitted);
    return 0;
}
