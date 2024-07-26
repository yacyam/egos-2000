/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: load an ELF-format executable file into memory
 * Only use the program header instead of the multiple section headers.
 */

#include "egos.h"
#include "elf.h"
#include "disk.h"
#include "servers.h"

#include <string.h>

static void load_kernel(elf_reader reader,
                       struct elf32_program_header* pheader,
                       char *entry) {
    INFO("KERNEL: 0x%.8x bytes", pheader->p_filesz);
    INFO("KERNEL: 0x%.8x bytes", pheader->p_memsz);

    uint block_offset = pheader->p_offset / BLOCK_SIZE;
    for (uint off = 0; off < pheader->p_filesz; off += BLOCK_SIZE)
        reader(block_offset++, entry + off);
    
    memset(entry + pheader->p_filesz, 0, pheader->p_memsz - pheader->p_filesz);
}

void elf_load(int pid, elf_reader reader, int argc, void** argv) {
    char buf[BLOCK_SIZE];
    reader(0, buf);

    struct elf32_header *header = (void*) buf;
    struct elf32_program_header *pheader = (void*)(buf + header->e_phoff);

    for (uint i = 0; i < header->e_phnum; i++) {
        if (pheader[i].p_memsz == 0) continue;
        else if (pheader[i].p_vaddr == GRASS_ENTRY)
            load_kernel(reader, &pheader[i], (char *)GRASS_ENTRY);
        else if (pheader[i].p_vaddr == LOADER_PENTRY)
            load_kernel(reader, &pheader[i], (char *)LOADER_PENTRY);
        else FATAL("elf_load: Invalid p_vaddr: 0x%.8x", pheader->p_vaddr);
    }
}
