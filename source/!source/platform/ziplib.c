//=======================================================================
//			Copyright (C) XashXT Group 2006
//			     All Rights Reserved
//=======================================================================

#include "platform.h"
#include "zip32.h"

#define BASE 65521UL    // largest prime smaller than 65536
#define NMAX 5552

//crc 32
#define DO1 crc = crc_table[0][((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8)
#define DO8 DO1; DO1; DO1; DO1; DO1; DO1; DO1; DO1

//adler 32
#define BASE 65521UL    /* largest prime smaller than 65536 */
#define ADO1(buf,i)  {adler += (buf)[i]; sum2 += adler;}
#define ADO2(buf,i)  ADO1(buf,i); ADO1(buf,i+1);
#define ADO4(buf,i)  ADO2(buf,i); ADO2(buf,i+2);
#define ADO8(buf,i)  ADO4(buf,i); ADO4(buf,i+4);
#define ADO16(buf)   ADO8(buf,0); ADO8(buf,8);
#define MOD(a) a %= BASE
#define MOD4(a) a %= BASE

//memory opertions
byte* zcalloc (byte* opaque, unsigned items, unsigned size)
{
	if (opaque) items += size - size; // make compiler happy
	return sizeof(dword) > 2 ? (byte*)malloc(items * size) : (byte*)calloc(items, size);
}

void zcfree (byte* opaque, byte* ptr)
{
	free(ptr);
	if (opaque) return; /* make compiler happy */
}

static const code lenfix[512] = {
	{96,7,0},{0,8,80},{0,8,16},{20,8,115},{18,7,31},{0,8,112},{0,8,48},
	{0,9,192},{16,7,10},{0,8,96},{0,8,32},{0,9,160},{0,8,0},{0,8,128},
	{0,8,64},{0,9,224},{16,7,6},{0,8,88},{0,8,24},{0,9,144},{19,7,59},
	{0,8,120},{0,8,56},{0,9,208},{17,7,17},{0,8,104},{0,8,40},{0,9,176},
	{0,8,8},{0,8,136},{0,8,72},{0,9,240},{16,7,4},{0,8,84},{0,8,20},
	{21,8,227},{19,7,43},{0,8,116},{0,8,52},{0,9,200},{17,7,13},{0,8,100},
	{0,8,36},{0,9,168},{0,8,4},{0,8,132},{0,8,68},{0,9,232},{16,7,8},
	{0,8,92},{0,8,28},{0,9,152},{20,7,83},{0,8,124},{0,8,60},{0,9,216},
	{18,7,23},{0,8,108},{0,8,44},{0,9,184},{0,8,12},{0,8,140},{0,8,76},
	{0,9,248},{16,7,3},{0,8,82},{0,8,18},{21,8,163},{19,7,35},{0,8,114},
	{0,8,50},{0,9,196},{17,7,11},{0,8,98},{0,8,34},{0,9,164},{0,8,2},
	{0,8,130},{0,8,66},{0,9,228},{16,7,7},{0,8,90},{0,8,26},{0,9,148},
	{20,7,67},{0,8,122},{0,8,58},{0,9,212},{18,7,19},{0,8,106},{0,8,42},
	{0,9,180},{0,8,10},{0,8,138},{0,8,74},{0,9,244},{16,7,5},{0,8,86},
	{0,8,22},{64,8,0},{19,7,51},{0,8,118},{0,8,54},{0,9,204},{17,7,15},
	{0,8,102},{0,8,38},{0,9,172},{0,8,6},{0,8,134},{0,8,70},{0,9,236},
	{16,7,9},{0,8,94},{0,8,30},{0,9,156},{20,7,99},{0,8,126},{0,8,62},
	{0,9,220},{18,7,27},{0,8,110},{0,8,46},{0,9,188},{0,8,14},{0,8,142},
	{0,8,78},{0,9,252},{96,7,0},{0,8,81},{0,8,17},{21,8,131},{18,7,31},
	{0,8,113},{0,8,49},{0,9,194},{16,7,10},{0,8,97},{0,8,33},{0,9,162},
	{0,8,1},{0,8,129},{0,8,65},{0,9,226},{16,7,6},{0,8,89},{0,8,25},
	{0,9,146},{19,7,59},{0,8,121},{0,8,57},{0,9,210},{17,7,17},{0,8,105},
	{0,8,41},{0,9,178},{0,8,9},{0,8,137},{0,8,73},{0,9,242},{16,7,4},
	{0,8,85},{0,8,21},{16,8,258},{19,7,43},{0,8,117},{0,8,53},{0,9,202},
	{17,7,13},{0,8,101},{0,8,37},{0,9,170},{0,8,5},{0,8,133},{0,8,69},
	{0,9,234},{16,7,8},{0,8,93},{0,8,29},{0,9,154},{20,7,83},{0,8,125},
	{0,8,61},{0,9,218},{18,7,23},{0,8,109},{0,8,45},{0,9,186},{0,8,13},
	{0,8,141},{0,8,77},{0,9,250},{16,7,3},{0,8,83},{0,8,19},{21,8,195},
	{19,7,35},{0,8,115},{0,8,51},{0,9,198},{17,7,11},{0,8,99},{0,8,35},
	{0,9,166},{0,8,3},{0,8,131},{0,8,67},{0,9,230},{16,7,7},{0,8,91},
	{0,8,27},{0,9,150},{20,7,67},{0,8,123},{0,8,59},{0,9,214},{18,7,19},
	{0,8,107},{0,8,43},{0,9,182},{0,8,11},{0,8,139},{0,8,75},{0,9,246},
	{16,7,5},{0,8,87},{0,8,23},{64,8,0},{19,7,51},{0,8,119},{0,8,55},
	{0,9,206},{17,7,15},{0,8,103},{0,8,39},{0,9,174},{0,8,7},{0,8,135},
	{0,8,71},{0,9,238},{16,7,9},{0,8,95},{0,8,31},{0,9,158},{20,7,99},
	{0,8,127},{0,8,63},{0,9,222},{18,7,27},{0,8,111},{0,8,47},{0,9,190},
	{0,8,15},{0,8,143},{0,8,79},{0,9,254},{96,7,0},{0,8,80},{0,8,16},
	{20,8,115},{18,7,31},{0,8,112},{0,8,48},{0,9,193},{16,7,10},{0,8,96},
	{0,8,32},{0,9,161},{0,8,0},{0,8,128},{0,8,64},{0,9,225},{16,7,6},
	{0,8,88},{0,8,24},{0,9,145},{19,7,59},{0,8,120},{0,8,56},{0,9,209},
	{17,7,17},{0,8,104},{0,8,40},{0,9,177},{0,8,8},{0,8,136},{0,8,72},
	{0,9,241},{16,7,4},{0,8,84},{0,8,20},{21,8,227},{19,7,43},{0,8,116},
	{0,8,52},{0,9,201},{17,7,13},{0,8,100},{0,8,36},{0,9,169},{0,8,4},
	{0,8,132},{0,8,68},{0,9,233},{16,7,8},{0,8,92},{0,8,28},{0,9,153},
	{20,7,83},{0,8,124},{0,8,60},{0,9,217},{18,7,23},{0,8,108},{0,8,44},
	{0,9,185},{0,8,12},{0,8,140},{0,8,76},{0,9,249},{16,7,3},{0,8,82},
	{0,8,18},{21,8,163},{19,7,35},{0,8,114},{0,8,50},{0,9,197},{17,7,11},
	{0,8,98},{0,8,34},{0,9,165},{0,8,2},{0,8,130},{0,8,66},{0,9,229},
	{16,7,7},{0,8,90},{0,8,26},{0,9,149},{20,7,67},{0,8,122},{0,8,58},
	{0,9,213},{18,7,19},{0,8,106},{0,8,42},{0,9,181},{0,8,10},{0,8,138},
	{0,8,74},{0,9,245},{16,7,5},{0,8,86},{0,8,22},{64,8,0},{19,7,51},
	{0,8,118},{0,8,54},{0,9,205},{17,7,15},{0,8,102},{0,8,38},{0,9,173},
	{0,8,6},{0,8,134},{0,8,70},{0,9,237},{16,7,9},{0,8,94},{0,8,30},
	{0,9,157},{20,7,99},{0,8,126},{0,8,62},{0,9,221},{18,7,27},{0,8,110},
	{0,8,46},{0,9,189},{0,8,14},{0,8,142},{0,8,78},{0,9,253},{96,7,0},
	{0,8,81},{0,8,17},{21,8,131},{18,7,31},{0,8,113},{0,8,49},{0,9,195},
	{16,7,10},{0,8,97},{0,8,33},{0,9,163},{0,8,1},{0,8,129},{0,8,65},
	{0,9,227},{16,7,6},{0,8,89},{0,8,25},{0,9,147},{19,7,59},{0,8,121},
	{0,8,57},{0,9,211},{17,7,17},{0,8,105},{0,8,41},{0,9,179},{0,8,9},
	{0,8,137},{0,8,73},{0,9,243},{16,7,4},{0,8,85},{0,8,21},{16,8,258},
	{19,7,43},{0,8,117},{0,8,53},{0,9,203},{17,7,13},{0,8,101},{0,8,37},
	{0,9,171},{0,8,5},{0,8,133},{0,8,69},{0,9,235},{16,7,8},{0,8,93},
	{0,8,29},{0,9,155},{20,7,83},{0,8,125},{0,8,61},{0,9,219},{18,7,23},
	{0,8,109},{0,8,45},{0,9,187},{0,8,13},{0,8,141},{0,8,77},{0,9,251},
	{16,7,3},{0,8,83},{0,8,19},{21,8,195},{19,7,35},{0,8,115},{0,8,51},
	{0,9,199},{17,7,11},{0,8,99},{0,8,35},{0,9,167},{0,8,3},{0,8,131},
	{0,8,67},{0,9,231},{16,7,7},{0,8,91},{0,8,27},{0,9,151},{20,7,67},
	{0,8,123},{0,8,59},{0,9,215},{18,7,19},{0,8,107},{0,8,43},{0,9,183},
	{0,8,11},{0,8,139},{0,8,75},{0,9,247},{16,7,5},{0,8,87},{0,8,23},
	{64,8,0},{19,7,51},{0,8,119},{0,8,55},{0,9,207},{17,7,15},{0,8,103},
	{0,8,39},{0,9,175},{0,8,7},{0,8,135},{0,8,71},{0,9,239},{16,7,9},
	{0,8,95},{0,8,31},{0,9,159},{20,7,99},{0,8,127},{0,8,63},{0,9,223},
	{18,7,27},{0,8,111},{0,8,47},{0,9,191},{0,8,15},{0,8,143},{0,8,79},
	{0,9,255}
};

