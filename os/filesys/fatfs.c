#include <stdio.h>
#include <types.h>
#include <shell.h>
#include <rtdef.h>
#include "queue.h"
#include "filesys.h"
#include "fatfs.h"
#include "devfs.h"
#define MAX_DIRBUF 10

//#define DEBUG_FATFS

#ifdef DEBUG_FATFS
#define debug_fatfs(format, arg...) printf("debug_fatfs: " format, ## arg)
#else
#define debug_fatfs(format, arg...)
#endif

static int MAX_BUFSZ_CLUSTS=32;
static void *_myfile[OPEN_MAX];
struct direntry dirbuf[MAX_DIRBUF];

int readsector(struct fat_sc *fsc, int sector, int count, u_int8_t *buffer)
{
	off_t res;
	res = ((off_t)(fsc->PartitionStart)) * SECTORSIZE + SECTORSIZE *((off_t)sector);
	res = devio_lseek(fsc->fd,	res, 0);
	res =devio_read(fsc->fd, buffer, (SECTORSIZE * count));
	if (res == (SECTORSIZE * count)) 
		return (1);
	else 
		return (-1);
}


u_int32_t getFatEntry(struct fat_sc *fsc, int entry)
{
	u_int32_t fatsect;
	u_int32_t byteoffset;
	u_int32_t fatstart;
	u_int32_t fatoffset;
	u_int8_t b1,b2,b3,b4;
	int res;

	fatstart = fsc->ResSectors;
	
	if (fsc->FatType == TYPE_FAT12) {
		int odd;

		odd = entry & 1;
		byteoffset = ((entry & ~1) * 3) / 2;
		fatsect = byteoffset / SECTORSIZE;
		fatoffset = byteoffset % SECTORSIZE;

		if (fsc->FatCacheNum != fatsect) {
			res = readsector(fsc, fatsect + fatstart, 1, fsc->FatBuffer);
			if (res < 0) {
				return res;
			}
			fsc->FatCacheNum = fatsect;
		}
		
		b1 = fsc->FatBuffer[fatoffset];
		
		if ((fatoffset+1) >= SECTORSIZE) {
			res = readsector(fsc, fatsect + 1 + fatstart, 1, fsc->FatBuffer);
			if (res < 0) {
				return res;
			}
			fsc->FatCacheNum = fatsect+1;
			fatoffset -= SECTORSIZE;
		}
		
		b2 = fsc->FatBuffer[fatoffset+1];

		if ((fatoffset+2) >= SECTORSIZE) {
			res = readsector(fsc, fatsect + 1 + fatstart, 1, fsc->FatBuffer);
			if (res < 0) {
				return res;
			}
			fsc->FatCacheNum = fatsect + 1;
			fatoffset -= SECTORSIZE;
		}
		
		b3 = fsc->FatBuffer[fatoffset+2];
		
		if (odd) {
			return ((unsigned int) b3 << 4) + ((unsigned int) (b2 & 0xF0) >> 4);
		} else {
			return ((unsigned int) (b2 & 0x0F) << 8) + ((unsigned int) b1);
		}

	} else if (fsc->FatType == TYPE_FAT16) {
		byteoffset = entry * 2;
		fatsect = byteoffset / SECTORSIZE;
		fatoffset = byteoffset % SECTORSIZE;
		
		if (fsc->FatCacheNum != fatsect) {
			res = readsector(fsc, fatsect + fatstart, 1, fsc->FatBuffer);
			if (res < 0) {
				return res;
			}
			fsc->FatCacheNum = fatsect;
		}

		b1 = fsc->FatBuffer[fatoffset];
		b2 = fsc->FatBuffer[fatoffset+1];
		return ((unsigned int) b1) + (((unsigned int) b2) << 8);
	} else if (fsc->FatType == TYPE_FAT32) {
		byteoffset = entry * 4;
		fatsect = byteoffset / SECTORSIZE;
		fatoffset = byteoffset % SECTORSIZE;
		
		if (fsc->FatCacheNum != fatsect) {
			res = readsector(fsc, fatsect + fatstart, 1, fsc->FatBuffer);
			if (res < 0) {
				return res;
			}
			fsc->FatCacheNum = fatsect;
		}

		b1 = fsc->FatBuffer[fatoffset];
		b2 = fsc->FatBuffer[fatoffset+1];
		b3 = fsc->FatBuffer[fatoffset+2];
		b4 = fsc->FatBuffer[fatoffset+3];
		return (((unsigned int) b1) + (((unsigned int) b2) << 8) + (((unsigned int) b3) << 16) + (((unsigned int) b4) << 24));
	}
	return (0);
}
/*
#define fat_getChain(a,b,c)	({int ret=fat_getChain2(a,b,c);\
								printf("%s,%d,\n",__func__, __LINE__);\
								ret;\
								})
*/
//static unsigned int chainMem[10240];
int fat_getChain(struct fat_sc *fsc, int start, struct fatchain *chain)
{
	u_int32_t mask;
	u_int32_t entry;
	int count;
	int i;
	int flag;

	debug_fatfs("fat_getChain start0x%x\n",start);
	switch (fsc->FatType) 
	{
	case TYPE_FAT12:
		mask = FAT12_MASK;
		break;
	case TYPE_FAT16:
		mask = FAT16_MASK;
		break;
	case TYPE_FAT32:
		mask = FAT32_MASK;
		break;
	default:
		mask = 0;
	}

	count = 0;
	flag = start;
	while (1) 
	{
		entry = getFatEntry(fsc, flag);
		if (entry == (CLUST_EOFE & mask & 0xfffffff7)) 
		{
			printf("clust is error!");
			return -1;
		}
		
		if(entry >= (CLUST_EOFE & mask & 0xfffffff8))
			break;
		flag = entry;
		count++;
	}

	chain->count = count + 1;
	chain->start = start;

	//chain->entries =chainMem; //(u_int32_t *)malloc(sizeof(u_int32_t) * chain->count+512);
	//memset(chain->entries,0,10240);//sizeof(u_int32_t) * chain->count+512

	chain->entries =(u_int32_t *)malloc(sizeof(u_int32_t) * chain->count+512);
	memset(chain->entries,0,sizeof(u_int32_t) * chain->count+512);
	
	if (chain->entries == NULL) 
	{
		printf("fat_getChain chain->entries malloc error\n");
		return (-1);
	}
	else
		debug_fatfs("chain->entries2 addr:0x%08x\n",chain->entries);

	chain->entries[0] = start;
	flag = start;
	i = 0;
	while(1) 
	{
		//debug_fatfs("chain->entries[%d]:0x%08x\n",i,chain->entries[i]);
		entry = getFatEntry(fsc, flag);
		chain->entries[i+1] = entry;
		if (entry == (CLUST_EOFE & mask & 0xfffffff7)) 
		{
			printf("clust is error!");
			return -1;
		}
		
		if(entry >= (CLUST_EOFE & mask & 0xfffffff8))
			break;
		flag = entry;
		i++;
	}
	return (1);
}

