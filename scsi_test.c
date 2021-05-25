
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <stdint.h>
#include <getopt.h>

#define MIN_BLOCK_LEN           512
#define OP6_CMD_LEN             6
#define OP10_CMD_LEN            10
#define SENSE_BUF_LEN           32

#define BLOCK_MAX               65535
#define LBA_MAX                 21474023647

#define CMD_INQUIRY             0x12
#define CMD_WRITE10             0x2A
#define CMD_READ10              0x28

#define PART_TABLE_MBR_OFF      0x1BE

int lba = 0;
unsigned no_of_blocks = 1;
unsigned max_blocks = 1;
int test_count = 1;
int verbose = 0;
int parse_mbr = 0;
int test_inquiry = 0;
int test_write = 0;
int test_read = 0;
char *device_name = NULL;

void hexDump(char *desc, void *addr, int len, int offset)
{
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;

    if (desc != NULL)
    {
        printf ("%s:\n", desc);
    }

    for (i = 0; i < len; i++)
    {
        if ((i % 16) == 0)
        {
            if (i != 0)
            {
                printf("  %s\n", buff);
            }
            printf("%08x ", i + (offset * 0x200));
        }
        printf(" %02x", pc[i]);
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
        {
            buff[i % 16] = '.';
        }
        else
        {
            buff[i % 16] = pc[i];
        }

        buff[(i % 16) + 1] = '\0';
    }
    while ((i % 16) != 0)
    {
        printf("   ");
        i++;
    }
    printf("  %s\n", buff);
    fflush(stdout);
}

void print_help()
{
    fprintf(stdout, "scsi_test [-l LBAVAL] [-b BLOCKS] [-m MAXBLOCKS [-c COUNT]] [-s] [-v] -d DEVICE\n"
    "\t-l, --lba VAL - logical block address\n"
    "\t-b, --blocks VAL - number of blocks\n"
    "\t-m, --maxblocks VAL - max number of blocks\n"
    "\t-c, --count VAL - test count\n"
    "\t-v, --verbose - verbose execution\n"
    "\t-t, --table - show partition table\n"
    "\t-i, --inquiry - test inquiry\n"
    "\t-w, --write - test write\n"
    "\t-r, --read - test read\n"
    "\t-d, --device - device node (/dev/sgX)\n"
    "\t-h, --help - print this message\n");
    exit(1);
}

void process_args(int argc, char** argv)
{
    const char* const short_opts = "l:b:m:c:vhd:tiwr";
    const struct option long_opts[] = {
        {"lba",         required_argument,  NULL, 'l'},
        {"blocks",      required_argument,  NULL, 'b'},
        {"maxblocks",   required_argument,  NULL, 'm'},
        {"count",       required_argument,  NULL, 'c'},
        {"verbose",     no_argument,        NULL, 'v'},
        {"write",       no_argument,        NULL, 'w'},
        {"inquiry",     no_argument,        NULL, 'i'},
        {"read",        no_argument,        NULL, 'r'},
        {"table",       required_argument,  NULL, 't'},
        {"device",      required_argument,  NULL, 'd'},
        {"help",        no_argument,        NULL, 'h'},
        {NULL,          0,                  NULL,  0}
    };

    int rez;
    int option_index;

    while ((rez=getopt_long(argc,argv, short_opts,
        long_opts, &option_index))!=-1)
    {

        switch(rez)
        {
            case 'l':
            {
                lba = atol(optarg);
                printf("lba: %i\n", lba);
                break;
            }
            case 'b':
            {
                no_of_blocks = atol(optarg);
                printf("no_of_blocks: %i\n", no_of_blocks);
                break;
            }
            case 'm':
            {
                max_blocks = atol(optarg);
                printf("maxblocks: %i\n", max_blocks);
                break;
            }
            case 'c':
            {
                test_count = atol(optarg);
                printf("test_count: %i\n", test_count);
                break;
            }
            case 'v':
            {
                verbose = 1;
                printf("verbose: %i\n", verbose);
                break;
            }
            case 'd':
            {
                device_name = strdup(optarg);
                printf("device_name: %s\n", device_name);
                break;
            }
            case 'w':
            {
                test_write = 1;
                break;
            }
            case 'i':
            {
                test_inquiry = 1;
                break;
            }
            case 'r':
            {
                test_read = 1;
                break;
            }
            case 't':
            {
                parse_mbr = 1;
                break;
            }
            case 'h':
            {
                print_help();
                break;
            }
            case '?':
            {
                print_help();
                break;
            }
            default:
            {
                break;
            }
        }
    }

    if (!device_name)
    {
        fprintf(stderr, "Device is not set\n");
        print_help();
        exit(1);
    }

    if (max_blocks < no_of_blocks)
    {
        max_blocks = no_of_blocks;
    }
}

