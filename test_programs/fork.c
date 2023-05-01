#include <stdlib.h>
#include <unistd.h>
int main() {
	while (1) {
		fork();
		usleep(25000);
	} 
	return 0;
}