int fat_findfile(struct fat_sc *fsc, char *name)
{
	struct fat_fileentry filee;
	struct direntry *dire;
	char *dpath;
	int long_name = 0;
	int dir_scan = 0, dir_list = 0;
	int i;
	int flag = 0;
	int dir_flag = 0;

	debug_fatfs("fat_findfile,fsc->RootClus:0x%08x\n",fsc->RootClus);
	if(fsc->FatType == TYPE_FAT32)
	{
		struct fatchain chain;
		int res;
		fat_getChain(fsc, fsc->RootClus , &chain);//根 填充fsc->file.Chain.entries信息
		//printf("fat_findfile falg\n");
		res = fat_subdirscan(fsc, name, &chain);//目录扫描
		if (chain.entries)
			free(chain.entries);
		return (res);
	}
	else
	{
		memset(&filee,0, sizeof(filee));
		dpath = strchr(name, '/');
		if (dpath != NULL) {
			*dpath = '\0';
			dpath++;
			dir_scan = 1;
		}		
		else if(*name == 0)
			dir_list = 1;

		for (fsc->DirCacheNum = fsc->FirstRootDirSecNum; fsc->DirCacheNum <=(fsc->RootDirSectors + fsc->FirstRootDirSecNum); fsc->DirCacheNum++) 
		{
			if (readsector(fsc, fsc->DirCacheNum, 1, fsc->DirBuffer) == 0) {
				return (0);
			}

			dire = (struct direntry *)fsc->DirBuffer;

			for (i = 0; (i < (SECTORSIZE / sizeof(struct direntry))); i++, dire++) {

				if (dire->dirName[0] == SLOT_EMPTY)
				{
					if(dir_list)
						return 2;
					return (0);
				}

				if (dire->dirName[0] == SLOT_DELETED)
				{
					continue;
				}
				if(dire->dirAttributes == ATTR_VOLUME)
					continue;

				if(dire->dirAttributes == ATTR_DIRECTORY)
				{
					dir_flag = 1;
				}
				if (dire->dirAttributes == ATTR_WIN95) {
					memcpy( (void *)&dirbuf[long_name], (void *)dire,sizeof(struct direntry));
					flag = 1;
					long_name++;
					if (long_name > MAX_DIRBUF - 1)
						long_name = 0;
					continue;
				}
				memcpy((void *)&dirbuf[long_name],(void *)dire,  sizeof(struct direntry));
				fat_parseDirEntries(long_name, &filee);
				long_name = 0;

				if(dir_list)
				{		
					if((flag == 0)||(strcmp(filee.shortName,".") == 0)||(strcmp(filee.shortName, "..") == 0))
					{
						printf("%s",filee.shortName);
					}
					else
						printf("%s",filee.longName);
					if(dir_flag == 1)
					{
						dir_flag = 0;
						printf("/");
					}
					printf("  ");
					flag = 0;
				}
				else if ((strcasecmp(name, filee.shortName) == 0) || (strcasecmp(name, filee.longName) == 0)) {
					if (dir_scan) {
						struct fatchain chain;
						int res;

						fat_getChain(fsc, letoh32(filee.StartCluster|(filee.HighClust << 16)) , &chain);
						res = fat_subdirscan(fsc, dpath, &chain);
						if (chain.entries) 
						{
							free(chain.entries);
						}
						return (res);
					} else {
						strcpy(fsc->file.shortName, filee.shortName);
						fsc->file.HighClust = filee.HighClust;
						fsc->file.StartCluster = filee.StartCluster;
						fsc->file.FileSize = filee.FileSize;
						fat_getChain(fsc, (fsc->file.StartCluster|(fsc->file.HighClust << 16)), &fsc->file.Chain);
						return (1);
					}
				}
			}
		}
	}
	printf("\n");
	return (2);
}

