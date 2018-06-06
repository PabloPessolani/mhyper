
/* The following names are used to inform the VMM for commands */
#define VMM_cmd    		m1_i1		/* vmmreq command code */

#define VMM_i2     		m1_i2
#define VMM_vmpid 		m1_i2		/* VM loader PID 	*/
#define VMM_loadep		m1_i2		/* loader endpoint */
#define VMM_vmid	 	m1_i2		/* VM id to change PM process name	*/

#define VMM_i3     		m1_i3
#define VMM_namelen		m1_i3		/* image filename lenght	*/
#define VMM_vmaddr		m1_i3		/* VM Virtual Address	*/
#define VMM_nprocs		m1_i3		/* number of boot processes */
#define VMM_vmsize 		m1_i3		/* VM size in CLICKS 	*/

#define VMM_p1     		m1_p1
#define VMM_imgname		m1_p1		/*address VM boot image filename	*/
#define VMM_process		m1_p1		/* address of boot process table */

#define VMM_p2     		m1_p2
#define VMM_img     		m1_p2		/* address of img table built by loader */

#define VMM_p3     		m1_p3
#define VMM_stack     	m1_p3		/* VM stack virtual  address		*/
#define VMM_vmdesc     	m1_p3		/* VM descriptor table address	*/

/* reply arguments */
#define VMM_rcode  		m_type

/* The following names are used to inform the FS about certain events. */
#define VMM_fs_arg1    m1_i1
#define VMM_fs_arg2    m1_i2
#define VMM_fs_arg3    m1_i3