int inquiry_data(int fd, unsigned char *buffer, unsigned int *duration)
{
    int i = 0;
    sg_io_hdr_t io_hdr_i;
    unsigned char sense_buffer[SENSE_BUF_LEN];
    struct timeval tv_start;
    struct timeval tv_stop;

    unsigned char inquiryCmdBlk[OP6_CMD_LEN] =
        {CMD_INQUIRY,
        0x00,
        0,
        0,
        0xff,
        0};

    if (verbose)
    {
        hexDump("inquiryCmdBlk", inquiryCmdBlk, OP6_CMD_LEN, 0);
    }

    memset(buffer, 0, MIN_BLOCK_LEN * 16);
    memset(&io_hdr_i,0,sizeof(sg_io_hdr_t));
    memset(sense_buffer, 0, SENSE_BUF_LEN);

    io_hdr_i.interface_id = 'S';
    io_hdr_i.cmd_len = sizeof(inquiryCmdBlk);
    io_hdr_i.mx_sb_len = sizeof(sense_buffer);
    io_hdr_i.dxfer_direction = SG_DXFER_FROM_DEV;
    io_hdr_i.dxfer_len = MIN_BLOCK_LEN * 16;
    io_hdr_i.dxferp = buffer;
    io_hdr_i.cmdp = inquiryCmdBlk;
    io_hdr_i.sbp = sense_buffer;
    io_hdr_i.timeout = 20000;

    gettimeofday(&tv_start, NULL);

    if(ioctl(fd, SG_IO, &io_hdr_i) < 0)
    {
        printf("\nioctl failed. %s\n", strerror(errno));
        for(i = 0; i < SENSE_BUF_LEN;i++)
        {
            printf("%02x",sense_buffer[i]);
        }
        printf("\n");
        close(fd);
        return 1;
    }

    gettimeofday(&tv_stop, NULL);

    if (verbose)
    {
        hexDump("BufI", buffer, MIN_BLOCK_LEN * 16, lba);
    }

    if (duration)
    {
        *duration = ((tv_stop.tv_sec) * 1000 + (tv_stop.tv_usec) / 1000) -
        (((tv_start.tv_sec) * 1000 + (tv_start.tv_usec) / 1000));
    }

    return 0;
}

int read_data(int fd, unsigned char *buffer, unsigned int *duration)
{
    int i = 0;
    sg_io_hdr_t io_hdr_r;
    unsigned char sense_buffer[SENSE_BUF_LEN];
    struct timeval tv_start;
    struct timeval tv_stop;

    unsigned char r10CmdBlk[OP10_CMD_LEN] =
        {CMD_READ10,       // cmd
        0x08,                   // flags
        (lba >> 24),            // lba msb
        (lba >> 16),
        (lba >> 8),
        lba,                    // lba lsb
        0,                      // RES + GN
        (no_of_blocks >> 8),    // transfer len msb
        no_of_blocks,           // transfer len lsb
        0};                     // control

    if (verbose)
    {
        hexDump("r10CmdBlk", r10CmdBlk, OP10_CMD_LEN, 0);
    }

    //     read
    memset(&io_hdr_r,0,sizeof(sg_io_hdr_t));
    memset(buffer, 0, MIN_BLOCK_LEN * no_of_blocks);
    io_hdr_r.interface_id = 'S';
    io_hdr_r.cmd_len = sizeof(r10CmdBlk);
    io_hdr_r.mx_sb_len = sizeof(sense_buffer);
    io_hdr_r.dxfer_direction = SG_DXFER_FROM_DEV;
    io_hdr_r.dxfer_len = MIN_BLOCK_LEN * no_of_blocks;
    io_hdr_r.dxferp = buffer;
    io_hdr_r.cmdp = r10CmdBlk;
    io_hdr_r.sbp = sense_buffer;
    io_hdr_r.timeout = 20000;

    memset(sense_buffer, 0, SENSE_BUF_LEN);

    gettimeofday(&tv_start, NULL);

    if(ioctl(fd, SG_IO, &io_hdr_r) < 0)
    {
        printf("ioctl failed. %s\n", strerror(errno));
        for(i = 0; i < SENSE_BUF_LEN;i++)
        {
            printf("%02x",sense_buffer[i]);
        }
        printf("\n");
        return 1;
    }

    gettimeofday(&tv_stop, NULL);

    if (verbose)
    {
        hexDump("BufR", buffer, MIN_BLOCK_LEN * no_of_blocks, lba);
    }

    if (duration)
    {
        *duration = ((tv_stop.tv_sec) * 1000 + (tv_stop.tv_usec) / 1000) -
            (((tv_start.tv_sec) * 1000 + (tv_start.tv_usec) / 1000));
    }

    return 0;
}

