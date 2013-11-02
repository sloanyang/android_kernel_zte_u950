/********************************************************************
added by pengtao for zte ext functions
***********************************************************************/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include "board.h"
#include <asm/uaccess.h>


static int sdw_fmstat=0xFF;
extern unsigned int g_zte_board_id;

int zte_get_board_id(void);


static ssize_t zte_read_hver(char *page, char **start, off_t off,
		int count, int *eof, void *data)
{
	int board_id = -1;
	int len = 0;	
	
    board_id = zte_get_board_id();
printk("[zte_read_hver]CXG get board_id=%d\n",board_id);
    switch (board_id)
    {
        case 0:
             len = 5;
             
            #if CONFIG_PROJECT_U950
                    snprintf(page ,8,"tx1C-GM");
                    len = 8;
            #elif CONFIG_PROJECT_U985
                     snprintf(page ,5,"tx3A");
            #elif CONFIG_PROJECT_V985
                     snprintf(page ,5,"wexA");
            #endif
            
            break;

        case 1:
            len = 5;
            
            #if CONFIG_PROJECT_U950
                    snprintf(page ,5,"tx1B");
            #elif CONFIG_PROJECT_U985
                     snprintf(page ,5,"tx3B");
            #elif CONFIG_PROJECT_V985
                     snprintf(page ,5,"wexB");
            #endif
            
            break;            

        case 2:
            len = 5;
           #if CONFIG_PROJECT_U950
                   snprintf(page ,5,"tx1C");
           #elif CONFIG_PROJECT_U985
                    snprintf(page ,8,"tx3B_P1");
                    len = 8;
           #elif CONFIG_PROJECT_V985
                    snprintf(page ,5,"wexC");
           #endif
            
            break;            

        case 3: 
            len = 5;
            #if CONFIG_PROJECT_U950
                    snprintf(page , 9, "tx1C-RDA");
                    len = 9;
            #elif CONFIG_PROJECT_U985
                     snprintf(page ,5,"tx3C");
            #elif CONFIG_PROJECT_V985
                     snprintf(page ,5,"wexD");
            #endif
            
            break;            
            
        default:
            snprintf(page ,4,"err");
            len = 4;
            break;
    }	
    return len;
}
#if 0
static ssize_t zte_read_fmstate(char *page, char **start, off_t off,
		int count, int *eof, void *data)
{

    if(0==sdw_fmstat)
             snprintf(page ,4,"off");
   else if(1==sdw_fmstat)
             snprintf(page ,4," on");
    else
             snprintf(page ,4,"err");
    return 4;
}

static ssize_t zte_set_fmstate(struct file *filp,
				       const char *buff, size_t len,
				       loff_t * off)
{
    char state[4];

    if (len > 3)
		len = 3;

    if (copy_from_user(state, buff, len))
		return -EFAULT;

    if ('0'==state[0])
        sdw_fmstat = 0;
    else if('1'==state[0])
        sdw_fmstat = 1;
    else 
        sdw_fmstat = 0xFF;
        return len;
}
#endif
static void zte_creat_hver_proc_file(void)
{
	struct proc_dir_entry *prop_proc_file =
		create_proc_entry("driver/hardwareVersion", 0444, NULL);

	if (prop_proc_file) {
		prop_proc_file->read_proc = zte_read_hver;
		prop_proc_file->write_proc = NULL;
	}
}

#if 0
int zte_get_fmstate(void)
{
    return sdw_fmstat;
}


static void zte_creat_fm_state_proc_file(void)
{
	struct proc_dir_entry *prop_proc_file =
		create_proc_entry("driver/fmstate", 0666, NULL);

	if (prop_proc_file) {
		prop_proc_file->read_proc = zte_read_fmstate;
		prop_proc_file->write_proc = zte_set_fmstate;
	}
}
#endif

int  zte_hver_proc_init(void)
{
    zte_creat_hver_proc_file();
    return 0;
}

#if 0
int __init zte_fm_state_proc_init(void)
{
    zte_creat_fm_state_proc_file();
    return 0;
}
#endif
