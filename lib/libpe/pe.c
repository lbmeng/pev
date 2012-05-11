/*
	libpe - the PE library

	Copyright (C) 2010 - 2012 Fernando Mercês

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "pe.h"

void *xmalloc(unsigned int size)
{
	void *new_mem = malloc(size);	

	if (new_mem == NULL)
	{
		fprintf(stderr, "fatal: memory exhausted (xmalloc of %u bytes)\n", size);
		exit(-1);
	}

	return new_mem;
}

bool pe_init(PE_FILE *pe, FILE *handle)
{
	if (!pe || !handle)
		return false;

	memset(pe, 0, sizeof(PE_FILE));
	pe->handle = handle;

	return true;
}

IMAGE_SECTION_HEADER* pe_get_section(PE_FILE *pe, const char *section_name)
{
	if (!pe || !section_name)
		return NULL;

	if (!pe->addr_sections || !pe->num_sections)
		pe_get_sections(pe);

	if (!pe->num_sections || pe->num_sections > 96)
		return NULL;
		
	for (unsigned int i=0; i < pe->num_sections; i++)
	{
		if (memcmp(pe->sections_ptr[i]->Name, section_name, strlen(section_name)) == 0)
			return pe->sections_ptr[i];
	}
	return NULL;
}

bool pe_get_resource_entries(PE_FILE *pe)
{
	IMAGE_RESOURCE_DIRECTORY dir;

	if (!pe)
		return false;
	
	if (pe->rsrc_entries_ptr)
		return pe->rsrc_entries_ptr;
	
	if (!pe_get_resource_directory(pe, &dir))
		return false;

	pe->num_rsrc_entries = dir.NumberOfIdEntries + dir.NumberOfNamedEntries;

	if (!pe->num_rsrc_entries)
		return false;

	pe->rsrc_entries_ptr = (IMAGE_RESOURCE_DIRECTORY_ENTRY **)
	xmalloc(sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY) * pe->num_rsrc_entries);
	
	for (unsigned int i=0; i < pe->num_rsrc_entries; i++)
	{
		pe->rsrc_entries_ptr[i] = (IMAGE_RESOURCE_DIRECTORY_ENTRY *) xmalloc
		(sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY));
		
		if (! fread(pe->rsrc_entries_ptr[i], sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY), 1, pe->handle))
			return false;
	}
	return true;
}

bool pe_get_resource_directory(PE_FILE *pe, IMAGE_RESOURCE_DIRECTORY *dir)
{
	if (!pe)
		return false;
	
	if (!pe->addr_rsrc_sec)
		pe->addr_rsrc_sec = (pe_get_section(pe, ".rsrc"))->PointerToRawData;

	for (unsigned int i=0; i < pe->num_sections; i++)
	{
		if (memcmp(pe->sections_ptr[i]->Name, ".rsrc", 5) == 0)
		{
			pe->addr_rsrc_dir = pe->sections_ptr[i]->PointerToRawData;
			fseek(pe->handle, pe->addr_rsrc_dir, SEEK_SET);
			fread(dir, sizeof(dir), 1, pe->handle);
			return true;
		}
	}
	return false;
}

/*
bool pe_get_tls_callbacks(PE_FILE *pe)
{
	IMAGE_TLS_DIRECTORY32 *tlsdir;
	int i;
	unsigned tls_addr = 0;

	pe_get_directories(pe);
	for (i=0; i < pe->num_directories; i++)
	{
		if (pe->directories_ptr[i].Size > 0)
		{
		    if (i==9) // 9 is TLS directory
		    	tls_addr = pe->directories_ptr[i].VirtualAddress;
		}
	}
	//printf("tls_addr: %#x\n", tls_addr);

	pe_get_sections(pe);
	for (i=0; i < pe->num_sections; i++)
	{
		if (tls_addr > pe->sections_ptr[i].VirtualAddress &&
		    tls_addr < (pe->sections_ptr[i].VirtualAddress + pe->sections_ptr[i].SizeOfRawData))
		{
			tlsdir = (IMAGE_TLS_DIRECTORY32 *) xmalloc(sizeof(tlsdir));

			fseek(pe->handle, tls_addr - pe->sections_ptr[i].VirtualAddress
			+ pe->sections_ptr[i].PointerToRawData, SEEK_SET);

			fread(tlsdir, sizeof(tlsdir), 1, pe->handle);

			pe->tls_ptr = tlsdir;

			return true;
		}
	}

	return false;
}
*/

bool pe_get_sections(PE_FILE *pe)
{
	IMAGE_SECTION_HEADER **sections;

	if (!pe)
		return false;

	if (pe->sections_ptr)
		return true;

	if (!pe->addr_sections || !pe->num_sections)
	{
		if (!pe_get_directories(pe))
			return false;
	}

	if (pe->num_sections > 96)
		return false;

	fseek(pe->handle, pe->addr_sections, SEEK_SET);
	sections = (IMAGE_SECTION_HEADER **) xmalloc(sizeof(IMAGE_SECTION_HEADER *) * pe->num_sections);

	for (unsigned int i=0; i < pe->num_sections; i++)
	{
		sections[i] = (IMAGE_SECTION_HEADER *) xmalloc(sizeof(IMAGE_SECTION_HEADER));
		fread(sections[i], sizeof(IMAGE_SECTION_HEADER), 1, pe->handle);
	}

	pe->sections_ptr = sections;

	if (!pe->sections_ptr)
		return false;

	return true;
}