int write_data(int fd, unsigned char *buffer, unsigned int *duration)
{
    struct timeval tv_start;
    struct timeval tv_stop;
    int i = 0;
    sg_io_hdr_t io_hdr_w;
    unsigned char sense_buffer[SENSE_BUF_LEN];
    unsigned char w10CmdBlk[OP10_CMD_LEN] =
        {CMD_WRITE10,      // cmd
        0x00,                   // flags
        (lba >> 24),            // lba msb
        (lba >> 16),
        (lba >> 8),
        lba,                    // lba lsb
        0,                      // RES + GN
        (no_of_blocks >> 8),       // transfer len msb
        no_of_blocks,              // transfer len lsb
        0};                     // control

    if (verbose)
    {
        hexDump("w10CmdBlk", w10CmdBlk, OP10_CMD_LEN, 0);
    }

    memset(sense_buffer, 0, SENSE_BUF_LEN);

    if (verbose)
    {
        hexDump("BufW", buffer, MIN_BLOCK_LEN * no_of_blocks, lba);
    }

    //write
    memset(&io_hdr_w,0,sizeof(sg_io_hdr_t));
    io_hdr_w.interface_id = 'S';
    io_hdr_w.cmd_len = sizeof(w10CmdBlk);
    io_hdr_w.mx_sb_len = sizeof(sense_buffer);
    io_hdr_w.dxfer_direction = SG_DXFER_TO_DEV;
    io_hdr_w.dxfer_len = MIN_BLOCK_LEN * no_of_blocks;
    io_hdr_w.dxferp = buffer;
    io_hdr_w.cmdp = w10CmdBlk;
    io_hdr_w.sbp = sense_buffer;
    io_hdr_w.timeout = 20000;

    gettimeofday(&tv_start, NULL);

    if(ioctl(fd, SG_IO, &io_hdr_w) < 0)
    {
        printf("ioctl failed. %s\n", strerror(errno));
        for(i = 0; i < SENSE_BUF_LEN;i++)
        {
            printf("%02x",sense_buffer[i]);
        }
        printf("\n");
        close(fd);
        return 1;
    }

    gettimeofday(&tv_stop, NULL);

    if (duration)
    {
        *duration = ((tv_stop.tv_sec) * 1000 + (tv_stop.tv_usec) / 1000) -
            (((tv_start.tv_sec) * 1000 + (tv_start.tv_usec) / 1000));
    }

    return 0;
}

