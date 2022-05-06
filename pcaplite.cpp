#include <math.h>
#include "pcaplite.h"

#define MAGIC_NG 0x0A0D0D0A
#define MACIG 0xA1B2C3D4  

#define MAX_CAN_PACKET_SIZE 256

// some pcap format constants
#define PCAP_FILE_HEADER_LENGTH 24
#define PCAP_FRAME_HEADER_LENGTH 16
#define PCAP_CAP_FRAME_LENGTH_OFFSET 8
#define PCAP_FRAME_LENGTH_OFFSET 12

// some pcapng format constants
#define INTERFACE_DESCRITION_BLOCK 0x01
#define ENCHANCED_PACKET_BLOCK 0x06

static unsigned char pcap_buffer[MAX_CAN_PACKET_SIZE];
static pcap_t p;

pcap *pcap_open_offline(const char *filename, char *error_text) {
	FILE *file;

    snprintf(error_text, PCAP_ERRBUF_SIZE, "OK");

    file = fopen(filename,"rb");

    if (NULL == file) {
        snprintf(error_text, PCAP_ERRBUF_SIZE,
               "Cannot open input file");
        return NULL;
    }

	unsigned int magic;
	size_t bytes_read;

	bytes_read = fread(&magic, 1, sizeof(magic), file);
	if (bytes_read != sizeof(magic)) {
		snprintf(error_text, PCAP_ERRBUF_SIZE, "Cannot read magic word");
        fclose(file);
		return (NULL);
	}

    if (magic != MACIG && magic != MAGIC_NG) {
        snprintf(error_text, PCAP_ERRBUF_SIZE, "Not a supported format %04x", magic);
        fclose(file);
		return (NULL);
    }

    // set the format
    // and seek past file header
    if (MAGIC_NG == magic) {
        p.is_ng = 1;
        fseek(file, sizeof(magic), SEEK_SET);
        unsigned int  section_length;
        bytes_read = fread(&section_length, 1, sizeof(section_length), file);
        if (bytes_read != sizeof(section_length)) {
		    snprintf(error_text, PCAP_ERRBUF_SIZE, "Cannot read section length");
            fclose(file);
		    return (NULL);
	    }
        fseek(file, section_length, SEEK_SET);
        
    } else {
        p.is_ng = 0;
        fseek(file, PCAP_FILE_HEADER_LENGTH, SEEK_SET);
    }

    p.file = file;
	
	return(&p);
}

const unsigned char *pcap_next_ng(pcap_t *p, struct pcap_pkthdr *h) {
    // this is for ENCHANCED_PACKET_BLOCK
    struct block_header { 
        unsigned int block_type;
        unsigned int block_size;
        unsigned int interface_id;
        unsigned int timestamp_hi;
        unsigned int timestamp_lo;
        unsigned int cap_len;
        unsigned int len;
    } bh;

    struct option_header {
        unsigned short option_type;
        unsigned short option_length;
    } oh;

    size_t bytes_read;

    static double timestamp_multiplier = 0.000001; //microseconds resolution

    long fpos = ftell(p->file);
    
    do {
        bytes_read = fread(&bh, 1, sizeof(bh), p->file);
        if (bytes_read != sizeof(bh)) {
            //probably EOF
            return (NULL);
        }

        if (bh.block_type != ENCHANCED_PACKET_BLOCK) {
            if (INTERFACE_DESCRITION_BLOCK == bh.block_type) {
                // Seek to start of options part in the header
                fseek(p->file, fpos + 16, SEEK_SET);
                do {
                    bytes_read = fread(&oh, 1, sizeof(oh), p->file);
                    if (bytes_read != sizeof(oh)) {
                        //probably EOF
                        return (NULL);
                    }
                    
                    // calcualte option length aligned on 4 byte boundary
                    unsigned int option_file_length = 4*(oh.option_length/4) + ((oh.option_length%4) ? 4:0);

                    
                    // read in the buffer if it's large enough or skip
                    if (option_file_length <= sizeof(pcap_buffer)) {
                        bytes_read = fread(pcap_buffer, 1, option_file_length, p->file);
                        if (bytes_read != option_file_length) {
                            //probably EOF
		                    return (NULL);
	                    }
                        //check if time resolution option
                        if (0x09 == oh.option_type) {
                            unsigned int res = *(unsigned int*)pcap_buffer;
                            if ((0x80000000 & res) == 0) {
                                timestamp_multiplier = 1/pow(10, res); 
                            } else {
                               timestamp_multiplier = 1/pow(2, (res & 0x7fffffff));  
                            }
                        }
                    } else if (fseek(p->file, option_file_length, SEEK_CUR)) {
                        //probably EOF
		                return (NULL);
                    }

                } while(ftell(p->file) < fpos + bh.block_size - sizeof(bh.block_size));

                if (fseek(p->file, sizeof(bh.block_size), SEEK_CUR)) {
                        //probably EOF
		                return (NULL);
                }
            }

            // skip to the next block
            if (fseek(p->file, fpos + bh.block_size, SEEK_SET)) {
                //probably EOF
		        return (NULL);
            }
            fpos = ftell(p->file);
        }
    } while (bh.block_type != ENCHANCED_PACKET_BLOCK);

    h->caplen = bh.cap_len;
    
    double timestamp = ((unsigned long long)bh.timestamp_hi << 32 | bh.timestamp_lo) * timestamp_multiplier;
    double fractional, integer;

    fractional = modf(timestamp, &integer);
    h->ts.tv_sec = (long)integer;
    h->ts.tv_usec = (long)(fractional * 1000000);
    
    bytes_read = fread(pcap_buffer, 1, h->caplen, p->file);
    if (bytes_read != h->caplen) {
        //probably EOF
		return (NULL);
	}
   
    //seek to the next block (go back with bytes read of the block and seek fwd with the block size)
    if (fseek(p->file, fpos + bh.block_size, SEEK_SET)) {
        //probably EOF
		return (NULL);
    }

    return pcap_buffer;
}


const unsigned char *pcap_next(pcap_t *p, struct pcap_pkthdr *h)
{
    if (p->is_ng) {
        return pcap_next_ng(p, h);
    }

	size_t bytes_read;

	bytes_read = fread(pcap_buffer, 1, PCAP_FRAME_HEADER_LENGTH, p->file);
    if (bytes_read != PCAP_FRAME_HEADER_LENGTH) {
        //probably EOF
		return (NULL);
	}

    h->ts.tv_sec = *(unsigned int*)(pcap_buffer);
    h->ts.tv_usec = *(unsigned int*)(pcap_buffer + 4);

    h->caplen = *(unsigned int*)(pcap_buffer + PCAP_CAP_FRAME_LENGTH_OFFSET);
    h->len = *(unsigned int*)(pcap_buffer + PCAP_FRAME_LENGTH_OFFSET);

    bytes_read = fread(pcap_buffer, 1, h->caplen, p->file);
    if (bytes_read != h->caplen) {
        //probably EOF
		return (NULL);
	}

    return pcap_buffer;
}

void pcap_close(pcap_t *p) {
    fclose(p->file);
}
