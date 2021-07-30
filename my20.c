#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <linux/if_tun.h>
#include <fcntl.h>		/*open*/
#include <unistd.h>		/*close*/
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/select.h>
#include <sys/msg.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <sys/prctl.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/wait.h>

/*
在eth II中type字段分配中有大量私有无效协议号，
这里选取2000-2079用于把802.3 LLC转换为eth II type，
以便应用程序通过标准socket接受802.3类型的报文。
*/
enum{
	ETH_P_802_3_BPDU = 2000,
};

union ethframe {
    struct {
        struct ethhdr header;
        unsigned char data[ETH_DATA_LEN];
    }field;
    unsigned char buffer[ETH_FRAME_LEN];	/*预留头部减少拷贝*/
};

typedef struct{
	uint32_t saddr; //源地址
	uint32_t daddr; //目的地址
	char mbz;//置空
	char ptcl; //协议类型
	unsigned short tcpl; //TCP长度
}psd_header_t;

typedef struct{
    long type;
	int port;
	//int offset;	/*预留头部减少拷贝*/
	int length;
	unsigned char buffer[2048];
}pkt_msg_t;

typedef struct{
	char *name;
	int priority;
	int(*func)(int);
	pid_t pid;
}subprocess_t;

static void dump_frame(char *desc, unsigned char *buffer, int length)
{
	int i;
	char temp[1024];
	int len, offset = 0;

	len = sprintf(temp, "%s, pkt length: %d\r\n", desc, length);
	offset += len;
	for(i=0;i<length&&i<128;i++){
		len = sprintf(&temp[offset], "0x%.2x ", buffer[i]);
		offset += len;
		if((i+1)%16 == 0){
			len = sprintf(&temp[offset], "\r\n");
			offset += len;
		}
	}
	if(i%16 != 0){
		len = sprintf(&temp[offset], "\r\n");
		offset += len;
	}
	printf("%s", temp);
}

static int swp_util_alloc_tap(char *name) {
    struct ifreq ifr;
    int fd=-1, err=0;
	int nonblock = 1;

    if ((fd=open("/dev/net/tun",O_RDWR))<0) {
		perror ("open");
        printf ("TAP-ALLOC, Can't open /dev/net/tun\r\n");
        return -1;
    }

	memset(&ifr,0,sizeof(ifr));
	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", name);
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	
    err = ioctl(fd, TUNSETIFF, (void*)&ifr);
    if (err) {
		perror ("ioctl TUNSETIFF");
        printf ("TAP-ALLOC, Can't associate file descriptor for tap (%s) :%d\r\n",
                ifr.ifr_name,err);
        close(fd);
        return -1;
    }
#if 0
    err = ioctl(fd, FIONBIO, (char *)&nonblock);
    if (err) {
		perror ("ioctl FIONBIO");
        printf ("TAP-ALLOC, Can't set non-blocking to tap fd for interface:%s: %d\r\n",
                   ifr.ifr_name,err);
        close(fd);
        return -1;
    }
#endif
    return fd;
}

static uint16_t ip_chksum(uint8_t *ptr, int size)
{
	int cksum = 0;
	int index = 0;
	
	*(ptr + 10) = 0;
	*(ptr + 11) = 0;
 
	if(size % 2 != 0)
		return 0;
	
	while(index < size)
	{        
		cksum += *(ptr + index + 1);
		cksum += *(ptr + index) << 8;
 
		index += 2;
	}
 
	while(cksum > 0xffff)
	{
		cksum = (cksum >> 16) + (cksum & 0xffff);
	}
	return ~cksum;
}

static uint16_t tcp_chksum(psd_header_t *psd_header, uint8_t *ptr, int size)
{
	int cksum = 0;
	int index = 0;

	*(ptr + 0x10) = 0;
	*(ptr + 0x11) = 0;
 
	if(size % 2 != 0)
		return 0;

	index = 0;
	while(index < sizeof(psd_header_t))
	{        
		cksum += *((uint8_t *)psd_header + index + 1);
		cksum += *((uint8_t *)psd_header + index) << 8;
 
		index += 2;
	}

	index = 0;
	while(index < size)
	{        
		cksum += *(ptr + index + 1);
		cksum += *(ptr + index) << 8;
 
		index += 2;
	}
 
	while(cksum > 0xffff)
	{
		cksum = (cksum >> 16) + (cksum & 0xffff);
	}
	return ~cksum;
}

static uint16_t udp_chksum(psd_header_t *psd_header, uint8_t *ptr, int size)
{
	int cksum = 0;
	int index = 0;

	*(ptr + 0x6) = 0;
	*(ptr + 0x7) = 0;
 
	if(size % 2 != 0)
		return 0;

	index = 0;
	while(index < sizeof(psd_header_t))
	{        
		cksum += *((uint8_t *)psd_header + index + 1);
		cksum += *((uint8_t *)psd_header + index) << 8;
 
		index += 2;
	}

	index = 0;
	while(index < size)
	{        
		cksum += *(ptr + index + 1);
		cksum += *(ptr + index) << 8;
 
		index += 2;
	}
 
	while(cksum > 0xffff)
	{
		cksum = (cksum >> 16) + (cksum & 0xffff);
	}
	return ~cksum;
}


static int panel_port_tap_fd[2] = {-1};
static int intf_vlan_tap_fd[3] = {-1};
static void *packet_send_thread_init(void *arg)
{
	int fd, maxfdp;
	fd_set fds;
	pkt_msg_t pkt;
	int panel_port, intf_vlan;
	int sdk_rx;
	int rv;
	char desc[128];
	
	prctl(PR_SET_NAME, "myTX");

	key_t key = ftok("/home", 100);
	sdk_rx = msgget(key, IPC_CREAT|O_RDWR|0777);
	
	while(1){
		maxfdp = 0;
		FD_ZERO(&fds);
		for(panel_port=0;panel_port<2;panel_port++){
			fd = panel_port_tap_fd[panel_port];
			FD_SET(fd, &fds);
			maxfdp = fd>maxfdp?fd:maxfdp; 
		}
		for(intf_vlan=1;intf_vlan<=2;intf_vlan++){
			fd = intf_vlan_tap_fd[intf_vlan];
			FD_SET(fd, &fds);
			maxfdp = fd>maxfdp?fd:maxfdp; 
		}
		maxfdp++;	//描述符最大值加1

		switch(select(maxfdp, &fds, NULL, NULL, NULL))
		{
			case -1:
				return NULL;	//select错误，退出程序 
			case 0:
				break; //再次轮询 
			default:
				for(panel_port=0;panel_port<2;panel_port++){
					fd = panel_port_tap_fd[panel_port];
					if(FD_ISSET(fd, &fds)){
						/*从tap端口读来自于内核的报文*/
						pkt.length = read(fd, pkt.buffer, 2048);
						if(pkt.length < 0){
							perror("[TX] Data read from fd:");
							continue;
						}
						/*内核为何会发IPV6报文？这里先屏蔽掉*/
						if((pkt.buffer[12] == 0x86) && (pkt.buffer[13] == 0xdd)){
							continue;
						}
						
						/*从网络发送报文*/
						pkt.type = 1;	//未使用，type不能为0
						pkt.port = (panel_port==0)?1:0;	/*G0.0->G0.1, G0.1->G0.0*/
						sprintf(desc, "SDK TX from G0.%d", panel_port);
						dump_frame(desc, pkt.buffer, pkt.length);
						rv = msgsnd(sdk_rx, &pkt, sizeof(pkt_msg_t)-sizeof(long)-2048+pkt.length, IPC_NOWAIT);	//队列满则丢弃	
						if(rv <0)
							perror("msgsnd failed");
					}
				}

				for(intf_vlan=1;intf_vlan<=2;intf_vlan++){
					fd = intf_vlan_tap_fd[intf_vlan];
					if(FD_ISSET(fd, &fds)){
						/*从tap端口读来自于内核的报文*/
						pkt.length = read(fd, pkt.buffer, 2048);
						if(pkt.length < 0){
							perror("[TX] Data read from fd:");
							continue;
						}
						/*内核为何会发IPV6报文？这里先屏蔽掉*/
						if((pkt.buffer[0xc] == 0x86) && (pkt.buffer[0xd] == 0xdd)){
							continue;
						}
						/*内核为何会自动发IGMP报文？这里先屏蔽掉*/
						if((pkt.buffer[0xc] == 0x08) && (pkt.buffer[0xd] == 0x00) && (pkt.buffer[0x17] == 0x2)){
							continue;
						}
				
						/*模拟虚拟对端*/
						if(intf_vlan==1){
							//Source MAC:	00:e0:0f:00:00:00->00:e0:0f:00:22:00
							//Source IP:	10.0.0.1->20.0.0.2
							if((pkt.buffer[0xc] == 0x08) && (pkt.buffer[0xd] == 0x06)){	//ARP
								pkt.buffer[0xa] = 0x22;
								pkt.buffer[0x1a] = 0x22;
								pkt.buffer[0x1c] = 20;
								pkt.buffer[0x1f] = 2;
							}else if((pkt.buffer[0xc] == 0x08) && (pkt.buffer[0xd] == 0x00)){	//IP
#if 0
								uint16_t checksum;
								psd_header_t psd_header;
								dump_frame(desc, pkt.buffer, pkt.length);
								checksum = ((uint16_t)pkt.buffer[0x18])<<8 | pkt.buffer[0x19];
								printf("ip checksum:0x%x\r\n", checksum);
								checksum = ip_chksum(&pkt.buffer[0xe], 0x14);
								printf("ip checksum:0x%x\r\n", checksum);
								if(pkt.buffer[0x17] == 0x06){	//TCP
									checksum = ((uint16_t)pkt.buffer[0x32])<<8 | pkt.buffer[0x33];
									printf("tcp checksum:0x%x\r\n", checksum);
									memcpy(&psd_header.saddr, &pkt.buffer[0x1a], 4);
									memcpy(&psd_header.daddr, &pkt.buffer[0x1e], 4);
									psd_header.mbz = 0;
									psd_header.ptcl = IPPROTO_TCP;
									unsigned short tcpl = (((uint16_t)pkt.buffer[0x10])<<8 | pkt.buffer[0x11]) - 20;
									psd_header.tcpl = htons(tcpl);
									checksum = tcp_chksum(&psd_header, &pkt.buffer[0x22], tcpl);
									printf("tcp checksum:0x%x\r\n", checksum);
								}
#endif
								pkt.buffer[0xa] = 0x22;
								pkt.buffer[0x1a] = 20;
								pkt.buffer[0x1d] = 2;					
							}
						}else{
							//Source MAC:	00:e0:0f:00:00:00->00:e0:0f:00:12:00
							//Source IP:	20.0.0.1->10.0.0.2
							if((pkt.buffer[0xc] == 0x08) && (pkt.buffer[0xd] == 0x06)){	//ARP
								pkt.buffer[0xa] = 0x12;
								pkt.buffer[0x1a] = 0x12;
								pkt.buffer[0x1c] = 10;
								pkt.buffer[0x1f] = 2;
							}else if((pkt.buffer[0xc] == 0x08) && (pkt.buffer[0xd] == 0x00)){	//IP
								pkt.buffer[0xa] = 0x12;
								pkt.buffer[0x1a] = 10;
								pkt.buffer[0x1d] = 2;
							}
						}
						
						/*从网络发送报文*/
						pkt.type = 1;	//未使用，type不能为0
						panel_port = intf_vlan - 1;	//查询mac表
						pkt.port = (panel_port==0)?1:0;	/*G0.0->G0.1, G0.1->G0.0*/
						sprintf(desc, "SDK TX from intfV%d", intf_vlan);
						dump_frame(desc, pkt.buffer, pkt.length);
						rv = msgsnd(sdk_rx, &pkt, sizeof(pkt_msg_t)-sizeof(long)-2048+pkt.length, IPC_NOWAIT);	//队列满则丢弃	
						if(rv <0)
							perror("msgsnd failed");
					}
				}
				break;
		}
	}
	return NULL;
}