int fat_subdirscan(struct fat_sc *fsc, char *name, struct fatchain *chain)
{
	struct fat_fileentry filee;
	struct direntry *dire;
	char *dpath;
	int long_name = 0;
	int dir_scan = 0, dir_list = 0;
	int sector;
	int i,j,k;
	int flag = 0;
	int dir_flag = 0;
	memset(&filee,0, sizeof(filee));

	dpath = strchr(name, '/');
	if (dpath != NULL) 
	{
		*dpath = '\0';
		dpath++;
		dir_scan = 1;
	}
	else if(*name == 0)
		dir_list = 1;

	for (i = 0; i < chain->count; i++) 
	{
		for (j = 0; j < fsc->SecPerClust; j++) 
		{
			sector = getSectorIndex(fsc, chain, j, i);
			if (readsector(fsc, sector, 1, fsc->DirBuffer) == 0) 
			{
				return (0);
			}

			dire = (struct direntry *)fsc->DirBuffer;

			for (k = 0; (k < (SECTORSIZE / sizeof(struct direntry))); k++, dire++) 
			{
				
				if (dire->dirName[0] == SLOT_EMPTY)
				{
					if(dir_list)
					{
						printf("\n");
						return 2;
					}
					return (0);
				}

				if (dire->dirName[0] == SLOT_DELETED)
					continue;
				if(dire->dirAttributes == ATTR_VOLUME)
					continue;
				if(dire->dirAttributes == ATTR_DIRECTORY)
				{
					dir_flag = 1;
				}
				if (dire->dirAttributes == ATTR_WIN95) 
				{
					memcpy( (void *)&dirbuf[long_name], (void *)dire,sizeof(struct direntry));
					long_name++;
					flag = 1;
					if (long_name > MAX_DIRBUF - 1)
						long_name = 0;
					continue;
				}
				memcpy( (void *)&dirbuf[long_name],(void *)dire, sizeof(struct direntry));
				fat_parseDirEntries(long_name, &filee);
				long_name = 0;
				if(dir_list)
				{
					if((flag == 0)||(strcmp(filee.shortName,".") == 0)||(strcmp(filee.shortName, "..") == 0))
					{
						printf("%s", filee.shortName);
					}
					else
						printf("%s", filee.longName);
					if(dir_flag == 1)
					{
						dir_flag = 0;
						printf("/");
					}
					printf("  ");
					flag = 0;
				}
				else if ((strcasecmp(name, filee.shortName) == 0) || (strcasecmp(name, filee.longName) == 0)) 
				{
					if (dir_scan) {
						struct fatchain chain;
						int res;
						fat_getChain(fsc, letoh32(filee.StartCluster|(filee.HighClust << 16)) , &chain);
						res = fat_subdirscan(fsc, dpath, &chain);
						///*
						if (chain.entries) 
						{
							free(chain.entries);
						}
						//*/
						return (res);
					} 
					else {
						strcpy(fsc->file.shortName, filee.shortName);
						fsc->file.HighClust = filee.HighClust;
						fsc->file.StartCluster = filee.StartCluster;
						fsc->file.FileSize = filee.FileSize;
						fat_getChain(fsc, (fsc->file.StartCluster|(fsc->file.HighClust << 16)), &fsc->file.Chain);
						return (1);
					}
				}
			}
		}
	}
	printf("\n");
	return (0);
}