static const code distfix[32] = {
	{16,5,1},{23,5,257},{19,5,17},{27,5,4097},{17,5,5},{25,5,1025},
	{21,5,65},{29,5,16385},{16,5,3},{24,5,513},{20,5,33},{28,5,8193},
	{18,5,9},{26,5,2049},{22,5,129},{64,5,0},{16,5,2},{23,5,385},
	{19,5,25},{27,5,6145},{17,5,7},{25,5,1537},{21,5,97},{29,5,24577},
	{16,5,4},{24,5,769},{20,5,49},{28,5,12289},{18,5,13},{26,5,3073},
	{22,5,193},{64,5,0}
};

//crc32 table
const unsigned long crc_table[1][256] = {
	0x00000000UL, 0x77073096UL, 0xee0e612cUL, 0x990951baUL, 0x076dc419UL,
	0x706af48fUL, 0xe963a535UL, 0x9e6495a3UL, 0x0edb8832UL, 0x79dcb8a4UL,
	0xe0d5e91eUL, 0x97d2d988UL, 0x09b64c2bUL, 0x7eb17cbdUL, 0xe7b82d07UL,
	0x90bf1d91UL, 0x1db71064UL, 0x6ab020f2UL, 0xf3b97148UL, 0x84be41deUL,
	0x1adad47dUL, 0x6ddde4ebUL, 0xf4d4b551UL, 0x83d385c7UL, 0x136c9856UL,
	0x646ba8c0UL, 0xfd62f97aUL, 0x8a65c9ecUL, 0x14015c4fUL, 0x63066cd9UL,
	0xfa0f3d63UL, 0x8d080df5UL, 0x3b6e20c8UL, 0x4c69105eUL, 0xd56041e4UL,
	0xa2677172UL, 0x3c03e4d1UL, 0x4b04d447UL, 0xd20d85fdUL, 0xa50ab56bUL,
	0x35b5a8faUL, 0x42b2986cUL, 0xdbbbc9d6UL, 0xacbcf940UL, 0x32d86ce3UL,
	0x45df5c75UL, 0xdcd60dcfUL, 0xabd13d59UL, 0x26d930acUL, 0x51de003aUL,
	0xc8d75180UL, 0xbfd06116UL, 0x21b4f4b5UL, 0x56b3c423UL, 0xcfba9599UL,
	0xb8bda50fUL, 0x2802b89eUL, 0x5f058808UL, 0xc60cd9b2UL, 0xb10be924UL,
	0x2f6f7c87UL, 0x58684c11UL, 0xc1611dabUL, 0xb6662d3dUL, 0x76dc4190UL,
	0x01db7106UL, 0x98d220bcUL, 0xefd5102aUL, 0x71b18589UL, 0x06b6b51fUL,
	0x9fbfe4a5UL, 0xe8b8d433UL, 0x7807c9a2UL, 0x0f00f934UL, 0x9609a88eUL,
	0xe10e9818UL, 0x7f6a0dbbUL, 0x086d3d2dUL, 0x91646c97UL, 0xe6635c01UL,
	0x6b6b51f4UL, 0x1c6c6162UL, 0x856530d8UL, 0xf262004eUL, 0x6c0695edUL,
	0x1b01a57bUL, 0x8208f4c1UL, 0xf50fc457UL, 0x65b0d9c6UL, 0x12b7e950UL,
	0x8bbeb8eaUL, 0xfcb9887cUL, 0x62dd1ddfUL, 0x15da2d49UL, 0x8cd37cf3UL,
	0xfbd44c65UL, 0x4db26158UL, 0x3ab551ceUL, 0xa3bc0074UL, 0xd4bb30e2UL,
	0x4adfa541UL, 0x3dd895d7UL, 0xa4d1c46dUL, 0xd3d6f4fbUL, 0x4369e96aUL,
	0x346ed9fcUL, 0xad678846UL, 0xda60b8d0UL, 0x44042d73UL, 0x33031de5UL,
	0xaa0a4c5fUL, 0xdd0d7cc9UL, 0x5005713cUL, 0x270241aaUL, 0xbe0b1010UL,
	0xc90c2086UL, 0x5768b525UL, 0x206f85b3UL, 0xb966d409UL, 0xce61e49fUL,
	0x5edef90eUL, 0x29d9c998UL, 0xb0d09822UL, 0xc7d7a8b4UL, 0x59b33d17UL,
	0x2eb40d81UL, 0xb7bd5c3bUL, 0xc0ba6cadUL, 0xedb88320UL, 0x9abfb3b6UL,
	0x03b6e20cUL, 0x74b1d29aUL, 0xead54739UL, 0x9dd277afUL, 0x04db2615UL,
	0x73dc1683UL, 0xe3630b12UL, 0x94643b84UL, 0x0d6d6a3eUL, 0x7a6a5aa8UL,
	0xe40ecf0bUL, 0x9309ff9dUL, 0x0a00ae27UL, 0x7d079eb1UL, 0xf00f9344UL,
	0x8708a3d2UL, 0x1e01f268UL, 0x6906c2feUL, 0xf762575dUL, 0x806567cbUL,
	0x196c3671UL, 0x6e6b06e7UL, 0xfed41b76UL, 0x89d32be0UL, 0x10da7a5aUL,
	0x67dd4accUL, 0xf9b9df6fUL, 0x8ebeeff9UL, 0x17b7be43UL, 0x60b08ed5UL,
	0xd6d6a3e8UL, 0xa1d1937eUL, 0x38d8c2c4UL, 0x4fdff252UL, 0xd1bb67f1UL,
	0xa6bc5767UL, 0x3fb506ddUL, 0x48b2364bUL, 0xd80d2bdaUL, 0xaf0a1b4cUL,
	0x36034af6UL, 0x41047a60UL, 0xdf60efc3UL, 0xa867df55UL, 0x316e8eefUL,
	0x4669be79UL, 0xcb61b38cUL, 0xbc66831aUL, 0x256fd2a0UL, 0x5268e236UL,
	0xcc0c7795UL, 0xbb0b4703UL, 0x220216b9UL, 0x5505262fUL, 0xc5ba3bbeUL,
	0xb2bd0b28UL, 0x2bb45a92UL, 0x5cb36a04UL, 0xc2d7ffa7UL, 0xb5d0cf31UL,
	0x2cd99e8bUL, 0x5bdeae1dUL, 0x9b64c2b0UL, 0xec63f226UL, 0x756aa39cUL,
	0x026d930aUL, 0x9c0906a9UL, 0xeb0e363fUL, 0x72076785UL, 0x05005713UL,
	0x95bf4a82UL, 0xe2b87a14UL, 0x7bb12baeUL, 0x0cb61b38UL, 0x92d28e9bUL,
	0xe5d5be0dUL, 0x7cdcefb7UL, 0x0bdbdf21UL, 0x86d3d2d4UL, 0xf1d4e242UL,
	0x68ddb3f8UL, 0x1fda836eUL, 0x81be16cdUL, 0xf6b9265bUL, 0x6fb077e1UL,
	0x18b74777UL, 0x88085ae6UL, 0xff0f6a70UL, 0x66063bcaUL, 0x11010b5cUL,
	0x8f659effUL, 0xf862ae69UL, 0x616bffd3UL, 0x166ccf45UL, 0xa00ae278UL,
	0xd70dd2eeUL, 0x4e048354UL, 0x3903b3c2UL, 0xa7672661UL, 0xd06016f7UL,
	0x4969474dUL, 0x3e6e77dbUL, 0xaed16a4aUL, 0xd9d65adcUL, 0x40df0b66UL,
	0x37d83bf0UL, 0xa9bcae53UL, 0xdebb9ec5UL, 0x47b2cf7fUL, 0x30b5ffe9UL,
	0xbdbdf21cUL, 0xcabac28aUL, 0x53b39330UL, 0x24b4a3a6UL, 0xbad03605UL,
	0xcdd70693UL, 0x54de5729UL, 0x23d967bfUL, 0xb3667a2eUL, 0xc4614ab8UL,
	0x5d681b02UL, 0x2a6f2b94UL, 0xb40bbe37UL, 0xc30c8ea1UL, 0x5a05df1bUL,
	0x2d02ef8dUL
};