static void *packet_recv_thread_init(void *arg)
{
	int fd;
	pkt_msg_t pkt;
	unsigned char buffer[2048];
	int offset;
	struct ethhdr *header;
	unsigned short proto;
	int length;
	int sdk_rx;
	char desc[128];
	uint16_t checksum;
	psd_header_t psd_header;
	unsigned short tcpl;

	prctl(PR_SET_NAME, "myRX");

	key_t key = ftok("/home", 100);
	sdk_rx = msgget(key, IPC_CREAT|O_RDWR|0777);

	while(1){
		/*从网络接受报文*/
		msgrcv(sdk_rx, &pkt, sizeof(pkt)-sizeof(pkt.type), 0, 0);	//阻塞等待接收
		/*模拟虚拟对端*/
		if(pkt.port+1==2){
			//Dest MAC:	00:e0:0f:00:12:00->00:e0:0f:00:00:00
			//Dest IP:	10.0.0.2->20.0.0.1
			if((pkt.buffer[0xc] == 0x08) && (pkt.buffer[0xd] == 0x06)){	//ARP
				if(pkt.buffer[0x0] != 0xff)
					pkt.buffer[0x4] = 0x00;
				pkt.buffer[0x26] = 20;
				pkt.buffer[0x29] = 1;
			}else if((pkt.buffer[0xc] == 0x08) && (pkt.buffer[0xd] == 0x00)){	//IP
				if(pkt.buffer[0x0] != 0xff && pkt.buffer[0x0] != 0x1)	//非广播和组播
					pkt.buffer[0x4] = 0x00;
				if(pkt.buffer[0x0] != 0x1){	//非组播
					pkt.buffer[0x1e] = 20;
					if(pkt.buffer[0x21] != 0xff)	//非广播
						pkt.buffer[0x21] = 1;
				}
				/*重新计算IP头checksum*/
				checksum = ip_chksum(&pkt.buffer[0xe], 0x14);
				pkt.buffer[0x18] = (checksum >> 8) & 0xff;
				pkt.buffer[0x19] = checksum & 0xff;
				if(pkt.buffer[0x17] == 0x06){	//TCP
					/*重新计算TCP头checksum*/
					memcpy(&psd_header.saddr, &pkt.buffer[0x1a], 4);
					memcpy(&psd_header.daddr, &pkt.buffer[0x1e], 4);
					psd_header.mbz = 0;
					psd_header.ptcl = IPPROTO_TCP;
					tcpl = (((uint16_t)pkt.buffer[0x10])<<8 | pkt.buffer[0x11]) - 20;
					psd_header.tcpl = htons(tcpl);
					checksum = tcp_chksum(&psd_header, &pkt.buffer[0x22], tcpl);
					pkt.buffer[0x32] = (checksum >> 8) & 0xff;
					pkt.buffer[0x33] = checksum & 0xff;
				}else if(pkt.buffer[0x17] == 0x11){	//UDP
					/*重新计算UDP头checksum*/
					memcpy(&psd_header.saddr, &pkt.buffer[0x1a], 4);
					memcpy(&psd_header.daddr, &pkt.buffer[0x1e], 4);
					psd_header.mbz = 0;
					psd_header.ptcl = IPPROTO_UDP;
					tcpl = (((uint16_t)pkt.buffer[0x10])<<8 | pkt.buffer[0x11]) - 20;
					psd_header.tcpl = htons(tcpl);
					checksum = udp_chksum(&psd_header, &pkt.buffer[0x22], tcpl);
					pkt.buffer[0x28] = (checksum >> 8) & 0xff;
					pkt.buffer[0x29] = checksum & 0xff;
				}
			}
		}else{
			//Dest MAC:	00:e0:0f:00:22:00->00:e0:0f:00:00:00
			//Dest IP:	20.0.0.2->10.0.0.1
			if((pkt.buffer[0xc] == 0x08) && (pkt.buffer[0xd] == 0x06)){	//ARP
				if(pkt.buffer[0x0] != 0xff)
					pkt.buffer[0x4] = 0x00;
				pkt.buffer[0x26] = 10;
				pkt.buffer[0x29] = 1;
			}else if((pkt.buffer[0xc] == 0x08) && (pkt.buffer[0xd] == 0x00)){	//IP
				if(pkt.buffer[0x0] != 0xff && pkt.buffer[0x0] != 0x1)	//非广播和组播
					pkt.buffer[0x4] = 0x00;
				if(pkt.buffer[0x0] != 0x1){	//非组播
					pkt.buffer[0x1e] = 10;
					if(pkt.buffer[0x21] != 0xff)	//非广播
						pkt.buffer[0x21] = 1;
				}
				/*重新计算IP头checksum*/
				checksum = ip_chksum(&pkt.buffer[0xe], 0x14);
				pkt.buffer[0x18] = (checksum >> 8) & 0xff;
				pkt.buffer[0x19] = checksum & 0xff;
				if(pkt.buffer[0x17] == 0x06){	//TCP
					/*重新计算TCP头checksum*/
					memcpy(&psd_header.saddr, &pkt.buffer[0x1a], 4);
					memcpy(&psd_header.daddr, &pkt.buffer[0x1e], 4);
					psd_header.mbz = 0;
					psd_header.ptcl = IPPROTO_TCP;
					tcpl = (((uint16_t)pkt.buffer[0x10])<<8 | pkt.buffer[0x11]) - 20;
					psd_header.tcpl = htons(tcpl);
					checksum = tcp_chksum(&psd_header, &pkt.buffer[0x22], tcpl);
					pkt.buffer[0x32] = (checksum >> 8) & 0xff;
					pkt.buffer[0x33] = checksum & 0xff;
				}else if(pkt.buffer[0x17] == 0x11){	//UDP
					/*重新计算UDP头checksum*/
					memcpy(&psd_header.saddr, &pkt.buffer[0x1a], 4);
					memcpy(&psd_header.daddr, &pkt.buffer[0x1e], 4);
					psd_header.mbz = 0;
					psd_header.ptcl = IPPROTO_UDP;
					tcpl = (((uint16_t)pkt.buffer[0x10])<<8 | pkt.buffer[0x11]) - 20;
					psd_header.tcpl = htons(tcpl);
					checksum = udp_chksum(&psd_header, &pkt.buffer[0x22], tcpl);
					pkt.buffer[0x28] = (checksum >> 8) & 0xff;
					pkt.buffer[0x29] = checksum & 0xff;
				}
			}
		}
		sprintf(desc, "SDK RX from G0.%d", pkt.port);
		dump_frame(desc, pkt.buffer, pkt.length);
		offset = 32;
		memcpy(buffer+offset, pkt.buffer, pkt.length);

		/*分析报文*/
		header = (struct ethhdr *)(buffer+offset);
		proto = htons(header->h_proto);
		if(proto <= 0x05DC){
			if((buffer[offset+14] == 0x42) && (buffer[offset+15] == 0x42)){
				proto = ETH_P_802_3_BPDU;
			}else{
				/*unknown*/
				continue;
			}
			/*添加新的ether type*/
			offset -= 2;
			memcpy(buffer+offset, header, 12);
			header = (struct ethhdr *)(buffer+offset);
			header->h_proto = htons(ETH_P_802_3_BPDU);
			pkt.length += 2;
		}
		
		/*从tap端口交给内核处理*/
		if((proto != ETH_P_IP) && (proto != ETH_P_ARP)){
			if(pkt.buffer[0x0] != 0xff)
				fd = panel_port_tap_fd[pkt.port];
			else
				fd = intf_vlan_tap_fd[pkt.port+1];
		}else{
			fd = intf_vlan_tap_fd[pkt.port+1];
		}
		length = write(fd, buffer+offset, pkt.length);
	}
	return NULL;
}

