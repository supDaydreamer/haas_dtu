#define TTY_DEV_PREFIX				("/dev/ttyS")
#define UART_RX_MAX_LEN				(1024)
#define UART_TX_MAX_LEN				(1024)
#define UART_MSG_BOX_DEEP			(64)

//#define UART_DEBUG_PRINT			(1)

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <termios.h>
#include <sys/timeb.h>
#include <stdint.h>
#include "common.h"
#include "uart.h"

uint32_t g_tty1_index = TTY_1_INDEX;
uint32_t g_tty2_index = TTY_2_INDEX;

BFMSG_BOX_HANDLE g_uart1_rx_box_handle = NULL;
BFMSG_BOX_HANDLE g_uart2_rx_box_handle = NULL;
BFMSG_BOX_HANDLE g_uart1_tx_box_handle = NULL;
BFMSG_BOX_HANDLE g_uart2_tx_box_handle = NULL;

static int set_serial(int fd,int nSpeed,int nBits,char nEvent,int nStop)
{
    struct termios newttys1,oldttys1;

     if(tcgetattr(fd,&oldttys1)!=0) 
     {
          perror("Setupserial 1");
          return -1;
     }
     bzero(&newttys1,sizeof(newttys1));
     newttys1.c_cflag|=(CLOCAL|CREAD ); 
     newttys1.c_cflag &=~CSIZE;
     switch(nBits)
     {
         case 7:
             newttys1.c_cflag |=CS7;
             break;
         case 8:
             newttys1.c_cflag |=CS8;
             break;
     }
     switch( nEvent )
     {
         case '0': 
             newttys1.c_cflag |= PARENB;
             newttys1.c_iflag |= (INPCK | ISTRIP);
             newttys1.c_cflag |= PARODD;
             break;
         case 'E':
             newttys1.c_cflag |= PARENB; 
             newttys1.c_iflag |= ( INPCK | ISTRIP);
             newttys1.c_cflag &= ~PARODD;
             break;
         case 'N': 
             newttys1.c_cflag &= ~PARENB;
             break;
     }
    switch( nSpeed )  
    {
        case 2400:
            cfsetispeed(&newttys1, B2400);
            cfsetospeed(&newttys1, B2400);
            break;
        case 4800:
            cfsetispeed(&newttys1, B4800);
            cfsetospeed(&newttys1, B4800);
            break;
        case 9600:
            cfsetispeed(&newttys1, B9600);
            cfsetospeed(&newttys1, B9600);
            break;
        case 115200:
            cfsetispeed(&newttys1, B115200);
            cfsetospeed(&newttys1, B115200);
            break;
        default:
            cfsetispeed(&newttys1, B9600);
            cfsetospeed(&newttys1, B9600);
            break;
    }
    if( nStop == 1)
    {
        newttys1.c_cflag &= ~CSTOPB;
    }
    else if( nStop == 2)
    {
        newttys1.c_cflag |= CSTOPB;
    }
    newttys1.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
 
    newttys1.c_oflag &= ~OPOST;  
    newttys1.c_oflag &= ~(ONLCR | OCRNL);
    newttys1.c_iflag &= ~(ICRNL | INLCR);    
    newttys1.c_cflag &= ~CRTSCTS;
    newttys1.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    newttys1.c_cc[VTIME] = 0;
    newttys1.c_cc[VMIN]  = 0; 
    tcflush(fd ,TCIFLUSH);
    if((tcsetattr( fd, TCSANOW,&newttys1))!=0)
    {
        perror("com set error");
        return -1;
    }

    return 0;
}

bool read_uart_msg(BFMSG_BOX_HANDLE box_handle, uint8_t *data, size_t *len)
{
	void *msg_type = NULL;
	void *msg_data = NULL;
	bool is_success = bfmsg_box_read(box_handle, &msg_type, &msg_data);
	uint8_t *msg_data_p = (uint8_t *)msg_type;
	size_t msg_data_len = (size_t)msg_data;
	if (is_success) {
		memcpy(data, msg_data_p, msg_data_len);
		*len = msg_data_len;
		if (msg_type) free(msg_type);
		//if (msg_data) free(msg_data);
	}
	return is_success;
}

void write_uart_msg(BFMSG_BOX_HANDLE box_handle, uint8_t *data, size_t len)
{
	uint8_t *msg_uart_data = malloc(len);
	memcpy(msg_uart_data, data, len);
	void *overwrite_msg_type = NULL;
	void *overwrite_msg_data = NULL;
	if(!bfmsg_box_write(box_handle, msg_uart_data, (void *)len, &overwrite_msg_type, &overwrite_msg_data)) {
		if (overwrite_msg_type) free(overwrite_msg_type);
		//if (overwrite_msg_data) free(overwrite_msg_data);
	}
}