int fat_init(int fd, struct fat_sc *fsc, int partition)
{
	unsigned char bootsector[SECTORSIZE];
	struct bpb_t *bpb;

	memset(fsc,0, sizeof(struct fat_sc));

	fsc->fd = fd;

	// Check for partition
	if(!fat_getPartition(fsc, partition))
	{
		printf("get partition error: %d\n", partition);
		return 0;
	}

	//Init BPB
	if (readsector(fsc, 0, 1, bootsector) == 0) 
	{
		printf("Can't read from %d bytes\n", SECTORSIZE);
		return (-1);
	}
	if((bootsector[0] == 0)&&(bootsector[1] == 0))
	{
		printf("It's not fat!\n");
		return -1;
	}
	bpb = (struct bpb_t *)bootsector;
	
	//Init Fat Software structure
	fsc->RootDirEnts = letoh16(bpb->bpbRootDirEnts);
	fsc->BytesPerSec = letoh16(bpb->bpbBytesPerSec);
	fsc->ResSectors = letoh16(bpb->bpbResSectors);
	fsc->SecPerClust = bpb->bpbSecPerClust;
	fsc->BytesPerClust = fsc->SecPerClust*SECTORSIZE;

	MAX_BUFSZ_CLUSTS = 0x100000/fsc->BytesPerClust;

	fsc->LastSectorBuffer = malloc(fsc->BytesPerClust*MAX_BUFSZ_CLUSTS);

	fsc->NumFATs = bpb->bpbFATs;
	fsc->FatCacheNum = -1;
	fsc->RootClus = bpb->efat32.bpbRootClus;

	if (bpb->bpbFATsecs != 0)
		fsc->FATsecs = (u_int32_t)letoh16(bpb->bpbFATsecs);
	else
		fsc->FATsecs = letoh32(bpb->efat32.bpbFATSz32);

	if (bpb->bpbSectors != 0)
		fsc->TotalSectors = (u_int32_t)letoh16(bpb->bpbSectors);
	else
		fsc->TotalSectors = letoh32(bpb->bpbHugeSectors);

	fsc->RootDirSectors = ((fsc->RootDirEnts * 32) + (fsc->BytesPerSec - 1)) / fsc->BytesPerSec;
	fsc->DataSectors = fsc->TotalSectors - (fsc->ResSectors + (fsc->NumFATs * fsc->FATsecs) + fsc->RootDirSectors);
	fsc->DataSectorBase = fsc->ResSectors + (fsc->NumFATs * fsc->FATsecs) + fsc->RootDirSectors;
	fsc->CountOfClusters = fsc->DataSectors / fsc->SecPerClust;
	fsc->ClusterSize = fsc->BytesPerSec * fsc->SecPerClust;
	fsc->FirstRootDirSecNum = fsc->ResSectors + (fsc->NumFATs * fsc->FATsecs);

	if (fsc->CountOfClusters < 4085) {
		/* Volume is FAT12 */
		fsc->FatType = TYPE_FAT12;
	} else if (fsc->CountOfClusters < 65525) {
		/* Volume is FAT16 */
		fsc->FatType = TYPE_FAT16;
	} else {
		/* Volume is FAT32 */
		fsc->FatType = TYPE_FAT32;
	}
	free(fsc->LastSectorBuffer);

	return (1);
}