static int app_l2A_process();
static int app_l2B_process();
static int app_l2C_process();
static int app_l2D_process();
static int app_l2E_process();
static int app_l2F_process();
static int app_l3A_process();
static int app_l3B_process();
static int app_l3C_process();
static int app_l3D_process();
static int app_l3E_process();
static int app_l3F_process();
static int app_l4A_process();
static int app_l4B_process();
static int app_l4C_process();
static int app_l4D_process();

/*
所有2层实验G0.0与G0.1自环；
所有3、4层实验intfV1和intfV2通过G0.0和G0.1自环，
intfV1和intfV2属于不同网段，底层模拟成相同网段，模拟方法是
从intfV1发往模拟intfV1对端的报文，改成从模拟intfV2对端发往intfV2的报文，
从intfV2发往模拟intfV1对端的报文，改成从模拟intfV2对端发往intfV1的报文。
*/
static subprocess_t processes[] = {
/*	{"APP-L2A", 60, app_l2A_process, -1},
	{"APP-L2B", 60, app_l2B_process, -1},*/
/*	{"APP-L2C", 60, app_l2C_process, -1},
	{"APP-L2D", 60, app_l2D_process, -1},*/
	{"APP-L2E", 60, app_l2E_process, -1},
	{"APP-L2F", 60, app_l2F_process, -1},
/*	{"APP-L3A", 60, app_l3A_process, -1},
	{"APP-L3B", 60, app_l3B_process, -1},*/
/*	{"APP-L3C", 60, app_l3C_process, -1},
	{"APP-L3D", 60, app_l3D_process, -1},*/
/*	{"APP-L3E", 60, app_l3E_process, -1},
	{"APP-L3F", 60, app_l3F_process, -1},*/
/*	{"APP-L4A", 60, app_l4A_process, -1},
	{"APP-L4B", 60, app_l4B_process, -1},*/
/*	{"APP-L4C", 60, app_l4C_process, -1},
	{"APP-L4D", 60, app_l4D_process, -1},*/
};

/*
应用A负责定时发送单播eth II协议报文
APP-L2A TX G0.0 to G0.1, pkt length: 26
0x00 0xe0 0x0f 0x00 0x00 0x02 0x00 0xe0 0x0f 0x00 0x00 0x01 0x12 0x34 0x68 0x65 
0x6c 0x6c 0x6f 0x20 0x77 0x6f 0x72 0x6c 0x64 0x00 
SDK TX from G0.0, pkt length: 26
0x00 0xe0 0x0f 0x00 0x00 0x02 0x00 0xe0 0x0f 0x00 0x00 0x01 0x12 0x34 0x68 0x65 
0x6c 0x6c 0x6f 0x20 0x77 0x6f 0x72 0x6c 0x64 0x00 
SDK RX from G0.1, pkt length: 26
0x00 0xe0 0x0f 0x00 0x00 0x02 0x00 0xe0 0x0f 0x00 0x00 0x01 0x12 0x34 0x68 0x65 
0x6c 0x6c 0x6f 0x20 0x77 0x6f 0x72 0x6c 0x64 0x00 
APP-L2B RX G0.0 to G0.1, pkt length: 26
0x00 0xe0 0x0f 0x00 0x00 0x02 0x00 0xe0 0x0f 0x00 0x00 0x01 0x12 0x34 0x68 0x65 
0x6c 0x6c 0x6f 0x20 0x77 0x6f 0x72 0x6c 0x64 0x00 
*/
static int app_l2A_process()
{
	unsigned char source[ETH_ALEN];
    char dest[ETH_ALEN] = {0x00, 0xe0, 0x0f, 0x00, 0x00, 0x02};	/*G0.1*/
    unsigned short proto = 0x1234;
    char *data = "hello world";
    unsigned short data_len = strlen(data)+1;
    int s;
	struct sockaddr_ll saddrll;
	struct ifreq buffer;
	int ifindex;
	union ethframe frame;
	unsigned int frame_len;

	if((s = socket(AF_PACKET, SOCK_RAW, htons(proto))) < 0) {
		printf("Error: could not open socket\n");
		return -1;
	}
	
	while(1){
		memset(&buffer, 0x00, sizeof(buffer));
		strncpy(buffer.ifr_name, "G0.0", IFNAMSIZ);
		if (ioctl(s, SIOCGIFINDEX, &buffer) < 0) {
			printf("Error: could not get interface index\n");
			close(s);
			return -1;
		}
		ifindex = buffer.ifr_ifindex;

		if (ioctl(s, SIOCGIFHWADDR, &buffer) < 0) {
			printf("Error: could not get interface address\n");
			close(s);
			return -1;
		}
		memcpy(source, buffer.ifr_hwaddr.sa_data, ETH_ALEN);

		memcpy(frame.field.header.h_dest, dest, ETH_ALEN);
		memcpy(frame.field.header.h_source, source, ETH_ALEN);
		frame.field.header.h_proto = htons(proto);
		memcpy(frame.field.data, data, data_len);
		frame_len = data_len + ETH_HLEN;
		dump_frame("APP-L2A TX G0.0 to G0.1", frame.buffer, frame_len);
	    
		memset(&saddrll, 0, sizeof(saddrll));
		saddrll.sll_family = PF_PACKET;
		saddrll.sll_ifindex = ifindex;
		saddrll.sll_halen = ETH_ALEN;
		memcpy(saddrll.sll_addr, dest, ETH_ALEN);

		if (sendto(s, frame.buffer, frame_len, 0, (struct sockaddr*)&saddrll, sizeof(saddrll)) <= 0)
			printf("Error, could not send\n");

		sleep(5);
	}

	close(s);
	return 0;
}

/*应用B负责接受单播eth II协议协议报文*/
static int app_l2B_process()
{
	int s;
	unsigned short proto = 0x1234;
	char buffer[2048];
	int n_read;
	struct sockaddr_ll from;
	int len = sizeof(struct sockaddr_ll);
	char if_name[IFNAMSIZ] = {'\0'};
	char desc[128];

	if((s = socket(AF_PACKET, SOCK_RAW, htons(proto))) < 0) {
		printf("Error: could not open socket\n");
		return -1;
	}

	while(1){
		n_read = recvfrom(s, buffer, 2048, 0, (struct sockaddr*)&from, &len);
		if(NULL == if_indextoname(from.sll_ifindex, if_name)){
			perror("if_indextoname failed");
			close(s);
			return -1;
		}
		sprintf(desc, "APP-L2B RX from %s", if_name);
		dump_frame(desc, buffer, n_read);
	}

	close(s);
	return 0;
}