unsigned long crc32(unsigned long crc, const unsigned char *buf, unsigned len)
{
	if (buf == NULL) return 0UL;

	crc = crc ^ 0xffffffffUL;
	while (len >= 8)
	{
		DO8;
		len -= 8;
	}
	if (len) do
	{
		DO1;
	}while (--len);

	return crc ^ 0xffffffffUL;
}

dword adler32(dword adler, const byte *buf, dword len)
{
	unsigned long sum2;
	unsigned n;

	/* split Adler-32 into component sums */
	sum2 = (adler >> 16) & 0xffff;
	adler &= 0xffff;

	/* in case user likes doing a byte at a time, keep it fast */
	if (len == 1)
	{
		adler += buf[0];
		if (adler >= BASE) adler -= BASE;
		sum2 += adler;
		if (sum2 >= BASE) sum2 -= BASE;
		return adler | (sum2 << 16);
	}

	/* initial Adler-32 value (deferred check for len == 1 speed) */
	if (buf == NULL) return 1L;

	/* in case short lengths are provided, keep it somewhat fast */
	if (len < 16)
	{
		while (len--)
		{
			adler += *buf++;
			sum2 += adler;
		}

		if (adler >= BASE) adler -= BASE;
		MOD4(sum2);             /* only added so many BASE's */
		return adler | (sum2 << 16);
	}

	/* do length NMAX blocks -- requires just one modulo operation */
	while (len >= NMAX)
	{
		len -= NMAX;
		n = NMAX / 16;          /* NMAX is divisible by 16 */
		do
		{
		ADO16(buf);          /* 16 sums unrolled */
		buf += 16;
		}while (--n);
        		MOD(adler);
		MOD(sum2);
	}

	/* do remaining bytes (less than NMAX, still just one modulo) */
	if (len) // avoid modulos if none remaining
	{
		while (len >= 16)
		{
			len -= 16;
			ADO16(buf);
			buf += 16;
		}
		while (len--)
		{
			adler += *buf++;
			sum2 += adler;
		}
		MOD(adler);
		MOD(sum2);
	}

	// return recombined sums
	return adler | (sum2 << 16);
}

