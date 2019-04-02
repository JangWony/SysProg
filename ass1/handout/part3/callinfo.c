#include<stdlib.h>
#include<string.h>
#define UNW_LOCAL_ONLY
#include<libunwind.h>

int get_callinfo(char *fname, size_t fnlen, unsigned long long *ofs){

        unw_context_t context;
        unw_cursor_t cursor;
        unw_word_t off, ip, sp;
        unw_proc_info_t pip;
        char procname[256];
        int ret;
        
        if (unw_getcontext(&context))
                return -1;
 
        if (unw_init_local(&cursor, &context))
                return -1;
        
        for(int i=0;i<3;i++)
                unw_step(&cursor);
        unw_get_proc_info(&cursor, &pip);
 
        ret = unw_get_proc_name(&cursor, procname, 256, &off);
        if (ret && ret != -UNW_ENOMEM) {
            procname[0] = '?';
            procname[1] = 0;
        }
 
        unw_get_reg(&cursor, UNW_REG_IP, &ip);
        unw_get_reg(&cursor, UNW_REG_SP, &sp);
 
        strcpy(fname,procname);
        *ofs = (long)off-5;
        return 1;
}