/*
应用C负责定时发送单播802.3协议报文
APP-L2C TX G0.0 to G0.1, pkt length: 25
0x00 0xe0 0x0f 0x00 0x00 0x02 0x00 0xe0 0x0f 0x00 0x00 0x01 0x00 0x0b 0x42 0x42 
0x03 0x55 0xaa 0x55 0xaa 0x55 0xaa 0x55 0xaa 
SDK TX from G0.0, pkt length: 25
0x00 0xe0 0x0f 0x00 0x00 0x02 0x00 0xe0 0x0f 0x00 0x00 0x01 0x00 0x0b 0x42 0x42 
0x03 0x55 0xaa 0x55 0xaa 0x55 0xaa 0x55 0xaa 
SDK RX from G0.1, pkt length: 25
0x00 0xe0 0x0f 0x00 0x00 0x02 0x00 0xe0 0x0f 0x00 0x00 0x01 0x00 0x0b 0x42 0x42 
0x03 0x55 0xaa 0x55 0xaa 0x55 0xaa 0x55 0xaa 
APP-L2D RX G0.0 to G0.1, pkt length: 25
0x00 0xe0 0x0f 0x00 0x00 0xe0 0x00 0xe0 0x0f 0x00 0x00 0x01 0x00 0x0b 0x42 0x42 
0x03 0x55 0xaa 0x55 0xaa 0x55 0xaa 0x55 0xaa 
*/
static int app_l2C_process()
{
	unsigned char source[ETH_ALEN];
    char dest[ETH_ALEN] = {0x00, 0xe0, 0x0f, 0x00, 0x00, 0x02};	/*G0.1*/
	char data[] = {0x42, 0x42, 0x3, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa};	/*LLC头及内容*/
	unsigned short data_len = sizeof(data);
	int s;
	struct sockaddr_ll saddrll;
	struct ifreq buffer;
	int ifindex;
	union ethframe frame;
	unsigned int frame_len;

	if((s = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_802_3))) < 0) {
		printf("Error: could not open socket\n");
		return -1;
	}
	
	while(1){
		memset(&buffer, 0x00, sizeof(buffer));
		strncpy(buffer.ifr_name, "G0.0", IFNAMSIZ);
		if (ioctl(s, SIOCGIFINDEX, &buffer) < 0) {
			printf("Error: could not get interface index\n");
			close(s);
			return -1;
		}
		ifindex = buffer.ifr_ifindex;

		if (ioctl(s, SIOCGIFHWADDR, &buffer) < 0) {
			printf("Error: could not get interface address\n");
			close(s);
			return -1;
		}
		memcpy(source, buffer.ifr_hwaddr.sa_data, ETH_ALEN);

		memcpy(frame.field.header.h_dest, dest, ETH_ALEN);
		memcpy(frame.field.header.h_source, source, ETH_ALEN);
		frame.field.header.h_proto = htons(data_len);
		memcpy(frame.field.data, data, data_len);
		frame_len = data_len + ETH_HLEN;
		dump_frame("APP-L2C TX G0.0 to G0.1", frame.buffer, frame_len);
	    
		memset(&saddrll, 0, sizeof(saddrll));
		saddrll.sll_family = PF_PACKET;
		saddrll.sll_ifindex = ifindex;
		saddrll.sll_halen = ETH_ALEN;
		memcpy(saddrll.sll_addr, dest, ETH_ALEN);

		if (sendto(s, frame.buffer, frame_len, 0, (struct sockaddr*)&saddrll, sizeof(saddrll)) <= 0)
			printf("Error, could not send\n");

		sleep(5);
	}

	close(s);
	return 0;
}

/*应用D负责接受单播802.3协议报文*/
static int app_l2D_process()
{
	int s;
	unsigned short proto = ETH_P_802_3_BPDU;
	char buffer[2048];
	int offset;
	int frame_len;
	struct ethhdr header;
	struct sockaddr_ll from;
	int len = sizeof(struct sockaddr_ll);
	char if_name[IFNAMSIZ] = {'\0'};
	char desc[128];

	if((s = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_802_3_BPDU))) < 0) {
		printf("Error: could not open socket\n");
		return -1;
	}

	while(1){
		offset = 0;
		frame_len = recvfrom(s, buffer, 2048, 0, (struct sockaddr*)&from, &len);
		/*删除添加的ether type*/
		memcpy(&header, buffer, 12);
		offset += 2;
		frame_len -= 2;
		memcpy(buffer+offset, &header, 12);

		if(NULL == if_indextoname(from.sll_ifindex, if_name)){
			perror("if_indextoname failed");
			close(s);
			return -1;
		}
		sprintf(desc, "APP-L2D RX from %s", if_name);
		dump_frame(desc, buffer+offset, frame_len);
	}

	close(s);
	return 0;
}