/*
 * Supported paths:
 *	/dev/fat@wd0/file
 *	/dev/fs/fat@wd0/file
 *
 */
int fat_open(int fd, const char *path, int flags, int mode)
{
	char dpath[64];
	char dpath2[64];
	const char *opath;
	char *filename;
	struct fat_sc *fsc;
	int fd2;
	int res;
	int partition = 0, dpathlen;
	char * p;

	/*  Try to get to the physical device */
	opath = path;
	if (strncmp(opath, "/dev/", 5) == 0)
		opath += 5;
	if (strncmp(opath, "fat/", 4) == 0)
		opath += 4;
	else if (strncmp(opath, "fat@", 4) == 0)
		opath += 4;

	/* There has to be at least one more component after the devicename */
	if (strchr(opath, '/') == NULL) 
	{
		errno = ENOENT;
		return (-1);
	}

	/* Dig out the device name */
	strncpy(dpath2, opath, sizeof(dpath2));
	
	char *pt2=NULL;
	pt2=strchr(dpath2, '/');
	if(pt2!=NULL)
		pt2[0]='\0';
	//*(strchr(dpath2, '/')) = '\0';
	
	sprintf(dpath, "/dev/%s", dpath2);

	/* Set opath to point at the isolated filname path */
	filename = strchr(opath, '/') + 1;

	fsc = (struct fat_sc *)malloc(sizeof(struct fat_sc));
	if (fsc == NULL) 
	{
		errno = ENOMEM;
		return (-1);
	}
	memset(fsc,0,sizeof(struct fat_sc));
	/*
	 * Need to mark existing fd to valid otherwise we can't
	 * open the new device.
	 */
	_file[fd].valid = 1;
	_file[fd].posn=0;


	//here we see if the name is /dev/disk/wd0a or something like this
	dpathlen = strlen(dpath);
	if(dpath[dpathlen - 1] >= 'a' && dpath[dpathlen - 1] <= 'z')
	{
		partition = dpath[dpathlen - 1] - 'a';
		dpath[dpathlen - 1] = 0;
	}
	
	if(devio_open(fd,dpath,flags,mode)==0)
		return -1;
	
	debug_fatfs("fat_open fd:%d\n",fd);
	if (fat_init(fd, fsc, partition) == 0) //初始化
	{
		printf("fat_open fat_init fd:%d\n",fd);
		free(fsc);
		errno = EINVAL;
		_file[fd].valid = 0;
		close(fd);
		return (-1);
	}
	debug_fatfs("fat_open fat_init filename:%s\n",filename);
	res = fat_findfile(fsc, filename);//查找文件并缓存目录信息等 填充fsc->file.Chain.entries信息但是为什么会在这里被free
	debug_fatfs("fat_open fat_findfile res:%d\n",res);
	if (res <= 0 || res == 2) 
	{
		free(fsc);
		errno = EINVAL;
		if(res == 2)
			errno = EISDIR;
		_file[fd].valid = 0;
		close(fd);
		return (-1);
	}

	fsc->LastSector = -1;	/* No valid sector in sector buffer */
	fsc->BytesOfBuffer = 0;
	fsc->SectorsOfBuffer = 0;

	_file[fd].posn = 0;
	_myfile[fd] = (void *)fsc;
//	free(fsc);
	debug_fatfs("fat_open success fd:%d\n",fd);
	return (fd);
}

