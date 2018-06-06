/* Function prototypes. */

/* main.c */
_PROTOTYPE(int main, (int argc, char **argv));

/* store.c */
_PROTOTYPE(int do_publish, (message *m_ptr));
_PROTOTYPE(int do_retrieve, (message *m_ptr));
_PROTOTYPE(int do_subscribe, (message *m_ptr));
_PROTOTYPE(int do_getsysinfo, (message *m_ptr));
_PROTOTYPE(int do_delete, (message *m_ptr));
_PROTOTYPE(void init_store, (void));

/* ds_test.c */
_PROTOTYPE(int ds_test, (int ds_vmid));