void *uart_rx_task(void *args)
{
	uint8_t in_buf[UART_RX_MAX_LEN] = {0};
	char tty_dev_buf[32] = {0};
	uint32_t uart_index = (uint32_t)args;
	uint32_t usleep_time = 100 * 1000;

	snprintf(tty_dev_buf, sizeof(tty_dev_buf), "%s%u", TTY_DEV_PREFIX, uart_index);

	int fd = open(tty_dev_buf, O_RDONLY | O_NOCTTY);
	if (fd < 0)
	{
		perror(tty_dev_buf);
		fprintf(stderr, "[%s]open %s failed!\n", __FUNCTION__, tty_dev_buf);
		exit(EXIT_FAILURE);
	}
	dbg_printf("=== TTY RX-%u <%s> init OK! ===\n", uart_index, tty_dev_buf);

	if (set_serial(fd, 9600, 8, 0, 1) == -1)
	{
		fprintf(stderr, "[%s]configure %s failed!\n", __FUNCTION__, tty_dev_buf);
		exit(EXIT_FAILURE);
	}

	BFMSG_BOX_HANDLE box_handle = bfmsg_box_init(UART_MSG_BOX_DEEP);
	if (uart_index == 1) {
		g_uart1_rx_box_handle = box_handle;
		usleep_time = 20 * 1000;
	} else if (uart_index == 2) {
		g_uart2_rx_box_handle = box_handle;
		usleep_time = 200 * 1000;
	}

	while (1) {
		usleep(usleep_time);
		size_t readlen = read(fd, in_buf, sizeof(in_buf));
		if (readlen > 0) {
#ifdef UART_DEBUG_PRINT
			dbg_printf("\033[33m");
			dbg_printf("======> read tty-%u (%s) %d byte\n", uart_index, tty_dev_buf, readlen);
			dbg_printf("------>     ");
			for (int i = 0; i < readlen; i++) {
				dbg_printf("%02X ", in_buf[i] & 0xFF);
			}
			dbg_printf("\n\n");
			dbg_printf("\033[0m");
#endif
			if (box_handle) write_uart_msg(box_handle, in_buf, readlen);
		}
	}
	close(fd);
	return 0;
}

void uart_tx(uint32_t uart_index, uint8_t *data, size_t len)
{
	static int s_fd[sizeof(uint32_t) * 8] = {0};
	static uint32_t s_fd_inited_map = 0;
	//dbg_printf("### s_fd_inited_map: %X ###\n", s_fd_inited_map);
	
	char tty_dev_buf[32] = {0};
	snprintf(tty_dev_buf, sizeof(tty_dev_buf), "%s%u", TTY_DEV_PREFIX, uart_index);
	
	if (((1 << uart_index) & s_fd_inited_map) == 0) {
		s_fd_inited_map |= (1 << uart_index);
		s_fd[uart_index] = open(tty_dev_buf, O_WRONLY | O_NOCTTY | O_NONBLOCK);
		if (s_fd[uart_index] < 0)
		{
			perror(tty_dev_buf);
			fprintf(stderr, "[%s]open %s failed!\n", __FUNCTION__, tty_dev_buf);
			exit(EXIT_FAILURE);
		}
		dbg_printf("=== TTY-%u TX <%s> init OK! ===\n", uart_index, tty_dev_buf);

		if (set_serial(s_fd[uart_index], 9600, 8, 0, 1) == -1)
		{
			fprintf(stderr, "[%s]configure %s failed!\n", __FUNCTION__, tty_dev_buf);
			exit(EXIT_FAILURE);
		}
	}

	BFMSG_BOX_HANDLE box_handle = bfmsg_box_init(UART_MSG_BOX_DEEP);
	if (uart_index == 1) {
		g_uart1_tx_box_handle = box_handle;
	} else if (uart_index == 2) {
		g_uart2_tx_box_handle = box_handle;
	}

#ifdef UART_DEBUG_PRINT
	size_t writelen = 0;
	if (len > 0) writelen = write(s_fd[uart_index], data, len);
	dbg_printf("\033[36m");
	dbg_printf("======> write tty-%lu (%s @ fd: %d) %d byte\n", uart_index, tty_dev_buf, s_fd[uart_index], writelen);
	dbg_printf("------>     ");
	for (int i = 0; i < writelen; i++) {
		dbg_printf("%02X ", data[i] & 0xFF);
	}
	dbg_printf("\n\n");
	dbg_printf("\033[0m");
#else
	write(s_fd[uart_index], data, len);
#endif
	if (box_handle) write_uart_msg(box_handle, data, len);

	//close(s_fd[uart_index]);
	//s_fd_inited_map &= (~(1 << uart_index));
}