int fat_close(int fd)
{
	struct fat_sc *fsc;

	fsc = (struct fat_sc *)_myfile[fd];
///*
	if (fsc->file.Chain.entries) 
	{
		free(fsc->file.Chain.entries);
		fsc->file.Chain.entries = NULL;
	}
	//*/
	fsc->file.Chain.count = 0;
	close(fsc->fd);
	free(fsc);

	devio_close(fd);

	return (0);
}

int fat_read(int fd, void *buf, size_t len)
{
	struct fat_sc *fsc;
	int sectorIndex;
	int totalcopy = 0;
	int origpos;
	int copylen;
	int offset;
	int sector;
	int res = 0;

	fsc = (struct fat_sc *)_myfile[fd];

	origpos = _file[fd].posn;

	 //Check file size bounder
	if (_file[fd].posn >= fsc->file.FileSize) 
	{
	        return (-1);
	}

	if ((_file[fd].posn + len) > fsc->file.FileSize) 
	{
		len = fsc->file.FileSize - _file[fd].posn;
	}


	while (len) 
	{
		sectorIndex = _file[fd].posn / SECTORSIZE;
		sector = getSectorIndex_read(fsc, &fsc->file.Chain, sectorIndex, (len + SECTORSIZE -1)/SECTORSIZE);
		debug_fatfs("fat_read,sectorIndex:%d,sector:0x%x,fsc->LastSector:%d,fsc->SectorsOfBuffer:%d\n",
			sectorIndex,sector,fsc->LastSector,fsc->SectorsOfBuffer);
		if(fsc->LastSector == -1 || sector < fsc->LastSector ||  sector >= fsc->LastSector + fsc->SectorsOfBuffer)
		{
			fsc->SectorsOfBuffer = fsc->nextSectorsOfBuffer;
			fsc->BytesOfBuffer = fsc->nextBytesOfBuffer;
			fsc->LastSector = sector;
			res = readsector(fsc, sector, fsc->SectorsOfBuffer, fsc->LastSectorBuffer);
			if (res < 0)
				break;
		}

		offset = (sector - fsc->LastSector)*SECTORSIZE + (_file[fd].posn % SECTORSIZE);
		copylen = len;
		if (copylen > (fsc->BytesOfBuffer - offset)) 
		{
			copylen = (fsc->BytesOfBuffer - offset);
		}
		
		memcpy(buf, &fsc->LastSectorBuffer[offset], copylen);
		//dataHexPrint2(buf,copylen,0,"fat_read buf");
		buf += copylen;
		_file[fd].posn += copylen;
		len -= copylen;
		totalcopy += copylen;
	}
	
	if (res < 0) 
	{
		_file[fd].posn = origpos;
		return res;
	}
	/*
	int i=0;
	for(i=0;i<fsc->file.Chain.count;i++)
	{
		printf("chain->entries[%d]:0x%08x\n",i,fsc->file.Chain.entries[i]);
	}
	*/
	return totalcopy;
}

int fat_write(int fd, const void *start, size_t size)
{
	errno = EROFS;
	return EROFS;
}

off_t fat_lseek(int fd, off_t offset, int whence)
{
	struct fat_sc *fsc;
	fsc = (struct fat_sc *)_myfile[fd];

	switch (whence) 
	{
		case SEEK_SET:
		{
			debug_fatfs("fat_lseek1,_file[fd].posn:%d\n",_file[fd].posn);
			_file[fd].posn = offset;
			debug_fatfs("fat_lseek2,_file[fd].posn:%d\n",_file[fd].posn);
			break;
		}
		case SEEK_CUR:
		{
			_file[fd].posn += offset;
			break;
		}
		case SEEK_END:
		{
			_file[fd].posn = fsc->file.FileSize + offset;
			break;
		}
		default:
		{
			errno = EINVAL;
			return (-1);
		}
	}
	return (_file[fd].posn);
}