bool pe_get_directories(PE_FILE *pe)
{
	IMAGE_DATA_DIRECTORY **dirs;

	if (!pe)
		return false;

	if (pe->directories_ptr)
		return true;

	if (!pe->addr_directories)
	{
		if (!pe_get_optional(pe))
			return false;
	}

	if (fseek(pe->handle, pe->addr_directories, SEEK_SET))
		return false;

	if (pe->num_directories > 32)
		return false;

	dirs = (IMAGE_DATA_DIRECTORY **) xmalloc(sizeof(IMAGE_DATA_DIRECTORY *) * pe->num_directories);

	for (unsigned int i=0; i < pe->num_directories; i++)
	{
		dirs[i] = (IMAGE_DATA_DIRECTORY *) xmalloc(sizeof(IMAGE_DATA_DIRECTORY));
		fread(dirs[i], sizeof(IMAGE_DATA_DIRECTORY), 1, pe->handle);
	}

	pe->addr_sections = ftell(pe->handle);
	pe->directories_ptr = dirs;

	if (!pe->addr_sections || !pe->directories_ptr)
		return false;

	return true;
}

bool pe_get_optional(PE_FILE *pe)
{
	IMAGE_OPTIONAL_HEADER *header;

	if (!pe)
		return false;

	if (pe->optional_ptr)
		return true;

	if (!pe->addr_optional)
	{
		IMAGE_COFF_HEADER coff;

		if (!pe_get_coff(pe, &coff))
			return false;
	}

	if (fseek(pe->handle, pe->addr_optional, SEEK_SET))
		return false;

	header = (IMAGE_OPTIONAL_HEADER *) xmalloc(sizeof(IMAGE_OPTIONAL_HEADER));

	switch (pe->architecture)
	{
		case PE32:
			header->_32 = (IMAGE_OPTIONAL_HEADER_32 *) xmalloc(sizeof (IMAGE_OPTIONAL_HEADER_32));
			if (!fread(header->_32, sizeof(IMAGE_OPTIONAL_HEADER_32), 1, pe->handle))
				return false;
			pe->num_directories = header->_32->NumberOfRvaAndSizes;
			pe->entrypoint = header->_32->AddressOfEntryPoint;
			//pe->imagebase = header->_32->ImageBase;
			header->_64 = NULL;
			break;

		case PE64:
			header->_64 = (IMAGE_OPTIONAL_HEADER_64 *) xmalloc(sizeof (IMAGE_OPTIONAL_HEADER_64));
			if (!fread(header->_64, sizeof(IMAGE_OPTIONAL_HEADER_64), 1, pe->handle))
				return false;
			pe->num_directories = header->_64->NumberOfRvaAndSizes;
			pe->entrypoint = header->_64->AddressOfEntryPoint;
			//pe->imagebase = header->_64->ImageBase;
			header->_32 = NULL;
			break;

		default:
			return false;
	}

	pe->optional_ptr = header;
	pe->addr_directories = ftell(pe->handle);

	if (!pe->optional_ptr || !pe->addr_directories)
		return false;

	return true;
}

bool pe_get_coff(PE_FILE *pe, IMAGE_COFF_HEADER *header)
{
	if (!pe)
		return false;

	if (!pe->addr_coff)
	{
		IMAGE_DOS_HEADER dos;

		if (!pe_get_dos(pe, &dos))
			return false;
	}

	if (!pe->handle)
		return false;

	if (fseek(pe->handle, pe->addr_coff, SEEK_SET))
		return false;

	if (!fread(header, sizeof(IMAGE_COFF_HEADER), 1, pe->handle))
		return false;

	pe->num_sections = header->NumberOfSections;
	pe->addr_optional = ftell(pe->handle);

	if (!fread(&pe->architecture, sizeof(WORD), 1, pe->handle))
		return false;

	if(!pe->num_sections || !pe->addr_optional)
		return false;

	return true;
}

bool pe_get_dos(PE_FILE *pe, IMAGE_DOS_HEADER *header)
{
	if (!pe)
		return false;
	
	if (!pe->handle)
		return false;

	rewind(pe->handle);

	if (!fread(header, sizeof(IMAGE_DOS_HEADER), 1, pe->handle))
		return false;

	pe->addr_coff = header->e_lfanew + 4; // NT signature (PE\0\0)

	return true;
}

bool ispe(PE_FILE *pe)
{
	WORD header;

	if (pe->handle == NULL)
		return false;

	rewind(pe->handle);
	fread(&header, sizeof(WORD), 1, pe->handle);

	if (header == MZ)
		return true;

	return false;
}

void pe_deinit(PE_FILE *pe)
{
	unsigned int i;

	if (pe->handle)
		fclose(pe->handle);

	if (pe->optional_ptr)
	{
		if (pe->optional_ptr->_32)
			free(pe->optional_ptr->_32);

		if (pe->optional_ptr->_64)
			free(pe->optional_ptr->_64);

		free(pe->optional_ptr);
	}

	if (pe->directories_ptr)
	{
		for (i=0; i < pe->num_directories; i++)
		{
			if (pe->directories_ptr[i])
				free(pe->directories_ptr[i]);
		}
		free(pe->directories_ptr);
	}

	if (pe->sections_ptr)
	{
		for (i=0; i < pe->num_sections; i++)
		{
			if (pe->sections_ptr[i])
				free(pe->sections_ptr[i]);
		}
		free(pe->sections_ptr);
	}

	/*
	if (pe->tls_ptr)
		free(pe->tls_ptr);
	*/	

	if (pe->rsrc_entries_ptr)
	{
		for (i=0; i < pe->num_rsrc_entries; i++)
		{
			if (pe->rsrc_entries_ptr[i])
				free(pe->rsrc_entries_ptr[i]);
		}

		free(pe->rsrc_entries_ptr);
	}

	if (pe->rsrc_ptr)
			free(pe->rsrc_ptr);
}