int inflateInit_(z_streamp strm, const char *version, int stream_size )
{
	//internal checking
	return inflateInit2_(strm, MAX_WBITS, version, stream_size);
}

int inflateInit2_(z_streamp strm, int windowBits, const char *version, int stream_size)
{
	struct inflate_state *state;

	if (version == NULL || version[0] != ZLIB_VERSION[0] || stream_size != (int)(sizeof(z_stream)))
		return Z_VERSION_ERROR;
    
	if (strm == NULL) return Z_STREAM_ERROR;
	strm->msg = NULL; //in case we return an error
	if (strm->zalloc == (alloc_func)0)
	{
		strm->zalloc = zcalloc;
		strm->opaque = (byte*)0;
    	}

	if (strm->zfree == (free_func)0) strm->zfree = zcfree;
	state = (struct inflate_state *) 
	(*((strm)->zalloc))((strm)->opaque, (1), (sizeof(struct inflate_state)));
	if (state == NULL) return Z_MEM_ERROR;
	strm->state = (struct int_state *)state;
	if (windowBits < 0)
	{
		state->wrap = 0;
		windowBits = -windowBits;
	}
	else state->wrap = (windowBits >> 4) + 1;
	if (windowBits < 8 || windowBits > 15)
	{
		(*((strm)->zfree))((strm)->opaque, (byte*)(state));
		strm->state = NULL;
		return Z_STREAM_ERROR;
	}

	state->wbits = (unsigned)windowBits;
	state->window = NULL;
	return inflateReset(strm);
}

int inflate_table(codetype type, unsigned short *lens, unsigned codes, code **table, unsigned *bits, unsigned short *work)
{
	unsigned len;               /* a code's length in bits */
	unsigned sym;               /* index of code symbols */
	unsigned min, max;          /* minimum and maximum code lengths */
	unsigned root;              /* number of index bits for root table */
	unsigned curr;              /* number of index bits for current table */
	unsigned drop;              /* code bits to drop for sub-table */
	int left;                   /* number of prefix codes available */
	unsigned used;              /* code entries in table used */
	unsigned huff;              /* Huffman code */
	unsigned incr;              /* for incrementing code, index */
	unsigned fill;              /* index for replicating entries */
	unsigned low;               /* low bits for current root entry */
	unsigned mask;              /* mask for low root bits */
	code this;                  /* table entry for duplication */
	code *next;                 /* next available space in table */
	const unsigned short *base; /* base value table to use */
	const unsigned short *extra;/* extra bits table to use */
	int end;                    /* use base and extra for symbol > end */
	unsigned short count[MAX_WBITS+1];    /* number of codes of each length */
	unsigned short offs[MAX_WBITS+1];     /* offsets in table for each length */
	static const unsigned short lbase[31] = { /* Length codes 257..285 base */
	3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
	35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0};
	static const unsigned short lext[31] = { /* Length codes 257..285 extra */
	16, 16, 16, 16, 16, 16, 16, 16, 17, 17, 17, 17, 18, 18, 18, 18,
	19, 19, 19, 19, 20, 20, 20, 20, 21, 21, 21, 21, 16, 201, 196};
	static const unsigned short dbase[32] = { /* Distance codes 0..29 base */
	1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
	257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
	8193, 12289, 16385, 24577, 0, 0};
	static const unsigned short dext[32] = { /* Distance codes 0..29 extra */
	16, 16, 16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22,
	23, 23, 24, 24, 25, 25, 26, 26, 27, 27,
	28, 28, 29, 29, 64, 64};

	/* accumulate lengths for codes (assumes lens[] all in 0..MAXBITS) */
	for (len = 0; len <= MAX_WBITS; len++) count[len] = 0;
	for (sym = 0; sym < codes; sym++) count[lens[sym]]++;

	/* bound code lengths, force root to be within code lengths */
	root = *bits;
	for (max = MAX_WBITS; max >= 1; max--) if (count[max] != 0) break;
	if (root > max) root = max;
	if (max == 0)                     /* no symbols to code at all */
	{
		this.op = (unsigned char)64;    /* invalid code marker */
		this.bits = (unsigned char)1;
		this.val = (unsigned short)0;
		*(*table)++ = this;             /* make a table to force an error */
		*(*table)++ = this;
		*bits = 1;
		return 0;     /* no symbols, but wait for decoding to report error */
	}
	for (min = 1; min <= MAX_WBITS; min++) if (count[min] != 0) break;
	if (root < min) root = min;

	/* check for an over-subscribed or incomplete set of lengths */
	left = 1;
	for (len = 1; len <= MAX_WBITS; len++)
	{
		left <<= 1;
		left -= count[len];
		if (left < 0) return -1;        /* over-subscribed */
	}
	if (left > 0 && (type == CODES || max != 1)) return -1;  /* incomplete set */

	/* generate offsets into symbol table for each length for sorting */
	offs[1] = 0;
	for (len = 1; len < MAX_WBITS; len++) offs[len + 1] = offs[len] + count[len];

	/* sort symbols by length, by symbol order within each length */
	for (sym = 0; sym < codes; sym++) if (lens[sym] != 0) work[offs[lens[sym]]++] = (unsigned short)sym;


	/* set up for code type */
	switch (type)
	{
	case CODES:
		base = extra = work;    /* dummy value--not used */
		end = 19;
		break;
	case LENS:
		base = lbase;
		base -= 257;
		extra = lext;
		extra -= 257;
		end = 256;
		break;
	default:            /* DISTS */
		base = dbase;
		extra = dext;
		end = -1;
	}

	/* initialize state for loop */
	huff = 0;                   /* starting code */
	sym = 0;                    /* starting code symbol */
	len = min;                  /* starting code length */
	next = *table;              /* current table to fill in */
	curr = root;                /* current table index bits */
	drop = 0;                   /* current bits to drop from code for index */
	low = (unsigned)(-1);       /* trigger new sub-table when len > root */
	used = 1U << root;          /* use root table entries */
	mask = used - 1;            /* mask for comparing low */

	/* check available table space */
	if (type == LENS && used >= 2048 - 592) return 1;

	/* process all codes and make table entries */
	for (;;)
	{
		/* create table entry */
		this.bits = (unsigned char)(len - drop);
		if ((int)(work[sym]) < end)
		{
			this.op = (unsigned char)0;
			this.val = work[sym];
		}
		else if ((int)(work[sym]) > end)
		{
			this.op = (unsigned char)(extra[work[sym]]);
			this.val = base[work[sym]];
		}
		else
		{
			this.op = (unsigned char)(32 + 64);         /* end of block */
			this.val = 0;
		}

		/* replicate for those indices with low len bits equal to huff */
		incr = 1U << (len - drop);
		fill = 1U << curr;
		min = fill;                 /* save offset to next table */
		do{
            	fill -= incr;
            	next[(huff >> drop) + fill] = this;
		}while (fill != 0);

		/* backwards increment the len-bit code huff */
		incr = 1U << (len - 1);
		while (huff & incr) incr >>= 1;
		if (incr != 0)
		{
			huff &= incr - 1;
			huff += incr;
		}
		else huff = 0;

		/* go to next symbol, update count, len */
		sym++;
		if (--(count[len]) == 0)
		{
			if (len == max) break;
			len = lens[work[sym]];
		}

		/* create new sub-table if needed */
		if (len > root && (huff & mask) != low)
		{
			/* if first time, transition to sub-tables */
			if (drop == 0) drop = root;
			/* increment past last table */
			next += min; /* here min is 1 << curr */

			/* determine length of next table */
			curr = len - drop;
			left = (int)(1 << curr);
			while (curr + drop < max)
			{
				left -= count[curr + drop];
				if (left <= 0) break;
				curr++;
				left <<= 1;
			}

			/* check for enough space */
			used += 1U << curr;
			if (type == LENS && used >= 2048 - 592) return 1;

			/* point entry in root table to sub-table */
			low = huff & mask;
			(*table)[low].op = (unsigned char)curr;
			(*table)[low].bits = (unsigned char)root;
			(*table)[low].val = (unsigned short)(next - *table);
		}
	}

	this.op = (unsigned char)64;                /* invalid code marker */
	this.bits = (unsigned char)(len - drop);
	this.val = (unsigned short)0;
	while (huff != 0)
	{
		/* when done with sub-table, drop back to root table */
		if (drop != 0 && (huff & mask) != low)
		{
			drop = 0;
			len = root;
			next = *table;
			this.bits = (unsigned char)len;
		}

		/* put invalid code marker in table */
		next[huff >> drop] = this;

		/* backwards increment the len-bit code huff */
		incr = 1U << (len - 1);
		while (huff & incr) incr >>= 1;
		if (incr != 0)
		{
			huff &= incr - 1;
			huff += incr;
		}
		else huff = 0;
	}

	/* set return parameters */
	*table += used;
	*bits = root;
	return 0;
}

