#include <stdlib.h>
#include <stdio.h>

#include <minix/com.h>

int main(int argc, char **argv){
	FILE *fd,*fp;
	int i,j,offset, nr_metrics;
	int metric[14];

	/* Invalid arguments */
	if(argc != 3){
		perror("Invalid usage.\n");
		perror("Usage: ./metric <METRIC_ADDR> <nr_metrics>\n");
		return -1;
	}

	nr_metrics = offset = atoi(argv[2]);
	if( nr_metrics < 0 || nr_metrics > NR_METRICS) {
		printf(" 0 < nr_metrics <=%d \n", NR_METRICS);
		return -1;
	}
	
	printf("Metric in progress...\n");
	fd = fopen("/dev/kmem", "r");
	fp = fopen("metricas.txt","w");
	printf("Metric [nr]: uptime K[VM0] K[VM1] K[VM2] K[VM3] T[VM0] T[VM1] T[VM2] T[VM3] P[VM0] P[VM1] P[VM2] P[VM3] IDLE\n");
    fprintf(fp, "Metric [nr]: uptime K[VM0] K[VM1] K[VM2] K[VM3] T[VM0] T[VM1] T[VM2] T[VM3] P[VM0] P[VM1] P[VM2] P[VM3] IDLE\n");
	    
	 /* Move file cursor to argv[1] position */
	 offset = atoi(argv[1]);
	 fseek(fd, offset, SEEK_SET);
		    
		for(j=0; j<nr_metrics; j++){
		    /* Read data from /dev/kmem */
			fread(metric, sizeof(int), 14, fd);
			if(metric[0] == 0) break;
			    
			    /* Print it in output file */
			printf("Metric [%d]:", j+1);
			fprintf(fp, "Metric [%d]:", j+1);
			for(i=0;i<14;i++) {
				printf(" %d",metric[i]);
				fprintf(fp, " %d",metric[i]);
			}
		    printf("\n");
		    fprintf(fp, "\n");
		}
	fclose(fd);
	fclose(fp);
}