/*
 *  File system registration info.
 */
 /*
static DiskFileSystem diskfile={
        "fat", 
        fat_open,
        fat_read,
        fat_write,
        fat_lseek,
        fat_close,
	NULL
};
		*/

/***************************************************************************************************
 * Internal function
 ***************************************************************************************************/

int getSectorIndex_read(struct fat_sc *fsc, struct fatchain * chain, int index, int need)
{
	int clusterIndex, sectorIndex;
	int sector;
	int entry, lastentry, entry0;
	int i;
	
	entry=lastentry=entry0=0;

	sectorIndex = index % fsc->SecPerClust;
	for(i=0,clusterIndex = index / fsc->SecPerClust;i<min(((need+fsc->SecPerClust-1)/fsc->SecPerClust),MAX_BUFSZ_CLUSTS);
									i++,clusterIndex++)
	{
		
		if (clusterIndex < chain->count)
			entry = chain->entries[clusterIndex];
		else
			break;
		if(i==0) 
			entry0 = entry;
		debug_fatfs("getSectorIndex_read,i:%d,clusterIndex:%d,entry:0x%x,entry0:0x%x,chain->count:%d\n",
			i,clusterIndex,entry,entry0,chain->count);
		if(i!=0 && lastentry + 1 != entry) 
			break;
		lastentry = entry;
	}
	debug_fatfs("entry0:0x%0x\n",entry0);
	fsc->nextSectorsOfBuffer = min((i * fsc->SecPerClust - sectorIndex),need);
	fsc->nextBytesOfBuffer = fsc->nextSectorsOfBuffer*SECTORSIZE;

	sector = fsc->DataSectorBase + (entry0 - 2) * fsc->SecPerClust + sectorIndex;
	debug_fatfs("getSectorIndex_read,fsc->DataSectorBase:0x%08x,entry0:0x%08x,fsc->SecPerClust:0x%08x,sectorIndex:0x%08x,sector:0x%08x\n",
		fsc->DataSectorBase,entry0,fsc->SecPerClust,sectorIndex,sector);
/*
	for(i=0;i<chain->count;i++)
		printf("chain[%d]:0x%08x\n",i,chain->entries[i]);
	*/
	return (sector);
}


int getSectorIndex(struct fat_sc *fsc, struct fatchain * chain, int index, int clust_num)
{
	int clusterIndex;
	int sectorIndex;
	int sector;
	int entry;

	clusterIndex = index / fsc->SecPerClust;
	sectorIndex = index % fsc->SecPerClust;

	if (clusterIndex < chain->count)
		entry = chain->entries[clust_num];
	else
		entry = -1;

	sector = fsc->DataSectorBase + (entry - 2) * fsc->SecPerClust + sectorIndex;
	return (sector);
}

int fat_getPartition(struct fat_sc *fsc, int partition)
{
	u_int8_t buffer[SECTORSIZE];
	struct mbr_t *mbr;

	fsc->PartitionStart = 0;
	devio_read(fsc->fd,buffer,SECTORSIZE);
	/*
	 * Check if there exists a MBR
	 */
	mbr = (struct mbr_t *)buffer;
	/*
	 * Find the active partition
	 */
	fsc->PartitionStart = letoh32(mbr->partition[partition].relsect);
	return (1);
}

u_int8_t shortNameChkSum(u_int8_t *name)
{
	u_int16_t len;
	u_int8_t sum;

	sum = 0;

	for (len = 11; len != 0; len--) {
		sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + *name++;
	}
	return (sum);
	
}