void inflate_fast(z_streamp strm, unsigned start)
{
	struct inflate_state *state;
	unsigned char *in;      /* local strm->next_in */
	unsigned char *last;    /* while in < last, enough input available */
	unsigned char *out;     /* local strm->next_out */
	unsigned char *beg;     /* inflate()'s initial strm->next_out */
	unsigned char *end;     /* while out < end, enough space available */
	unsigned wsize;             /* window size or zero if not using window */
	unsigned whave;             /* valid bytes in the window */
	unsigned write;             /* window write index */
	unsigned char *window;      /* allocated sliding window, if wsize != 0 */
	unsigned long hold;         /* local strm->hold */
	unsigned bits;              /* local strm->bits */
	code const *lcode;          /* local strm->lencode */
	code const *dcode;          /* local strm->distcode */
	unsigned lmask;             /* mask for first level of length codes */
	unsigned dmask;             /* mask for first level of distance codes */
	code this;                  /* retrieved table entry */
	unsigned op;                /* code bits, operation, extra bits, or */
                                      /*  window position, window bytes to copy */
	unsigned len;               /* match length, unused bytes */
	unsigned dist;              /* match distance */
	unsigned char *from;    /* where to copy match from */

	/* copy state to local variables */
	state = (struct inflate_state *)strm->state;
	in = strm->next_in;
	last = in + (strm->avail_in - 5);
	out = strm->next_out;
	beg = out - (start - strm->avail_out);
	end = out + (strm->avail_out - 257);
	wsize = state->wsize;
	whave = state->whave;
	write = state->write;
	window = state->window;
	hold = state->hold;
	bits = state->bits;
	lcode = state->lencode;
	dcode = state->distcode;
	lmask = (1U << state->lenbits) - 1;
	dmask = (1U << state->distbits) - 1;

	/* decode literals and length/distances until end-of-block or not enough
	input data or output space */
	do {
	if (bits < 15)
	{
		hold += (unsigned long)(PUP(in)) << bits;
		bits += 8;
		hold += (unsigned long)(PUP(in)) << bits;
		bits += 8;
	}
	this = lcode[hold & lmask];
dolen:
	op = (unsigned)(this.bits);
	hold >>= op;
	bits -= op;
	op = (unsigned)(this.op);
	if (op == 0) PUP(out) = (byte)(this.val);
	else if (op & 16)                     /* length base */
	{
		len = (unsigned)(this.val);
		op &= 15;                   /* number of extra bits */
		if (op)
		{
			if (bits < op)
			{
				hold += (unsigned long)(PUP(in)) << bits;
				bits += 8;
			}
			len += (unsigned)hold & ((1U << op) - 1);
			hold >>= op;
			bits -= op;
		}
		if (bits < 15)
		{
			hold += (dword)(PUP(in)) << bits;
			bits += 8;
			hold += (dword)(PUP(in)) << bits;
			bits += 8;
		}
		this = dcode[hold & dmask];
dodist:
		op = (unsigned)(this.bits);
		hold >>= op;
		bits -= op;
		op = (unsigned)(this.op);
		if (op & 16)                      /* distance base */
		{                
			dist = (unsigned)(this.val);
			op &= 15;               /* number of extra bits */
			if (bits < op)
			{
				hold += (unsigned long)(PUP(in)) << bits;
				bits += 8;
				if (bits < op)
				{
					hold += (unsigned long)(PUP(in)) << bits;
					bits += 8;
				}
			}
			dist += (unsigned)hold & ((1U << op) - 1);
			hold >>= op;
			bits -= op;
			op = (unsigned)(out - beg);     /* max distance in output */
			if (dist > op)                  /* see if copy from window */
			{
				op = dist - op;       /* distance back in window */
				if (op > whave)
				{
					strm->msg = (char *)"invalid distance too far back";
					state->mode = BAD;
					break;
				}
				from = window;
				if (write == 0)        // very common case
				{
					from += wsize - op;
					if (op < len) // some from window
					{
						len -= op;
						do{
						PUP(out) = PUP(from);
						}while (--op);
						from = out - dist;  // rest from output
					}
				}
				else if (write < op)      // wrap around window
				{
					from += wsize + write - op;
					op -= write;
					if (op < len)   /* some from end of window */
					{                            
						len -= op;
						do{
						PUP(out) = PUP(from);
						}while (--op);
						from = window;
						if (write < len)  /* some from start of window */
						{
							op = write;
							len -= op;
							do{
							PUP(out) = PUP(from);
							}while (--op);
							from = out - dist;      /* rest from output */
						}
					}
				}
				else                      /* contiguous in window */
				{
					from += write - op;
					if (op < len)   /* some from window */
					{
						len -= op;
						do{
						PUP(out) = PUP(from);
						}while (--op);
						from = out - dist;  /* rest from output */
					}
				}
				while (len > 2)
				{
					PUP(out) = PUP(from);
					PUP(out) = PUP(from);
					PUP(out) = PUP(from);
					len -= 3;
				}
				if (len)
				{
					PUP(out) = PUP(from);
					if (len > 1) PUP(out) = PUP(from);
				}
			}
			else
			{
				from = out - dist;		// copy direct from output
				do			// minimum length is three
				{
					PUP(out) = PUP(from);
					PUP(out) = PUP(from);
					PUP(out) = PUP(from);
					len -= 3;
				}while (len > 2);
				if (len)
				{
					PUP(out) = PUP(from);
					if (len > 1) PUP(out) = PUP(from);
				}
			}
		}
		else if ((op & 64) == 0)          // 2nd level distance code
		{
			this = dcode[this.val + (hold & ((1U << op) - 1))];
			goto dodist;
		}
		else
		{
			strm->msg = (char *)"invalid distance code";
			state->mode = BAD;
			break;
		}
	}
	else if ((op & 64) == 0)              //2nd level length code
	{
		this = lcode[this.val + (hold & ((1U << op) - 1))];
		goto dolen;
	}
	else if (op & 32)                     /* end-of-block */
	{
		state->mode = TYPE;
		break;
	}
	else
	{
		strm->msg = (char *)"invalid literal/length code";
		state->mode = BAD;
		break;
	}
	}while (in < last && out < end);

	// return unused bytes (on entry, bits < 8, so in won't go too far back)
	len = bits >> 3;
	in -= len;
	bits -= len << 3;
	hold &= (1U << bits) - 1;

	// update state and return
	strm->next_in = in;
	strm->next_out = out;
	strm->avail_in = (unsigned)(in < last ? 5 + (last - in) : 5 - (in - last));
	strm->avail_out = (unsigned)(out < end ? 257 + (end - out) : 257 - (out - end));
	state->hold = hold;
	state->bits = bits;
	return;  //???
}

