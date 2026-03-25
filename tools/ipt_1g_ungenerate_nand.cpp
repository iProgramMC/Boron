#include <cstdio>
#include <string>
#include <sstream>
#include <thread>
std::string nand_folder_path = "W:\\iphone-qemu\\ipod-data\\nand-old";
std::string nand_out_path = "W:\\iphone-qemu\\ipod-data\\nand.img";

#define BANKS 8
#define BLOCKS_PER_BANK 4096
#define PAGES_PER_BANK 524288
#define BYTES_PER_PAGE 2048
#define PAGES_PER_SUBLOCK 1024
#define PAGES_PER_BLOCK 128
#define BYTES_PER_SPARE 64
#define FIL_ID 0x43303032
#define FTL_CTX_VBLK_IND 0 // virtual block index of the FTL Context

#define NUM_PARTITIONS 1
#define BOOT_PARTITION_FIRST_PAGE (NUM_PARTITIONS + 2)  // first two LBAs are for MBR and GUID header

/*
my best guess is that this'll probably create two files:
- nand-data.img (contains all the data)
- nand-spare.img (contains all the spare ECC data or whatever tf)

from reading the code, it seems like the offsets are structured like this:

[page number] [bank number] [page offset]
[   virtual page number   ] [page offset]

*/

template<typename T>
std::string toString(T t) {
	std::stringstream ss;
	ss << t;
	return ss.str();
}

void expand_file(FILE* f, size_t sz) {
	char data = 0;
	_fseeki64(f, sz - 1, SEEK_SET);
	fwrite(&data, 1, 1, f);
}

int main()
{
	FILE* nand_out;

	nand_out = fopen(nand_out_path.c_str(), "wb");
	if (!nand_out) {
		perror(("Cannot open " + nand_out_path).c_str());
		exit(1);
	}

	const uint64_t spareStart = (uint64_t) PAGES_PER_BANK * BANKS * BYTES_PER_PAGE;
	const uint64_t total_vpns = (uint64_t) PAGES_PER_BANK * BANKS;

	expand_file(nand_out, total_vpns * (BYTES_PER_PAGE + BYTES_PER_SPARE));

	for (int page = 0; page < PAGES_PER_BANK; page++)
	{
		for (int bank = 0; bank < BANKS; bank++)
		{
			uint8_t page_buffer[BYTES_PER_PAGE];
			uint8_t spare_buffer[BYTES_PER_SPARE];

			uint64_t vpn = (uint64_t) page * 8 + bank;
			fprintf(stderr, "\rprocessed %llu/%llu banks.", vpn, total_vpns);

			std::string path = nand_folder_path + "/bank" + toString(bank) + "/" + toString(page) + ".page";
			struct stat st = { 0 };
			if (stat(path.c_str(), &st) == -1)
			{
				// page storage does not exist - initialize an empty buffer
				memset(page_buffer, 0, BYTES_PER_PAGE);
				memset(spare_buffer, 0, BYTES_PER_SPARE);
				spare_buffer[0xA] = 0xFF; // make sure we add the FTL mark to an empty page
			}
			else
			{
				FILE* f = fopen(path.c_str(), "rb");
				if (!f) {
					perror(("Cannot open path " + path).c_str());
					exit(1);
				}

				fread(page_buffer, 1, BYTES_PER_PAGE, f);
				fread(spare_buffer, 1, BYTES_PER_SPARE, f);
				fclose(f);
			}

			_fseeki64(nand_out, vpn * BYTES_PER_PAGE, SEEK_SET);
			fwrite(page_buffer, 1, BYTES_PER_PAGE, nand_out);

			_fseeki64(nand_out, spareStart + vpn * BYTES_PER_SPARE, SEEK_SET);
			fwrite(spare_buffer, 1, BYTES_PER_SPARE, nand_out);
		}
	}

	fprintf(stderr, "\ndone.\n");
	fclose(nand_out);
	return 0;
}
