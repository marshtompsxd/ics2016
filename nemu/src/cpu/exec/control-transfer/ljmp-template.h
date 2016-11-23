#include "cpu/exec/template-start.h"
#include "../../../../../lib-common/x86-inc/mmu.h"
#include "desc.h"
#define instr ljmp

/*
 * Direct far JMP. Direct JMP instructions that specify a target location
 * outside the current code segment contain a far pointer. This pointer
 * consists of a selector for the new code segment and an offset within the new
 * segment.
 */


make_helper(concat(ljmp_,SUFFIX)){
    if(DATA_BYTE==2){
        panic("please implment ljmp");
    }
    else{

        SegDesc desc;
        SegDescBase descbase;
        SegDescLimit desclimit;

        cpu.sreg[SR_CS].selector.SELECTOR=instr_fetch(eip+5,2);
        cpu.eip=instr_fetch(eip+1,4);

        uint32_t index=cpu.sreg[SR_CS].selector.INDEX;
        desc.content[0]=lnaddr_read(cpu.gdtr.base+index*8,4);
        desc.content[1]=lnaddr_read(cpu.gdtr.base+index*8+4,4);

        loadbase(&desc,&descbase);
        loadlimit(&desc,&desclimit);
        setsreg(desc, descbase, desclimit, SR_CS);

        print_asm("ljmp");
        return 0;
    }
}
#include "cpu/exec/template-end.h"