void fixedtables(struct inflate_state *state)
{
	state->lencode = lenfix;
	state->lenbits = 9;
	state->distcode = distfix;
	state->distbits = 5;
}

int updatewindow(z_streamp strm, unsigned out)
{
	struct inflate_state *state;
	unsigned copy, dist;

	state = (struct inflate_state *)strm->state;

	/* if it hasn't been done already, allocate space for the window */
	if (state->window == NULL)
	{
		state->window = (unsigned char *)
		(*((strm)->zalloc))((strm)->opaque, (1U << state->wbits), (sizeof(unsigned char)));
		if (state->window == NULL) return 1;
	}

	/* if window not in use yet, initialize */
	if (state->wsize == 0)
	{
		state->wsize = 1U << state->wbits;
		state->write = 0;
		state->whave = 0;
	}

	/* copy state->wsize or less output bytes into the circular window */
	copy = out - strm->avail_out;
	if (copy >= state->wsize)
	{
		memcpy(state->window, strm->next_out - state->wsize, state->wsize);
		state->write = 0;
		state->whave = state->wsize;
	}
	else
	{
		dist = state->wsize - state->write;
		if (dist > copy) dist = copy;
		memcpy(state->window + state->write, strm->next_out - copy, dist);
		copy -= dist;

		if (copy)
		{
			memcpy(state->window, strm->next_out - copy, copy);
			state->write = copy;
			state->whave = state->wsize;
		}
		else
		{
			state->write += dist;
			if (state->write == state->wsize) state->write = 0;
			if (state->whave < state->wsize) state->whave += dist;
		}
	}
	return 0;
}

// Macros for inflate():

// check function to use adler32() for zlib or crc32() for gzip
#define UPDATE(check, buf, len) adler32(check, buf, len)

// Load registers with state in inflate() for speed
#define LOAD() \
	do { \
	put = strm->next_out; \
	left = strm->avail_out; \
	next = strm->next_in; \
	have = strm->avail_in; \
	hold = state->hold; \
	bits = state->bits; \
	} while (0)

// Restore state from registers in inflate()
#define RESTORE() \
	do { \
	strm->next_out = put; \
	strm->avail_out = left; \
	strm->next_in = next; \
	strm->avail_in = have; \
	state->hold = hold; \
	state->bits = bits; \
	} while (0)

// Clear the input bit accumulator
#define INITBITS() \
	do { \
	hold = 0; \
	bits = 0; \
	} while (0)

// Get a byte of input into the bit accumulator, or return from inflate()
// if there is no input available.
#define PULLBYTE() \
	do { \
	if (have == 0) goto inf_leave; \
	have--; \
	hold += (unsigned long)(*next++) << bits; \
	bits += 8; \
	} while (0)

// Assure that there are at least n bits in the bit accumulator.  If there is
// not enough available input to do that, then return from inflate().
#define NEEDBITS(n) \
	do { \
	while (bits < (unsigned)(n)) \
	PULLBYTE(); \
	} while (0)

// Return the low n bits of the bit accumulator (n < 16)
#define BITS(n) \
	((unsigned)hold & ((1U << (n)) - 1))

// Remove n bits from the bit accumulator
#define DROPBITS(n) \
	do { \
	hold >>= (n); \
	bits -= (unsigned)(n); \
	} while (0)

// Remove zero to seven bits as needed to go to a byte boundary
#define BYTEBITS() \
	do { \
	hold >>= bits & 7; \
	bits -= bits & 7; \
	} while (0)

// Reverse the bytes in a 32-bit value
#define REVERSE(q) \
	((((q) >> 24) & 0xff) + (((q) >> 8) & 0xff00) + \
	(((q) & 0xff00) << 8) + (((q) & 0xff) << 24))

