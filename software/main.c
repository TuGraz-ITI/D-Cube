#define _GNU_SOURCE

#include <fcntl.h>			//Needed for SPI port
#include <sys/ioctl.h>			//Needed for SPI port
#include <linux/spi/spidev.h>		//Needed for SPI port
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sched.h>
#include <sys/mman.h>
#include <bcm2835.h>

int spi_cs0_fd;				//file descriptor for the SPI device
int spi_cs1_fd;				//file descriptor for the SPI device
unsigned char spi_mode;
unsigned char spi_bitsPerWord;
unsigned int spi_speed;

#define MY_PRIORITY (99) /* we use 99 as the PRREMPT_RT use 50
			    as the priority of kernel tasklets
			    and interrupt handler by default */

#define MAX_SAFE_STACK (32*1024) /* The maximum stack size which is
				    guaranteed safe to access without
				    faulting */

#define NSEC_PER_SEC    (1000000000) /* The number of nsecs per sec. */

void rpi_stack_prefault() {
	unsigned char dummy[MAX_SAFE_STACK];
	memset(dummy, 0, MAX_SAFE_STACK);
}

#define PIN_GPIO RPI_V2_GPIO_P1_12	
#define PIN_RESET RPI_V2_GPIO_P1_11
#define OFFSET_GPIO 0
#define OFFSET_RESET 1


struct timespec diff(struct timespec start, struct timespec end)
{
	struct timespec temp;
	if ((end.tv_nsec-start.tv_nsec)<0) {
		temp.tv_sec = end.tv_sec-start.tv_sec-1;
		temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
	} else {
		temp.tv_sec = end.tv_sec-start.tv_sec;
		temp.tv_nsec = end.tv_nsec-start.tv_nsec;
	}
	return temp;
}

void clear_pin(uint8_t p){
	bcm2835_gpio_clr_len(p);
	bcm2835_gpio_clr_hen(p);
	bcm2835_gpio_clr_afen(p);
	bcm2835_gpio_clr_fen(p);
	bcm2835_gpio_clr_aren(p);
	bcm2835_gpio_clr_ren(p);
	bcm2835_gpio_set_eds(p);
}

void setup_pin(uint8_t p){
	bcm2835_gpio_fsel(p, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_set_pud(p, BCM2835_GPIO_PUD_UP);

	clear_pin(p);
	//	bcm2835_gpio_fen(p);
}

//*******************************************
//*******************************************
//****************** main *******************
//*******************************************
//*******************************************

uint32_t current=0, voltage=0;
uint8_t gpio=0,avg_count=0;

int main(int argc, char* argv){

	struct timespec r,t,s;
	struct sched_param param;
	int interval = 25000; /* 30us*/

	param.sched_priority = MY_PRIORITY;
	if(sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
		perror("sched_setscheduler failed");
		exit(-1);
	}

	cpu_set_t  mask;
	CPU_ZERO(&mask);
	CPU_SET(3, &mask);
	if (sched_setaffinity(0, sizeof(mask), &mask) )
	{
		perror( "sched_setaffinity failed");
		exit(-3);
	}

	if(mlockall(MCL_CURRENT|MCL_FUTURE) == -1) {
		perror("mlockall failed");
		exit(-2);
	}

	int fd;
	char * myfifo = "/fifo/logger";
//	mkfifo(myfifo, 0666);
//	fd = open(myfifo, O_WRONLY);
	fd = open(myfifo, O_WRONLY | O_CREAT);
	chmod(myfifo, 644);

	if (!bcm2835_init())
		return -5;
	bcm2835_spi_begin();
	bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
	bcm2835_spi_setDataMode(BCM2835_SPI_MODE3);
	bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_32);
	bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
	bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);

	setup_pin(PIN_GPIO);
	setup_pin(PIN_RESET);
	bcm2835_gpio_fen(PIN_RESET);

	rpi_stack_prefault();

	//	clock_gettime(CLOCK_REALTIME ,&t);

	unsigned char tx_data[] = { 0x82, 0x03, 0x00, 0x00, 0x00, 0x00 }; 
	unsigned char rx_data[sizeof(tx_data)] = { 0x00 }; 
	unsigned char buf[sizeof(rx_data)-2+sizeof(struct timespec)+sizeof(uint8_t)]={0};
	//unsigned char buf[sizeof(rx_data)-2+sizeof(long)]={0};


	clock_gettime(CLOCK_MONOTONIC ,&s);
	s.tv_sec++;
	uint8_t gpio=0;
	uint8_t event=0;

	unsigned long l,delta;
	while(1) {
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &s, NULL);
		clock_gettime(CLOCK_REALTIME ,&t);
		bcm2835_spi_transfernb(tx_data,rx_data,sizeof(tx_data));
		if (bcm2835_gpio_eds(PIN_RESET))
		{                                                
			bcm2835_gpio_set_eds(PIN_RESET);
			event=1;
		} else{

		}
		/*if (bcm2835_gpio_eds(PIN_GPIO))
		  {                                                
		  bcm2835_gpio_set_eds(PIN_GPIO);
		  gpio|=1<<OFFSET_GPIO;
		  } else{
		  gpio&=~(1<<OFFSET_GPIO);
		  }*/
		gpio=0|(bcm2835_gpio_lev(PIN_GPIO)<<OFFSET_GPIO)|(bcm2835_gpio_lev(PIN_RESET)<<OFFSET_RESET);
		gpio|=(event<<2);

		uint16_t c=(rx_data[2]<<8)|rx_data[3];
		uint16_t v=(rx_data[4]<<8)|rx_data[5];

                current+=c;
                voltage+=v;                

                avg_count++;
		if(avg_count>=5){
                        uint16_t a_voltage=voltage/avg_count;
                        uint16_t a_current=current/avg_count;
			//bcm2835_delayMicroseconds(2);
			//memcpy(buf,rx_data+2,sizeof(rx_data)-2);
			memcpy(buf,&a_current,sizeof(uint16_t));
			memcpy(buf+sizeof(uint16_t),&a_voltage,sizeof(uint16_t));
			memcpy(buf+2*sizeof(uint16_t),&(gpio),sizeof(uint8_t));
			memcpy(buf+2*sizeof(uint16_t)+sizeof(uint8_t),&(t),sizeof(struct timespec));

			//memcpy(buf+sizeof(rx_data)-2,&(t.tv_nsec),sizeof(long));
			write(fd, buf, sizeof(buf));
			event=0;
                        avg_count=0;
                        current=0;
                        voltage=0;
		}
		s.tv_nsec += interval;
		while (s.tv_nsec >= NSEC_PER_SEC) {
			s.tv_nsec -= NSEC_PER_SEC;
			s.tv_sec++;
		}
		r.tv_nsec=t.tv_nsec;
		r.tv_sec=r.tv_sec;
		/*
		   if(t.tv_nsec>l){
		   delta=t.tv_nsec-l;
		   if(delta>100000L)
		   printf("%lu\n",delta);
		   }
		   l=t.tv_nsec;
		 */
	}
	bcm2835_spi_end();
	bcm2835_close();

}