int show_partition()
{
    int i = 0;
    int fd = 0;
    lba = 0;
    no_of_blocks = 1;

    unsigned char buffer[MIN_BLOCK_LEN * no_of_blocks];

    if((fd = open(device_name, O_RDWR)) < 0)
    {
        printf("device file opening failed: %s\n", strerror(errno));
        free(device_name);
        return 1;
    }

    read_data(fd, buffer, NULL);

    free(device_name);
    close(fd);

    unsigned char *bufp = buffer;
    bufp += PART_TABLE_MBR_OFF;

    uint32_t part_offset = 0;
    uint32_t part_sectors = 0;

    for (i = 0;i < 4;i++)
    {
        memcpy(&part_offset, bufp + 0x08, sizeof(part_offset));
        memcpy(&part_sectors, bufp + 0x0C, sizeof(part_sectors));

        if (!part_offset && !part_sectors)
        {
            bufp += 0x10;
            continue;
        }

        fprintf(stdout, "\nPartition %i\n", i + 1);

        fprintf(stdout, "\tstatus: %02x\n", *bufp);
        fprintf(stdout, "\tCHS address: %02x\n", *(bufp + 1));

        fprintf(stdout, "\toffset: %x (%d)\n", part_offset, part_offset);
        fprintf(stdout, "\tsectors: %x (%d)\n", part_sectors, part_sectors);

        bufp += 0x10;
    }

    return 0;
}

int main(int argc, char* argv[])
{
    int fd = 0;
    int i = 0;

    uint64_t iduration = 0;
    uint64_t wduration = 0;
    uint64_t rduration = 0;
    unsigned int icmdduration = 0;
    unsigned int wcmdduration = 0;
    unsigned int rcmdduration = 0;

    if(argc < 2)
    {
        print_help();
        return 1;
    }

    process_args(argc, argv);

    if (parse_mbr)
    {
        show_partition();
        return 0;
    }

    if((fd = open(device_name, O_RDWR)) < 0)
    {
        printf("device file opening failed: %s\n", strerror(errno));
        return  1;
    }

    if (test_inquiry)
    {
        unsigned char buffer_i[MIN_BLOCK_LEN * 16];

        unsigned char inquiryCmdBlk[OP6_CMD_LEN] =
            {CMD_INQUIRY,
            0x00,
            0,
            0,
            0xff,
            0};


        inquiry_data(fd, buffer_i, &icmdduration);
        iduration += icmdduration;

        printf("(INQUIRY) test time: %lu ms, aver cmd time: %lu ms\n",
            iduration,
            (iduration / test_count));

    }

    while(no_of_blocks <= max_blocks)
    {
        wduration = 0;
        rduration = 0;

        if (test_write)
        {
            unsigned char buffer_w[MIN_BLOCK_LEN * no_of_blocks];

            for (i = 0; i < test_count;i++)
            {
                memset(buffer_w, 0, MIN_BLOCK_LEN * no_of_blocks);

                if (i != test_count - 1)
                {
                    srand(time(NULL));
                    int j = 0;
                    for(j = 0; (j < MIN_BLOCK_LEN * no_of_blocks);j++)
                    {
                        buffer_w[j] = rand();
                    }
                }

                fprintf(stdout, "\rwrite count: %d", i + 1);
                fflush(stdout);
                write_data(fd, buffer_w, &wcmdduration);

                wduration += wcmdduration;
            }

            printf("\r");

            printf("(WRITE) blocks: %3d (%5d), data: %8u B, test time: %6lu ms, aver cmd time: %3lu ms, averspeed: %.2f KB/s\n",
                no_of_blocks,
                no_of_blocks * MIN_BLOCK_LEN,
                (MIN_BLOCK_LEN * no_of_blocks * test_count),
                wduration,
                (wduration / test_count),
                ((MIN_BLOCK_LEN * no_of_blocks * test_count) / 1024.0) /
                                            (wduration / 1000.0));
        }

        if (test_read)
        {
            unsigned char buffer_r[MIN_BLOCK_LEN * no_of_blocks];

            //     read
            for (i = 0; i < test_count;i++)
            {
                fprintf(stdout, "\rread count: %d", i + 1);
                fflush(stdout);
                read_data(fd, buffer_r, &rcmdduration);
                rduration += rcmdduration;
            }

            printf("\r");

            printf("(READ)  blocks: %3d (%5d), data: %8u B, test time: %6lu ms, aver cmd time: %3lu ms, averspeed: %.2f KB/s\n",
                no_of_blocks,
                no_of_blocks * MIN_BLOCK_LEN,
                (MIN_BLOCK_LEN * no_of_blocks * test_count),
                rduration,
                (rduration / test_count),
                ((MIN_BLOCK_LEN * no_of_blocks * test_count) / 1024.0) /
                (rduration / 1000.0));
        }

        no_of_blocks++;
    }


    free(device_name);
    close(fd);

    return 0;
}
