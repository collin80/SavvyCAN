#ifndef PCAPLITE_H
#define PCAPLITE_H

#include <stdio.h>
#if defined(unix)
#include <sys/time.h>
#else
#include <winsock.h>
#endif

#define PCAP_ERRBUF_SIZE 256

struct pcap_pkthdr {
	struct timeval ts;	/* time stamp */
	unsigned int caplen;	/* length of portion present */
	unsigned int len;	/* length of this packet (off wire) */
};

struct pcap {
    FILE *file;
    bool is_ng;
};

typedef struct pcap pcap_t;

pcap *pcap_open_offline(const char *, char *);

const unsigned char *pcap_next(pcap_t *, struct pcap_pkthdr *);

void pcap_close(pcap_t *);

#endif// PCAPLITE_H