int inflate(z_streamp strm, int flush)
{
	struct inflate_state *state;
	unsigned char *next;    /* next input */
	unsigned char *put;     /* next output */
	unsigned have, left;        /* available input and output */
	unsigned long hold;         /* bit buffer */
	unsigned bits;              /* bits in bit buffer */
	unsigned in, out;           /* save starting available input and output */
	unsigned copy;              /* number of stored or match bytes to copy */
	unsigned char *from;        /* where to copy match bytes from */
	code this;                  /* current decoding table entry */
	code last;                  /* parent table entry */
	unsigned len;               /* length to copy for repeats, bits to drop */
	int ret;                    /* return code */
          
	// permutation of code lengths
	static const unsigned short order[19] ={16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

	if (strm == NULL || strm->state == NULL || strm->next_out == NULL || (strm->next_in == NULL && strm->avail_in != 0))
		return Z_STREAM_ERROR;

	state = (struct inflate_state *)strm->state;
	if (state->mode == TYPE) state->mode = TYPEDO;      /* skip check */
	LOAD();
	in = have;
	out = left;
	ret = Z_OK;

	for (;;)
	switch (state->mode)
	{
	case HEAD:
		if (state->wrap == 0)
		{
			state->mode = TYPEDO;
			break;
		}
		NEEDBITS(16);

		if (((BITS(8) << 8) + (hold >> 8)) % 31) // check if zlib header allowed
		{
			strm->msg = (char *)"incorrect header check";
			state->mode = BAD;
			break;
		}
		if (BITS(4) != Z_DEFLATED)
		{
			strm->msg = (char *)"unknown compression method";
			state->mode = BAD;
			break;
		}
		DROPBITS(4);
		len = BITS(4) + 8;
		if (len > state->wbits)
		{
			strm->msg = (char *)"invalid window size";
			state->mode = BAD;
			break;
		}
		state->dmax = 1U << len;
		strm->adler = state->check = adler32(0L, NULL, 0);
		state->mode = hold & 0x200 ? DICTID : TYPE;
		INITBITS();
		break;
	case DICTID:
		NEEDBITS(32);
		strm->adler = state->check = REVERSE(hold);
		INITBITS();
		state->mode = DICT;
	case DICT:
		if (state->havedict == 0)
		{
			RESTORE();
			return Z_NEED_DICT;
		}
		strm->adler = state->check = adler32(0L, NULL, 0);
		state->mode = TYPE;
	case TYPE:
		if (flush == Z_BLOCK) goto inf_leave;
	case TYPEDO:
		if (state->last)
		{
			BYTEBITS();
			state->mode = CHECK;
			break;
		}
		NEEDBITS(3);
		state->last = BITS(1);
		DROPBITS(1);
		switch (BITS(2))
		{
		case 0:                             /* stored block */
			state->mode = STORED;
			break;
		case 1:                             /* fixed block */
			fixedtables(state);
			state->mode = LEN;              /* decode codes */
			break;
		case 2:                             /* dynamic block */
			state->mode = TABLE;
			break;
		case 3:
			strm->msg = (char *)"invalid block type";
			state->mode = BAD;
		}
		DROPBITS(2);
		break;
	case STORED:
		BYTEBITS();                         /* go to byte boundary */
		NEEDBITS(32);
		if ((hold & 0xffff) != ((hold >> 16) ^ 0xffff))
		{
			strm->msg = (char *)"invalid stored block lengths";
			state->mode = BAD;
			break;
		}
		state->length = (unsigned)hold & 0xffff;
		INITBITS();
		state->mode = COPY;
	case COPY:
		copy = state->length;
		if (copy)
		{
			if (copy > have) copy = have;
			if (copy > left) copy = left;
			if (copy == 0) goto inf_leave;
			memcpy(put, next, copy);
			have -= copy;
			next += copy;
			left -= copy;
			put += copy;
			state->length -= copy;
			break;
		}
		state->mode = TYPE;
		break;
	case TABLE:
		NEEDBITS(14);
		state->nlen = BITS(5) + 257;
		DROPBITS(5);
		state->ndist = BITS(5) + 1;
		DROPBITS(5);
		state->ncode = BITS(4) + 4;
		DROPBITS(4);
		if (state->nlen > 286 || state->ndist > 30)
		{
			strm->msg = (char *)"too many length or distance symbols";
			state->mode = BAD;
			break;
		}
		state->have = 0;
		state->mode = LENLENS;
	case LENLENS:
		while (state->have < state->ncode)
		{
			NEEDBITS(3);
			state->lens[order[state->have++]] = (unsigned short)BITS(3);
			DROPBITS(3);
		}
		while (state->have < 19) state->lens[order[state->have++]] = 0;
		state->next = state->codes;
		state->lencode = (code const *)(state->next);
		state->lenbits = 7;
		ret = inflate_table(CODES, state->lens, 19, &(state->next), &(state->lenbits), state->work);
		if (ret)
		{
			strm->msg = (char *)"invalid code lengths set";
			state->mode = BAD;
			break;
		}
		state->have = 0;
		state->mode = CODELENS;
	case CODELENS:
		while (state->have < state->nlen + state->ndist)
		{
			for (;;)
			{
				this = state->lencode[BITS(state->lenbits)];
				if ((unsigned)(this.bits) <= bits) break;
				PULLBYTE();
			}
			if (this.val < 16)
			{
				NEEDBITS(this.bits);
				DROPBITS(this.bits);
				state->lens[state->have++] = this.val;
			}
			else
			{
				if (this.val == 16)
				{
					NEEDBITS(this.bits + 2);
					DROPBITS(this.bits);
					if (state->have == 0)
					{
						strm->msg = (char *)"invalid bit length repeat";
						state->mode = BAD;
						break;
					}
					len = state->lens[state->have - 1];
					copy = 3 + BITS(2);
					DROPBITS(2);
				}
				else if (this.val == 17)
				{
					NEEDBITS(this.bits + 3);
					DROPBITS(this.bits);
					len = 0;
					copy = 3 + BITS(3);
					DROPBITS(3);
				}
				else
				{
					NEEDBITS(this.bits + 7);
					DROPBITS(this.bits);
					len = 0;
					copy = 11 + BITS(7);
					DROPBITS(7);
				}
				if (state->have + copy > state->nlen + state->ndist)
				{
					strm->msg = (char *)"invalid bit length repeat";
					state->mode = BAD;
					break;
				}
				while (copy--) state->lens[state->have++] = (unsigned short)len;
			}
		}

		/* handle error breaks in while */
		if (state->mode == BAD) break;

		/* build code tables */
		state->next = state->codes;
		state->lencode = (code const *)(state->next);
		state->lenbits = 9;
		ret = inflate_table(LENS, state->lens, state->nlen, &(state->next), &(state->lenbits), state->work);
		if (ret)
		{
			strm->msg = (char *)"invalid literal/lengths set";
			state->mode = BAD;
			break;
		}
		state->distcode = (code const *)(state->next);
		state->distbits = 6;
		ret = inflate_table(DISTS, state->lens + state->nlen, state->ndist, &(state->next), &(state->distbits), state->work);
		if (ret)
		{
			strm->msg = (char *)"invalid distances set";
			state->mode = BAD;
			break;
		}
		state->mode = LEN;
	case LEN:
		if (have >= 6 && left >= 258)
		{
			RESTORE();
			inflate_fast(strm, out);
			LOAD();
			break;
		}
		for (;;)
		{
			this = state->lencode[BITS(state->lenbits)];
			if ((unsigned)(this.bits) <= bits) break;
			PULLBYTE();
		}
		if (this.op && (this.op & 0xf0) == 0)
		{
			last = this;
			for (;;)
			{
				this = state->lencode[last.val + (BITS(last.bits + last.op) >> last.bits)];
				if ((unsigned)(last.bits + this.bits) <= bits) break;
				PULLBYTE();
			}
			DROPBITS(last.bits);
		}
		DROPBITS(this.bits);
		state->length = (unsigned)this.val;
		if ((int)(this.op) == 0)
		{
			state->mode = LIT;
			break;
		}
		if (this.op & 32)
		{
			state->mode = TYPE;
			break;
		}
		if (this.op & 64)
		{
			strm->msg = (char *)"invalid literal/length code";
			state->mode = BAD;
			break;
		}
		state->extra = (unsigned)(this.op) & 15;
		state->mode = LENEXT;
	case LENEXT:
		if (state->extra)
		{
			NEEDBITS(state->extra);
			state->length += BITS(state->extra);
			DROPBITS(state->extra);
		}
		state->mode = DIST;
	case DIST:
		for (;;)
		{
			this = state->distcode[BITS(state->distbits)];
                		if ((unsigned)(this.bits) <= bits) break;
			PULLBYTE();
		}
		if ((this.op & 0xf0) == 0)
		{
			last = this;
			for (;;)
			{
				this = state->distcode[last.val + (BITS(last.bits + last.op) >> last.bits)];
				if ((unsigned)(last.bits + this.bits) <= bits) break;
				PULLBYTE();
			}
			DROPBITS(last.bits);
		}
		DROPBITS(this.bits);
		if (this.op & 64)
		{
			strm->msg = (char *)"invalid distance code";
			state->mode = BAD;
			break;
		}
		state->offset = (unsigned)this.val;
		state->extra = (unsigned)(this.op) & 15;
		state->mode = DISTEXT;
	case DISTEXT:
		if (state->extra)
		{
			NEEDBITS(state->extra);
			state->offset += BITS(state->extra);
			DROPBITS(state->extra);
		}
		if (state->offset > state->whave + out - left)
		{
			strm->msg = (char *)"invalid distance too far back";
			state->mode = BAD;
			break;
		}
		state->mode = MATCH;
	case MATCH:
		if (left == 0) goto inf_leave;
		copy = out - left;
		if (state->offset > copy)         /* copy from window */
		{
			copy = state->offset - copy;
			if (copy > state->write)
			{
				copy -= state->write;
				from = state->window + (state->wsize - copy);
			}
			else from = state->window + (state->write - copy);
			if (copy > state->length) copy = state->length;
		}
		else                              /* copy from output */
		{
			from = put - state->offset;
			copy = state->length;
		}
		if (copy > left) copy = left;
		left -= copy;
		state->length -= copy;
		do{
		*put++ = *from++;
		}while (--copy);
		if (state->length == 0) state->mode = LEN;
		break;
	case LIT:
		if (left == 0) goto inf_leave;
		*put++ = (unsigned char)(state->length);
		left--;
		state->mode = LEN;
		break;
	case CHECK:
		if (state->wrap)
		{
			NEEDBITS(32);
			out -= left;
			strm->total_out += out;
			state->total += out;
			if (out) strm->adler = state->check =
			UPDATE(state->check, put - out, out);
			out = left;
			if ((REVERSE(hold)) != state->check)
			{
				strm->msg = (char *)"incorrect data check";
				state->mode = BAD;
				break;
			}
			INITBITS();
		}
		state->mode = DONE;
	case DONE:
		ret = Z_STREAM_END;
		goto inf_leave;
	case BAD:
		ret = Z_DATA_ERROR;
		goto inf_leave;
	case MEM:
		return Z_MEM_ERROR;
	case SYNC:
	default: return Z_STREAM_ERROR;
	}

	//Return from inflate(), updating the total counts and the check value.
	//If there was no progress during the inflate() call, return a buffer
	//error.  Call updatewindow() to create and/or update the window state.
	//Note: a memory error from inflate() is non-recoverable.

inf_leave:
	RESTORE();
	if (state->wsize || (state->mode < CHECK && out != strm->avail_out))
	if (updatewindow(strm, out))
	{
		state->mode = MEM;
		return Z_MEM_ERROR;
	}
	in -= strm->avail_in;
	out -= strm->avail_out;
	strm->total_in += in;
	strm->total_out += out;
	state->total += out;
	if (state->wrap && out) strm->adler = state->check = UPDATE(state->check, strm->next_out - out, out);
	strm->data_type = state->bits + (state->last ? 64 : 0) + (state->mode == TYPE ? 128 : 0);
	if (((in == 0 && out == 0) || flush == Z_FINISH) && ret == Z_OK) ret = Z_BUF_ERROR;
	return ret;
}

int inflateEnd(z_streamp strm)
{
	struct inflate_state *state;
	if (strm == NULL || strm->state == NULL || strm->zfree == (free_func)0)
		return Z_STREAM_ERROR;
	state = (struct inflate_state *)strm->state;
	if (state->window != NULL) (*((strm)->zfree))((strm)->opaque, (byte*)(state->window));
	(*((strm)->zfree))((strm)->opaque, (byte*)(strm->state));
	strm->state = NULL;
	return Z_OK;
}

int inflateReset(z_streamp strm)
{
	struct inflate_state *state;

	if (strm == NULL || strm->state == NULL) return Z_STREAM_ERROR;
	state = (struct inflate_state *)strm->state;
	strm->total_in = strm->total_out = state->total = 0;
	strm->msg = NULL;
	strm->adler = 1;        // to support ill-conceived Java test suite
	state->mode = HEAD;
	state->last = 0;
	state->havedict = 0;
	state->dmax = 32768U;
	state->head = NULL;
	state->wsize = 0;
	state->whave = 0;
	state->write = 0;
	state->hold = 0;
	state->bits = 0;
	state->lencode = state->distcode = state->next = state->codes;
	return Z_OK;
}