#include "cpu/exec/helper.h"


#define DATA_BYTE 2
#include "movzx-template.h"
#undef DATA_BYTE

#define DATA_BYTE 4
#include "movzx-template.h"
#undef DATA_BYTE

make_helper_v(movzx_mzbrm2r)

make_helper(movzx_mzwrm2r_v){
	return movzx_mzwrm2r_l(eip);
}
