#include "nemu.h"

#define ENTRY_START 0x100000

extern uint8_t entry [];
extern uint32_t entry_len;
extern char *exec_file;

void load_elf_tables(int, char *[]);
void init_regex();
void init_wp_pool();
void init_ddr3();

void init_CL1();
void init_CL2();
void init_TLB();

void init_device();
void init_sdl();

FILE *log_fp = NULL;

static void init_log() {
	log_fp = fopen("log.txt", "w");
	Assert(log_fp, "Can not open 'log.txt'");
}

static void welcome() {
	printf("Welcome to NEMU!\nThe executable is %s.\nFor help, type \"help\"\n",
			exec_file);
}

void init_monitor(int argc, char *argv[]) {
	/* Perform some global initialization */

	/* Open the log file. */
	init_log();

	/* Load the string table and symbol table from the ELF file for future use. */
	load_elf_tables(argc, argv);

	/* Compile the regular expressions. */
	init_regex();

	/* Initialize the watchpoint pool. */
	init_wp_pool();

	/* Initialize the device. */
	init_device();

	/* Initialize the sdl. */
	init_sdl();

	/* Display welcome message. */
	welcome();
}

#ifdef USE_RAMDISK
static void init_ramdisk() {
	int ret;
	const int ramdisk_max_size = 0xa0000;
	FILE *fp = fopen(exec_file, "rb");
	Assert(fp, "Can not open '%s'", exec_file);

	fseek(fp, 0, SEEK_END);
	size_t file_size = ftell(fp);
	Assert(file_size < ramdisk_max_size, "file size(%zd) too large", file_size);

	fseek(fp, 0, SEEK_SET);
	ret = fread(hwa_to_va(0), file_size, 1, fp);
	assert(ret == 1);
	fclose(fp);
}
#endif

static void load_entry() {
	int ret;
	FILE *fp = fopen("entry", "rb");
	Assert(fp, "Can not open 'entry'");

	fseek(fp, 0, SEEK_END);
	size_t file_size = ftell(fp);

	fseek(fp, 0, SEEK_SET);
	ret = fread(hwa_to_va(ENTRY_START), file_size, 1, fp);
	assert(ret == 1);
	fclose(fp);
}

void init_eflags(){
	cpu.eflags.EFLAGS=2;
}

void init_cr0(){
	cpu.cr0.val=0;
}

void init_cr3(){
	cpu.cr3.val=0;
}

void init_sreg(){
	int i;
	for(i=0;i<4;i++){
		cpu.sreg[i].selector=0;
	}
	cpu.sregdesc[SR_CS].base=0;
	cpu.sregdesc[SR_CS].limit=0xffffffff;
}

void restart() {
	/* Perform some initialization to restart a program */
#ifdef USE_RAMDISK
	/* Read the file with name `argv[1]' into ramdisk. */
	init_ramdisk();
#endif

	/* Read the entry code into memory. */
	load_entry();

	/* Set the initial instruction pointer. */
	cpu.eip = ENTRY_START;

	/* Initialize DRAM. */
	init_ddr3();

#ifdef USE_CACHE
	/* Initialize CACHE L1. */
	init_CL1();

	/* Initialize CACHE L2. */
	init_CL2();
#endif

#ifdef USE_TLB
	printf("execute init TLB\n");
	/* Initialize TLB. */
	init_TLB();
#endif

	/* Initialize EFLAGS. */
	init_eflags();

	/* Initialize CR0. */
	init_cr0();

	/* Initialize CR3. */
	init_cr3();

	/* Initialize SREG. */
	init_sreg();
}
