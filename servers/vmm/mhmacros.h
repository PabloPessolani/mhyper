
#define MHERROR(T,X) 	do { 			\
				printf("ERROR %s:%u: ",__FILE__,__LINE__);\
				printf(T,X);	\
				return(X);	\
				}while(0)

#if     MHDBG
#define MHDEBUG	printf
#else
#define MHDEBUG if(0) printf
#endif