/*
应用E负责定时发送广播eth II协议报文
APP-L2E TX G0.0 to G0.1, pkt length: 26
0xff 0xff 0xff 0xff 0xff 0xff 0x00 0xe0 0x0f 0x00 0x00 0x00 0x56 0x78 0x68 0x65 
0x6c 0x6c 0x6f 0x20 0x77 0x6f 0x72 0x6c 0x64 0x00 
SDK TX from intfV1, pkt length: 26
0xff 0xff 0xff 0xff 0xff 0xff 0x00 0xe0 0x0f 0x00 0x00 0x00 0x56 0x78 0x68 0x65 
0x6c 0x6c 0x6f 0x20 0x77 0x6f 0x72 0x6c 0x64 0x00 
SDK RX from G0.1, pkt length: 26
0xff 0xff 0xff 0xff 0xff 0xff 0x00 0xe0 0x0f 0x00 0x00 0x00 0x56 0x78 0x68 0x65 
0x6c 0x6c 0x6f 0x20 0x77 0x6f 0x72 0x6c 0x64 0x00 
APP-L2F RX G0.0 to G0.1, pkt length: 26
0xff 0xff 0xff 0xff 0xff 0xff 0x00 0xe0 0x0f 0x00 0x00 0x00 0x56 0x78 0x68 0x65 
0x6c 0x6c 0x6f 0x20 0x77 0x6f 0x72 0x6c 0x64 0x00 
*/
static int app_l2E_process()
{
	unsigned char source[ETH_ALEN];
    char dest[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    unsigned short proto = 0x5678;
    char *data = "hello world";
    unsigned short data_len = strlen(data)+1;
    int s;
	struct sockaddr_ll saddrll;
	struct ifreq buffer;
	int ifindex;
	union ethframe frame;
	unsigned int frame_len;

	if((s = socket(AF_PACKET, SOCK_RAW, htons(proto))) < 0) {
		printf("Error: could not open socket\n");
		return -1;
	}
	
	while(1){
		memset(&buffer, 0x00, sizeof(buffer));
		strncpy(buffer.ifr_name, "intfV1", IFNAMSIZ);
		if (ioctl(s, SIOCGIFINDEX, &buffer) < 0) {
			printf("Error: could not get interface index\n");
			close(s);
			return -1;
		}
		ifindex = buffer.ifr_ifindex;

		if (ioctl(s, SIOCGIFHWADDR, &buffer) < 0) {
			printf("Error: could not get interface address\n");
			close(s);
			return -1;
		}
		memcpy(source, buffer.ifr_hwaddr.sa_data, ETH_ALEN);

		memcpy(frame.field.header.h_dest, dest, ETH_ALEN);
		memcpy(frame.field.header.h_source, source, ETH_ALEN);
		frame.field.header.h_proto = htons(proto);
		memcpy(frame.field.data, data, data_len);
		frame_len = data_len + ETH_HLEN;
		dump_frame("APP-L2E TX from intfV1", frame.buffer, frame_len);
	    
		memset(&saddrll, 0, sizeof(saddrll));
		saddrll.sll_family = PF_PACKET;
		saddrll.sll_ifindex = ifindex;
		saddrll.sll_halen = ETH_ALEN;
		memcpy(saddrll.sll_addr, dest, ETH_ALEN);

		if (sendto(s, frame.buffer, frame_len, 0, (struct sockaddr*)&saddrll, sizeof(saddrll)) <= 0)
			printf("Error, could not send\n");

		sleep(5);
	}

	close(s);
	return 0;
}

/*应用F负责接受广播eth II协议协议报文*/
static int app_l2F_process()
{
	int s;
	unsigned short proto = 0x5678;
	char buffer[2048];
	int n_read;
	struct sockaddr_ll from;
	int len = sizeof(struct sockaddr_ll);
	char if_name[IFNAMSIZ] = {'\0'};
	char desc[128];

	if((s = socket(AF_PACKET, SOCK_RAW, htons(proto))) < 0) {
		printf("Error: could not open socket\n");
		return -1;
	}

	while(1){
		n_read = recvfrom(s, buffer, 2048, 0, (struct sockaddr*)&from, &len);
		if(NULL == if_indextoname(from.sll_ifindex, if_name)){
			perror("if_indextoname failed");
			close(s);
			return -1;
		}
		sprintf(desc, "APP-L2F RX from %s", if_name);
		dump_frame(desc, buffer, n_read);
	}

	close(s);
	return 0;
}


/*
应用A负责定时发送单播IP协议报文
APP-L3A TX, pkt length: 12
0x68 0x65 0x6c 0x6c 0x6f 0x20 0x77 0x6f 0x72 0x6c 0x64 0x00 
SDK TX from intfV1, pkt length: 42
0xff 0xff 0xff 0xff 0xff 0xff 0x00 0xe0 0x0f 0x00 0x22 0x00 0x08 0x06 0x00 0x01 
0x08 0x00 0x06 0x04 0x00 0x01 0x00 0xe0 0x0f 0x00 0x22 0x00 0x14 0x00 0x00 0x02 
0x00 0x00 0x00 0x00 0x00 0x00 0x0a 0x00 0x00 0x02 
SDK RX from G0.1, pkt length: 42
0xff 0xff 0xff 0xff 0xff 0xff 0x00 0xe0 0x0f 0x00 0x22 0x00 0x08 0x06 0x00 0x01 
0x08 0x00 0x06 0x04 0x00 0x01 0x00 0xe0 0x0f 0x00 0x22 0x00 0x14 0x00 0x00 0x02 
0x00 0x00 0x00 0x00 0x00 0x00 0x14 0x00 0x00 0x01 
SDK TX from intfV2, pkt length: 42
0x00 0xe0 0x0f 0x00 0x22 0x00 0x00 0xe0 0x0f 0x00 0x12 0x00 0x08 0x06 0x00 0x01 
0x08 0x00 0x06 0x04 0x00 0x02 0x00 0xe0 0x0f 0x00 0x12 0x00 0x0a 0x00 0x00 0x02 
0x00 0xe0 0x0f 0x00 0x22 0x00 0x14 0x00 0x00 0x02 
SDK RX from G0.0, pkt length: 42
0x00 0xe0 0x0f 0x00 0x00 0x00 0x00 0xe0 0x0f 0x00 0x12 0x00 0x08 0x06 0x00 0x01 
0x08 0x00 0x06 0x04 0x00 0x02 0x00 0xe0 0x0f 0x00 0x12 0x00 0x0a 0x00 0x00 0x02 
0x00 0xe0 0x0f 0x00 0x22 0x00 0x0a 0x00 0x00 0x01 
SDK TX from intfV1, pkt length: 46
0x00 0xe0 0x0f 0x00 0x12 0x00 0x00 0xe0 0x0f 0x00 0x22 0x00 0x08 0x00 0x45 0x00 
0x00 0x20 0xb4 0x61 0x40 0x00 0x40 0xfa 0x71 0x80 0x14 0x00 0x00 0x02 0x0a 0x00 
0x00 0x02 0x68 0x65 0x6c 0x6c 0x6f 0x20 0x77 0x6f 0x72 0x6c 0x64 0x00 
SDK RX from G0.1, pkt length: 46
0x00 0xe0 0x0f 0x00 0x00 0x00 0x00 0xe0 0x0f 0x00 0x22 0x00 0x08 0x00 0x45 0x00 
0x00 0x20 0xb4 0x61 0x40 0x00 0x40 0xfa 0x5d 0x80 0x14 0x00 0x00 0x02 0x14 0x00 
0x00 0x01 0x68 0x65 0x6c 0x6c 0x6f 0x20 0x77 0x6f 0x72 0x6c 0x64 0x00 
APP-L3B RX from 20.0.0.2, pkt length: 12
0x68 0x65 0x6c 0x6c 0x6f 0x20 0x77 0x6f 0x72 0x6c 0x64 0x00 
*/
static int app_l3A_process()
{
	unsigned char ipproto = 250;
    char *data = "hello world";
    unsigned short data_len = strlen(data)+1;
    int s;
	struct sockaddr_in din;

	if((s = socket(AF_INET, SOCK_RAW, ipproto)) < 0) {
		perror("Error: could not open socket");
		return -1;
	}
	
	while(1){
		memset(&din, 0, sizeof(din));
		din.sin_family = AF_INET;
		din.sin_addr.s_addr = inet_addr("10.0.0.2");
		dump_frame("APP-L3A TX", data, data_len);
		if (sendto(s, data, data_len, 0, (struct sockaddr *)&din, sizeof(din)) <= 0)
			perror("sendto() error");
		sleep(5);
	}

	close(s);
	return 0;
}

/*应用B负责接受单播IP协议报文*/
static int app_l3B_process()
{
	int s;
	unsigned char ipproto = 250;
	char buffer[2048];
	int n_read, offset;
	struct sockaddr_in from;
	int len = sizeof(struct sockaddr_in);
	char desc[128];
	struct in_addr addr;

	if((s = socket(AF_INET, SOCK_RAW, ipproto)) < 0) {
		perror("Error: could not open socket");
		return -1;
	}

	while(1){
		n_read = recvfrom(s, buffer, 2048, 0, (struct sockaddr *)&from, &len);
		offset = 20;
		memcpy(&addr, &from.sin_addr.s_addr, 4);
		sprintf(desc, "APP-L3B RX from %s", (char *)inet_ntoa(addr));
		dump_frame(desc, buffer + offset, n_read - offset);
	}

	close(s);
	return 0;
}

/*
应用C负责定时发送广播IP协议报文
APP-L3C TX, pkt length: 12
0x68 0x65 0x6c 0x6c 0x6f 0x20 0x77 0x6f 0x72 0x6c 0x64 0x00 
SDK TX from intfV1, pkt length: 46
0xff 0xff 0xff 0xff 0xff 0xff 0x00 0xe0 0x0f 0x00 0x22 0x00 0x08 0x00 0x45 0x00 
0x00 0x20 0x84 0x28 0x40 0x00 0x40 0xfb 0xa0 0xbb 0x14 0x00 0x00 0x02 0x0a 0xff 
0xff 0xff 0x68 0x65 0x6c 0x6c 0x6f 0x20 0x77 0x6f 0x72 0x6c 0x64 0x00 
APP-L3D RX from 10.0.0.1, pkt length: 12
SDK RX from G0.1, pkt length: 46
0xff 0xff 0xff 0xff 0xff 0xff 0x00 0xe0 0x0f 0x00 0x22 0x00 0x08 0x00 0x45 0x00 
0x00 0x20 0x84 0x28 0x40 0x00 0x40 0xfb 0x8c 0xba 0x14 0x00 0x00 0x02 0x14 0xff 
0xff 0xff 0x68 0x65 0x6c 0x6c 0x6f 0x20 0x77 0x6f 0x72 0x6c 0x64 0x00 
0x68 0x65 0x6c 0x6c 0x6f 0x20 0x77 0x6f 0x72 0x6c 0x64 0x00 
APP-L3D RX from 20.0.0.2, pkt length: 12
0x68 0x65 0x6c 0x6c 0x6f 0x20 0x77 0x6f 0x72 0x6c 0x64 0x00
*/
static int app_l3C_process()
{
	unsigned char ipproto = 251;
    char *data = "hello world";
    unsigned short data_len = strlen(data)+1;
    int s;
	struct sockaddr_in din;
	int on = 1;

	if((s = socket(AF_INET, SOCK_RAW, ipproto)) < 0) {
		perror("Error: could not open socket");
		return -1;
	}

	if(setsockopt(s, SOL_SOCKET, SO_BROADCAST, (char *)&on, sizeof(int)) < 0){
		perror("setsockopt SO_BROADCAST failed");
		close(s);
		return -1;
	}

	while(1){
		memset(&din, 0, sizeof(din));
		din.sin_family = AF_INET;
		din.sin_addr.s_addr = inet_addr("10.255.255.255");
		dump_frame("APP-L3C TX", data, data_len);
		if (sendto(s, data, data_len, 0, (struct sockaddr *)&din, sizeof(din)) <= 0)
			perror("sendto() error");
		sleep(5);
	}

	close(s);
	return 0;
}

/*应用D负责接受广播IP协议报文，有个广播回环的问题*/
static int app_l3D_process()
{
	int s;
	unsigned char ipproto = 251;
	char buffer[2048];
	int n_read, offset;
	struct sockaddr_in from;
	int len = sizeof(struct sockaddr_in);
	char desc[128];
	struct in_addr addr;

	if((s = socket(AF_INET, SOCK_RAW, ipproto)) < 0) {
		perror("Error: could not open socket");
		return -1;
	}

	while(1){
		n_read = recvfrom(s, buffer, 2048, 0, (struct sockaddr *)&from, &len);
		if(n_read < 0){
			perror("recvfrom failed");
			close(s);
			return -1;
		}
		offset = 20;
		memcpy(&addr, &from.sin_addr.s_addr, 4);
		sprintf(desc, "APP-L3D RX from %s", (char *)inet_ntoa(addr));
		dump_frame(desc, buffer + offset, n_read - offset);
	}

	close(s);
	return 0;
}

/*
应用E负责定时发送保留组播IP协议报文到指定intferface vlan
APP-L3E TX, pkt length: 12
0x68 0x65 0x6c 0x6c 0x6f 0x20 0x77 0x6f 0x72 0x6c 0x64 0x00 
APP-L3F RX from 10.0.0.1, pkt length: 12
0x68 0x65 0x6c 0x6c 0x6f 0x20 0x77 0x6f 0x72 0x6c 0x64 0x00 
SDK TX from intfV1, pkt length: 46
0x01 0x00 0x5e 0x00 0x00 0x01 0x00 0xe0 0x0f 0x00 0x22 0x00 0x08 0x00 0x45 0x00 
0x00 0x20 0x7b 0x8b 0x40 0x00 0x01 0xfc 0x13 0x55 0x14 0x00 0x00 0x02 0xe0 0x00 
0x00 0x01 0x68 0x65 0x6c 0x6c 0x6f 0x20 0x77 0x6f 0x72 0x6c 0x64 0x00 
SDK RX from G0.1, pkt length: 46
0x01 0x00 0x5e 0x00 0x00 0x01 0x00 0xe0 0x0f 0x00 0x22 0x00 0x08 0x00 0x45 0x00 
0x00 0x20 0x7b 0x8b 0x40 0x00 0x01 0xfc 0x09 0x54 0x14 0x00 0x00 0x02 0xe0 0x00 
0x00 0x01 0x68 0x65 0x6c 0x6c 0x6f 0x20 0x77 0x6f 0x72 0x6c 0x64 0x00 
APP-L3F RX from 20.0.0.2, pkt length: 12
0x68 0x65 0x6c 0x6c 0x6f 0x20 0x77 0x6f 0x72 0x6c 0x64 0x00
*/
static int app_l3E_process()
{
	unsigned char ipproto = 252;
    char *data = "hello world";
    unsigned short data_len = strlen(data)+1;
    int s;
	struct sockaddr_in localaddr, din;
	int on = 1;

	if((s = socket(AF_INET, SOCK_RAW, ipproto)) < 0) {
		perror("Error: could not open socket");
		return -1;
	}

	localaddr.sin_family = AF_INET;
    localaddr.sin_addr.s_addr = inet_addr("10.0.0.1");
    if(bind(s, (struct sockaddr *)&localaddr, sizeof(localaddr)) < 0){
		perror("socket bind failed");
		close(s);
		return -1;
	}

	while(1){
		memset(&din, 0, sizeof(din));
		din.sin_family = AF_INET;
		din.sin_addr.s_addr = inet_addr("224.0.0.1");
		dump_frame("APP-L3E TX", data, data_len);
		if (sendto(s, data, data_len, 0, (struct sockaddr *)&din, sizeof(din)) <= 0)
			perror("sendto() error");
		sleep(5);
	}

	close(s);
	return 0;
}

/*应用F负责接受保留组播IP协议报文*/
static int app_l3F_process()
{
	int s;
	unsigned char ipproto = 252;
	char buffer[2048];
	int n_read, offset;
	struct sockaddr_in from;
	int len = sizeof(struct sockaddr_in);
	char desc[128];
	struct in_addr addr;

	if((s = socket(AF_INET, SOCK_RAW, ipproto)) < 0) {
		perror("Error: could not open socket");
		return -1;
	}

	while(1){
		n_read = recvfrom(s, buffer, 2048, 0, (struct sockaddr *)&from, &len);
		if(n_read < 0){
			perror("recvfrom failed");
			close(s);
			return -1;
		}
		offset = 20;
		memcpy(&addr, &from.sin_addr.s_addr, 4);
		sprintf(desc, "APP-L3F RX from %s", (char *)inet_ntoa(addr));
		dump_frame(desc, buffer + offset, n_read - offset);
	}

	close(s);
	return 0;
}


/*
应用A负责定时发送单播TCP应用报文
APP-L4B bind finsihed
APP-L4B listen finsihed
SDK TX from intfV1, pkt length: 42
0xff 0xff 0xff 0xff 0xff 0xff 0x00 0xe0 0x0f 0x00 0x22 0x00 0x08 0x06 0x00 0x01 
0x08 0x00 0x06 0x04 0x00 0x01 0x00 0xe0 0x0f 0x00 0x22 0x00 0x14 0x00 0x00 0x02 
0x00 0x00 0x00 0x00 0x00 0x00 0x0a 0x00 0x00 0x02 
SDK RX from G0.1, pkt length: 42
0xff 0xff 0xff 0xff 0xff 0xff 0x00 0xe0 0x0f 0x00 0x22 0x00 0x08 0x06 0x00 0x01 
0x08 0x00 0x06 0x04 0x00 0x01 0x00 0xe0 0x0f 0x00 0x22 0x00 0x14 0x00 0x00 0x02 
0x00 0x00 0x00 0x00 0x00 0x00 0x14 0x00 0x00 0x01 
SDK TX from intfV2, pkt length: 42
0x00 0xe0 0x0f 0x00 0x22 0x00 0x00 0xe0 0x0f 0x00 0x12 0x00 0x08 0x06 0x00 0x01 
0x08 0x00 0x06 0x04 0x00 0x02 0x00 0xe0 0x0f 0x00 0x12 0x00 0x0a 0x00 0x00 0x02 
0x00 0xe0 0x0f 0x00 0x22 0x00 0x14 0x00 0x00 0x02 
SDK RX from G0.0, pkt length: 42
0x00 0xe0 0x0f 0x00 0x00 0x00 0x00 0xe0 0x0f 0x00 0x12 0x00 0x08 0x06 0x00 0x01 
0x08 0x00 0x06 0x04 0x00 0x02 0x00 0xe0 0x0f 0x00 0x12 0x00 0x0a 0x00 0x00 0x02 
0x00 0xe0 0x0f 0x00 0x22 0x00 0x0a 0x00 0x00 0x01 
SDK TX from intfV1, pkt length: 74
0x00 0xe0 0x0f 0x00 0x12 0x00 0x00 0xe0 0x0f 0x00 0x22 0x00 0x08 0x00 0x45 0x00 
0x00 0x3c 0x82 0x46 0x40 0x00 0x40 0x06 0xa4 0x73 0x14 0x00 0x00 0x02 0x0a 0x00 
0x00 0x02 0xb2 0xd5 0x55 0xaa 0x62 0x3b 0x23 0x55 0x00 0x00 0x00 0x00 0xa0 0x02 
0x72 0x10 0x5b 0x30 0x00 0x00 0x02 0x04 0x05 0xb4 0x04 0x02 0x08 0x0a 0x37 0x99 
0xa1 0x13 0x00 0x00 0x00 0x00 0x01 0x03 0x03 0x07 
SDK RX from G0.1, pkt length: 74
0x00 0xe0 0x0f 0x00 0x00 0x00 0x00 0xe0 0x0f 0x00 0x22 0x00 0x08 0x00 0x45 0x00 
0x00 0x3c 0x82 0x46 0x40 0x00 0x40 0x06 0x90 0x73 0x14 0x00 0x00 0x02 0x14 0x00 
0x00 0x01 0xb2 0xd5 0x55 0xaa 0x62 0x3b 0x23 0x55 0x00 0x00 0x00 0x00 0xa0 0x02 
0x72 0x10 0x47 0x30 0x00 0x00 0x02 0x04 0x05 0xb4 0x04 0x02 0x08 0x0a 0x37 0x99 
0xa1 0x13 0x00 0x00 0x00 0x00 0x01 0x03 0x03 0x07 
SDK TX from intfV2, pkt length: 74
0x00 0xe0 0x0f 0x00 0x22 0x00 0x00 0xe0 0x0f 0x00 0x12 0x00 0x08 0x00 0x45 0x00 
0x00 0x3c 0x00 0x00 0x40 0x00 0x40 0x06 0x12 0xba 0x0a 0x00 0x00 0x02 0x14 0x00 
0x00 0x02 0x55 0xaa 0xb2 0xd5 0x08 0x38 0x5d 0x7f 0x62 0x3b 0x23 0x56 0xa0 0x12 
0x71 0x20 0x09 0xab 0x00 0x00 0x02 0x04 0x05 0xb4 0x04 0x02 0x08 0x0a 0x37 0x99 
0xa1 0x13 0x37 0x99 0xa1 0x13 0x01 0x03 0x03 0x07 
SDK RX from G0.0, pkt length: 74
0x00 0xe0 0x0f 0x00 0x00 0x00 0x00 0xe0 0x0f 0x00 0x12 0x00 0x08 0x00 0x45 0x00 
0x00 0x3c 0x00 0x00 0x40 0x00 0x40 0x06 0x26 0xba 0x0a 0x00 0x00 0x02 0x0a 0x00 
0x00 0x01 0x55 0xaa 0xb2 0xd5 0x08 0x38 0x5d 0x7f 0x62 0x3b 0x23 0x56 0xa0 0x12 
0x71 0x20 0x1d 0xab 0x00 0x00 0x02 0x04 0x05 0xb4 0x04 0x02 0x08 0x0a 0x37 0x99 
0xa1 0x13 0x37 0x99 0xa1 0x13 0x01 0x03 0x03 0x07 
SDK TX from intfV1, pkt length: 66
0x00 0xe0 0x0f 0x00 0x12 0x00 0x00 0xe0 0x0f 0x00 0x22 0x00 0x08 0x00 0x45 0x00 
0x00 0x34 0x82 0x47 0x40 0x00 0x40 0x06 0xa4 0x7a 0x14 0x00 0x00 0x02 0x0a 0x00 
0x00 0x02 0xb2 0xd5 0x55 0xaa 0x62 0x3b 0x23 0x56 0x08 0x38 0x5d 0x80 0x80 0x10 
0x00 0xe5 0xbc 0xb2 0x00 0x00 0x01 0x01 0x08 0x0a 0x37 0x99 0xa1 0x13 0x37 0x99 
0xa1 0x13 
APP-L4A connect finsihed
APP-L4A TX, pkt length: 12
0x68 0x65 0x6c 0x6c 0x6f 0x20 0x77 0x6f 0x72 0x6c 0x64 0x00 
SDK RX from G0.1, pkt length: 66
0x00 0xe0 0x0f 0x00 0x00 0x00 0x00 0xe0 0x0f 0x00 0x22 0x00 0x08 0x00 0x45 0x00 
0x00 0x34 0x82 0x47 0x40 0x00 0x40 0x06 0x90 0x7a 0x14 0x00 0x00 0x02 0x14 0x00 
0x00 0x01 0xb2 0xd5 0x55 0xaa 0x62 0x3b 0x23 0x56 0x08 0x38 0x5d 0x80 0x80 0x10 
0x00 0xe5 0xa8 0xb2 0x00 0x00 0x01 0x01 0x08 0x0a 0x37 0x99 0xa1 0x13 0x37 0x99 
0xa1 0x13 
APP-L4B accept finsihed
SDK TX from intfV1, pkt length: 78
0x00 0xe0 0x0f 0x00 0x12 0x00 0x00 0xe0 0x0f 0x00 0x22 0x00 0x08 0x00 0x45 0x00 
0x00 0x40 0x82 0x48 0x40 0x00 0x40 0x06 0xa4 0x6d 0x14 0x00 0x00 0x02 0x0a 0x00 
0x00 0x02 0xb2 0xd5 0x55 0xaa 0x62 0x3b 0x23 0x56 0x08 0x38 0x5d 0x80 0x80 0x18 
0x00 0xe5 0x2a 0xd0 0x00 0x00 0x01 0x01 0x08 0x0a 0x37 0x99 0xa1 0x13 0x37 0x99 
0xa1 0x13 0x68 0x65 0x6c 0x6c 0x6f 0x20 0x77 0x6f 0x72 0x6c 0x64 0x00 
SDK RX from G0.1, pkt length: 78
0x00 0xe0 0x0f 0x00 0x00 0x00 0x00 0xe0 0x0f 0x00 0x22 0x00 0x08 0x00 0x45 0x00 
0x00 0x40 0x82 0x48 0x40 0x00 0x40 0x06 0x90 0x6d 0x14 0x00 0x00 0x02 0x14 0x00 
0x00 0x01 0xb2 0xd5 0x55 0xaa 0x62 0x3b 0x23 0x56 0x08 0x38 0x5d 0x80 0x80 0x18 
0x00 0xe5 0x16 0xd0 0x00 0x00 0x01 0x01 0x08 0x0a 0x37 0x99 0xa1 0x13 0x37 0x99 
0xa1 0x13 0x68 0x65 0x6c 0x6c 0x6f 0x20 0x77 0x6f 0x72 0x6c 0x64 0x00 
APP-L4B RX, pkt length: 12
0x68 0x65 0x6c 0x6c 0x6f 0x20 0x77 0x6f 0x72 0x6c 0x64 0x00 
SDK TX from intfV2, pkt length: 66
0x00 0xe0 0x0f 0x00 0x22 0x00 0x00 0xe0 0x0f 0x00 0x12 0x00 0x08 0x00 0x45 0x00 
0x00 0x34 0xfc 0x93 0x40 0x00 0x40 0x06 0x16 0x2e 0x0a 0x00 0x00 0x02 0x14 0x00 
0x00 0x02 0x55 0xaa 0xb2 0xd5 0x08 0x38 0x5d 0x80 0x62 0x3b 0x23 0x62 0x80 0x10 
0x00 0xe3 0xa8 0xa8 0x00 0x00 0x01 0x01 0x08 0x0a 0x37 0x99 0xa1 0x13 0x37 0x99 
0xa1 0x13 
SDK RX from G0.0, pkt length: 66
0x00 0xe0 0x0f 0x00 0x00 0x00 0x00 0xe0 0x0f 0x00 0x12 0x00 0x08 0x00 0x45 0x00 
0x00 0x34 0xfc 0x93 0x40 0x00 0x40 0x06 0x2a 0x2e 0x0a 0x00 0x00 0x02 0x0a 0x00 
0x00 0x01 0x55 0xaa 0xb2 0xd5 0x08 0x38 0x5d 0x80 0x62 0x3b 0x23 0x62 0x80 0x10 
0x00 0xe3 0xbc 0xa8 0x00 0x00 0x01 0x01 0x08 0x0a 0x37 0x99 0xa1 0x13 0x37 0x99 
0xa1 0x13 
*/
static int app_l4A_process()
{
	unsigned short port = 0x55aa;
    char *data = "hello world";
    unsigned short data_len = strlen(data)+1;
    int sServer;
	struct sockaddr_in addrServ;
	int rv;

	if((sServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		perror("Error: could not open socket");
		return -1;
	}

	addrServ.sin_family = AF_INET;
	addrServ.sin_port = htons(port);
	addrServ.sin_addr.s_addr = inet_addr("10.0.0.2");
	rv = connect(sServer, (struct sockaddr *)&addrServ, sizeof(struct sockaddr));  
	if (rv < 0)
	{
		perror("socket connect failed");
		close(sServer);
		return -1;
	}
	printf("APP-L4A connect finsihed\r\n");
	
	while(1){
		dump_frame("APP-L4A TX", data, data_len);
		rv = send(sServer, data, data_len, 0);
		if (rv < 0)
		{
			perror("socket send failed");
			close(sServer);
			return -1;
		}
		sleep(5);
	}

	close(sServer);
	return 0;
}

/*应用B负责接受单播TCP应用报文*/
static int app_l4B_process()
{
	int sServer, sClient;
	unsigned short port = 0x55aa;
	char buffer[2048];
	int n_read;
	struct sockaddr_in addrServ, addrClient;	
	int addrClientLen = sizeof(struct sockaddr_in);
	int rv;
	int on = 1;

	if((sServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		perror("Error: could not open socket");
		return -1;
	}

	if(setsockopt(sServer, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int)) < 0){
		perror("setsockopt SO_REUSEADDR failed");
		close(sServer);
		return -1;
	}
	addrServ.sin_family = AF_INET;  
	addrServ.sin_port = htons(port);
	addrServ.sin_addr.s_addr = inet_addr("20.0.0.1");
	rv = bind(sServer, (struct sockaddr *)&addrServ, sizeof(struct sockaddr));
	if (rv < 0)
	{
		perror("socket bind failed");
		close(sServer);
		return -1;
	}
	printf("APP-L4B bind finsihed\r\n");
	
	rv = listen(sServer, 1);
	if (rv < 0)
	{
		perror("socket listen failed");
		close(sServer);
		return -1;
	}
	printf("APP-L4B listen finsihed\r\n");

	sClient = accept(sServer, (struct sockaddr*)&addrClient, &addrClientLen);
	if(sClient < 0){
		perror("socket accept failed");
		close(sServer);
		return -1;
	}
	printf("APP-L4B accept finsihed\r\n");

	while(1){
		n_read = recv(sClient, buffer, 2048, 0);
		dump_frame("APP-L4B RX", buffer, n_read);
	}

	close(sServer);
	close(sClient);
	return 0;
}

/*
应用C负责定时发送广播UDP应用报文
APP-L4C TX, pkt length: 12
0x68 0x65 0x6c 0x6c 0x6f 0x20 0x77 0x6f 0x72 0x6c 0x64 0x00 
APP-L4D RX from 10.0.0.1, pkt length: 12
0x68 0x65 0x6c 0x6c 0x6f 0x20 0x77 0x6f 0x72 0x6c 0x64 0x00 
SDK TX from intfV1, pkt length: 54
0xff 0xff 0xff 0xff 0xff 0xff 0x00 0xe0 0x0f 0x00 0x22 0x00 0x08 0x00 0x45 0x00 
0x00 0x28 0x2d 0x63 0x40 0x00 0x40 0x11 0xf8 0x62 0x14 0x00 0x00 0x02 0x0a 0xff 
0xff 0xff 0xcf 0xe1 0x55 0xbb 0x00 0x14 0x33 0x5b 0x68 0x65 0x6c 0x6c 0x6f 0x20 
0x77 0x6f 0x72 0x6c 0x64 0x00 
SDK RX from G0.1, pkt length: 54
0xff 0xff 0xff 0xff 0xff 0xff 0x00 0xe0 0x0f 0x00 0x22 0x00 0x08 0x00 0x45 0x00 
0x00 0x28 0x2d 0x63 0x40 0x00 0x40 0x11 0xe4 0x61 0x14 0x00 0x00 0x02 0x14 0xff 
0xff 0xff 0xcf 0xe1 0x55 0xbb 0x00 0x14 0x1f 0x5a 0x68 0x65 0x6c 0x6c 0x6f 0x20 
0x77 0x6f 0x72 0x6c 0x64 0x00 
APP-L4D RX from 20.0.0.2, pkt length: 12
0x68 0x65 0x6c 0x6c 0x6f 0x20 0x77 0x6f 0x72 0x6c 0x64 0x00
*/
static int app_l4C_process()
{
	unsigned short port = 0x55bb;
    char *data = "hello world";
    unsigned short data_len = strlen(data)+1;
    int s;
	struct sockaddr_in din;
	int rv;
	int on = 1;
	int n_write;

	if((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("Error: could not open socket");
		return -1;
	}

	if(setsockopt(s, SOL_SOCKET, SO_BROADCAST, (char *)&on, sizeof(int)) < 0){
		perror("setsockopt SO_BROADCAST failed");
		close(s);
		return -1;
	}

	memset(&din, 0, sizeof(din));
	din.sin_family = AF_INET;
	din.sin_port = htons(port);
	din.sin_addr.s_addr = inet_addr("10.255.255.255");
	while(1){
		dump_frame("APP-L4C TX", data, data_len);
		n_write = sendto(s, data, data_len, 0, (struct sockaddr *)&din, sizeof(din));
		if (n_write <= 0){
			perror("socket sendto failed");
			close(s);
			return -1;
		}
		sleep(5);
	}

	close(s);
	return 0;
}

/*应用D负责接受广播UDP应用报文*/
static int app_l4D_process()
{
	int s;
	unsigned short port = 0x55bb;
	char buffer[2048];
	int n_read;
	struct sockaddr_in din;
	int rv;
	struct sockaddr_in from;
	int len = sizeof(struct sockaddr_in);
	char desc[128];
	struct in_addr addr;

	if((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("Error: could not open socket");
		return -1;
	}

	din.sin_family = AF_INET;  
	din.sin_port = htons(port);
	din.sin_addr.s_addr = INADDR_ANY;
	rv = bind(s, (struct sockaddr *)&din, sizeof(struct sockaddr));
	if (rv < 0)
	{
		perror("socket bind failed");
		close(s);
		return -1;
	}

	while(1){
		n_read = recvfrom(s, buffer, 2048, 0, (struct sockaddr *)&from, &len);
		if(n_read < 0){
			perror("recvfrom failed");
			close(s);
			return -1;
		}
		memcpy(&addr, &from.sin_addr.s_addr, 4);
		sprintf(desc, "APP-L4D RX from %s", (char *)inet_ntoa(addr));
		dump_frame(desc, buffer, n_read);
	}

	close(s);
	return 0;
}

int my20()
{
	char cmd[128];
	int panel_port, intf_vlan;
	char tap_name[16];
	unsigned char mac[6] = {0x00, 0xe0, 0x0f, 0x00, 0x00, 0x00};
	pthread_t tid[2] = {-1};
	int i;
	
	/*创建2层内核端口G0.0-1*/
	for(panel_port=0;panel_port<2;panel_port++)
	{
		snprintf(tap_name, sizeof(tap_name), "G0.%d", panel_port);
		
		panel_port_tap_fd[panel_port] = swp_util_alloc_tap(tap_name);
		if(panel_port_tap_fd[panel_port] < 0)
			return -1;
		
		sprintf(cmd, "ifconfig %s hw ether %02x:%02x:%02x:%02x:%02x:%02x", 
				tap_name, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]+panel_port+1);
		system(cmd);
		sprintf(cmd, "ifconfig %s promisc", tap_name);
		system(cmd);
	}

	/*打开G0.0-1*/
	for(panel_port=0;panel_port<2;panel_port++)
	{
		snprintf(tap_name, sizeof(tap_name), "G0.%d", panel_port);
		sprintf(cmd, "ifconfig %s up", tap_name);
		system(cmd);
	}

	/*创建3层内核端口intfV1-2*/
	for(intf_vlan=1;intf_vlan<=2;intf_vlan++)
	{
		snprintf(tap_name, sizeof(tap_name), "intfV%d", intf_vlan);
		
		intf_vlan_tap_fd[intf_vlan] = swp_util_alloc_tap(tap_name);
		if(intf_vlan_tap_fd[intf_vlan] < 0)
			return -1;
		sprintf(cmd, "ifconfig %s hw ether %02x:%02x:%02x:%02x:%02x:%02x", 
				tap_name, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		system(cmd);
		system(cmd);
		sprintf(cmd, "ifconfig %s promisc", tap_name);
		system(cmd);
	}
	system("ifconfig intfV1 10.0.0.1 netmask 255.0.0.0");
	system("ifconfig intfV2 20.0.0.1 netmask 255.0.0.0");

	/*打开intfV1-2*/
	for(intf_vlan=1;intf_vlan<=2;intf_vlan++)
	{
		snprintf(tap_name, sizeof(tap_name), "intfV%d", intf_vlan);
		sprintf(cmd, "ifconfig %s up", tap_name);
		system(cmd);
	}

	/*创建SDK发送线程*/
	pthread_create(&tid[0], NULL, packet_send_thread_init, NULL);

	/*创建SDK接受线程*/
	pthread_create(&tid[1], NULL, packet_recv_thread_init, NULL);

	/*创建应用进程*/
	for(i=0;i<sizeof(processes)/sizeof(subprocess_t);i++){
		processes[i].pid = fork();
		if(processes[i].pid == -1){
			//父进程创建子进程失败
			printf("fork %s failed\r\n", processes[i].name);
			goto exit;
		}else if(processes[i].pid == 0){
			//子进程上下文
			prctl(PR_SET_PDEATHSIG,SIGKILL);	/*父进程退出时，会收到SIGKILL信号*/
			if(!processes[i].func)
				return 0;
			int rc;
			char name[128];
			cpu_set_t mask;
			sprintf(name, "my%s", processes[i].name);
			prctl(PR_SET_NAME, name);
			CPU_ZERO(&mask);
			CPU_SET(7, &mask);
			sched_setaffinity(0, sizeof(mask), &mask);
#if 0
			param.sched_priority = my9_subprocesses[i].priority;
			rc = sched_setscheduler(0, SCHED_FIFO, &param);
			if(rc <0)
				printf("sched_setscheduler failed %d\r\n", rc);
#else
			nice(processes[i].priority);
#endif
			processes[i].func(0);
			while(1){
				sleep(1);
			}
			return 0;
		}
	}

exit:
	for(i=0;i<sizeof(processes)/sizeof(subprocess_t);i++){
		if(processes[i].pid == -1)
			continue;
		waitpid(processes[i].pid, NULL, 0);
		processes[i].pid = -1;
	}

	for(i=0;i<2;i++){
		if(tid[i] == -1)
			continue;
		pthread_join(tid[i], NULL);
	}

	return 0;
}