int fat_parseDirEntries(int dirc, struct fat_fileentry *filee)
{
	struct direntry *dire;
	struct winentry *wine;
	u_int8_t longName[290];
	u_int8_t chksum;
	int c = 0;
	int i;

	dire = &dirbuf[dirc];
	filee->HighClust = letoh16(dire->dirHighClust);
	filee->StartCluster = letoh16(dire->dirStartCluster);
	filee->FileSize = letoh32(dire->dirFileSize);
	parseShortFilename(dire, filee->shortName);
	chksum = shortNameChkSum(filee->shortName);

	if (dirc != 0) {
		u_int8_t buffer[26];
		u_int16_t *bp;
		int j = 0;
		for (i = dirc; i != 0; i--) {
			wine = (struct winentry *)&dirbuf[i-1];
			memset(buffer,0, sizeof(buffer));
			memcpy(&buffer[0], wine->wePart1, sizeof(wine->wePart1));
			memcpy(&buffer[sizeof(wine->wePart1)],wine->wePart2,  sizeof(wine->wePart2));
			memcpy(&buffer[sizeof(wine->wePart1) + sizeof(wine->wePart2)],wine->wePart3,  sizeof(wine->wePart3));
			bp = (u_int16_t *)buffer;
			for (j = 0; j < 13; j++, c++) 
			{
				longName[c] = (u_int8_t)letoh16(bp[j]);
				if(longName[c] == '\n')
					longName[c] = '_';
				if (longName[c] == '\0')
					goto done;
			}
		}
		longName[c] = '\0';
done:
		strcpy(filee->longName, longName);
	}
	return (1);
}

int parseShortFilename(struct direntry *dire, char *name)
{
	int j;

	for (j = 0; j < 8; j++) 
	{
		if (dire->dirName[j] == ' ')
			break;
		*name++ = dire->dirName[j];
	}
	if (dire->dirExtension[0] != ' ')
		*name++ = '.';
	for (j = 0; j < 3; j++) 
	{
		if (dire->dirExtension[j] == ' ')
			break;
		*name++ = dire->dirExtension[j];
	}
	*name = '\0';

	return (-1);
}

#ifdef FAT_DEBUG
int fat_dump_fatsc(struct fat_sc *fsc)
{
	printf("Bytes per Sector = %d\n", fsc->BytesPerSec);
	printf("Sectors Per Cluster = %d\n", fsc->SecPerClust);
	printf("Number of reserved sectors = %d\n", fsc->ResSectors);
	printf("Number of FATs = %d\n", fsc->NumFATs);
	printf("Number of Root directory entries = %d\n", fsc->RootDirEnts);
	printf("Total number of sectors = %d\n", fsc->TotalSectors);
	printf("Number of sectors per FAT = %d\n", fsc->FATsecs);
	printf("Sectors per track = %d\n", fsc->SecPerTrack);
	printf("Number of hidden sectors = %d\n", fsc->HiddenSecs);
	printf("Number of Root directory sectors = %d\n", fsc->RootDirSectors);
	printf("Count of clusters = %d\n", fsc->CountOfClusters);
	printf("Clustersize = %d\n", fsc->ClusterSize);
	printf("First Root directory sector = %d\n", fsc->FirstRootDirSecNum);
	printf("Total Data Sectors = %d\n", fsc->DataSectors);
	printf("Data Sector base = %d\n", fsc->DataSectorBase);
	return (1);
}

int fat_dump_fileentry(struct fat_sc *fsc)
{
	int i;
	
	printf("        File: %s\n", fsc->file);
	printf("   HighClust: %d\n", fsc->file.HighClust);
	printf("StartCluster: %d\n", fsc->file.StartCluster);
	printf("    FileSize: %d\n", fsc->file.FileSize);

	if (fsc->file.Chain.count) {
		printf("Cluster chain: ");
		for (i = 0; i < fsc->file.Chain.count; i++) {
			printf("0x%08x ", fsc->file.Chain.entries[i]);
		}
		printf("\n");
	}
	return (1);
}
#endif /* FAT_DEBUG */

static FileSystem fatfs = {
        "fat", FS_FILE,
        fat_open,
        fat_read,
        fat_write,
        fat_lseek,
        fat_close,
	NULL
};

void init_fs_fat(void)
{
	filefs_init(&fatfs);
	//diskfs_init(&diskfile);
}










