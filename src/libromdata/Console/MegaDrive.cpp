/***************************************************************************
 * ROM Properties Page shell extension. (libromdata)                       *
 * MegaDrive.cpp: Sega Mega Drive ROM reader.                              *
 *                                                                         *
 * Copyright (c) 2016-2020 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "stdafx.h"
#include "MegaDrive.hpp"

#include "md_structs.h"
#include "data/SegaPublishers.hpp"
#include "MegaDriveRegions.hpp"
#include "CopierFormats.h"
#include "utils/SuperMagicDrive.hpp"

// librpbase, librpfile
#include "librpbase/Achievements.hpp"
using namespace LibRpBase;
using LibRpFile::IRpFile;

// Other RomData subclasses
#include "Other/ISO.hpp"

// C++ STL classes.
using std::string;
using std::vector;

namespace LibRomData {

ROMDATA_IMPL(MegaDrive)

class MegaDrivePrivate final : public RomDataPrivate
{
	public:
		MegaDrivePrivate(MegaDrive *q, IRpFile *file);
		~MegaDrivePrivate();

	private:
		typedef RomDataPrivate super;
		RP_DISABLE_COPY(MegaDrivePrivate)

	public:
		/** RomFields **/

		// I/O support. (RFT_BITFIELD)
		enum MD_IO_Support_Bitfield {
			MD_IOBF_JOYPAD_3	= (1U << 0),	// 3-button joypad
			MD_IOBF_JOYPAD_6	= (1U << 1),	// 6-button joypad
			MD_IOBF_JOYPAD_SMS	= (1U << 2),	// 2-button joypad (SMS)
			MD_IOBF_TEAM_PLAYER	= (1U << 3),	// Team Player
			MD_IOBF_KEYBOARD	= (1U << 4),	// Keyboard
			MD_IOBF_SERIAL		= (1U << 5),	// Serial (RS-232C)
			MD_IOBF_PRINTER		= (1U << 6),	// Printer
			MD_IOBF_TABLET		= (1U << 7),	// Tablet
			MD_IOBF_TRACKBALL	= (1U << 8),	// Trackball
			MD_IOBF_PADDLE		= (1U << 9),	// Paddle
			MD_IOBF_FDD		= (1U << 10),	// Floppy Drive
			MD_IOBF_CDROM		= (1U << 11),	// CD-ROM
			MD_IOBF_ACTIVATOR	= (1U << 12),	// Activator
			MD_IOBF_MEGA_MOUSE	= (1U << 13),	// Mega Mouse
		};

		/** Internal ROM data. **/

		/**
		 * Parse the I/O support field.
		 * @param io_support I/O support field.
		 * @param size Size of io_support.
		 * @return io_support bitfield.
		 */
		static uint32_t parseIOSupport(const char *io_support, int size);

	public:
		enum MD_RomType {
			ROM_UNKNOWN = -1,		// Unknown ROM type.

			// Low byte: System ID
			// (TODO: MCD Boot ROMs, other specialized types?)
			ROM_SYSTEM_MD		= 0,	// Mega Drive
			ROM_SYSTEM_MCD		= 1,	// Mega CD
			ROM_SYSTEM_32X		= 2,	// Sega 32X
			ROM_SYSTEM_MCD32X	= 3,	// Sega CD 32X
			ROM_SYSTEM_PICO		= 4,	// Sega Pico
			ROM_SYSTEM_TERADRIVE	= 5,	// Sega Teradrive
			ROM_SYSTEM_MAX		= ROM_SYSTEM_TERADRIVE,
			ROM_SYSTEM_UNKNOWN	= 0xFF,
			ROM_SYSTEM_MASK		= 0xFF,

			// Mid byte: Image format
			ROM_FORMAT_CART_BIN	= (0 << 8),	// Cartridge: Binary format.
			ROM_FORMAT_CART_SMD	= (1 << 8),	// Cartridge: SMD format.
			ROM_FORMAT_DISC_2048	= (2 << 8),	// Disc: 2048-byte sectors. (ISO)
			ROM_FORMAT_DISC_2352	= (3 << 8),	// Disc: 2352-byte sectors. (BIN)
			ROM_FORMAT_MAX		= ROM_FORMAT_DISC_2352,
			ROM_FORMAT_UNKNOWN	= (0xFF << 8),
			ROM_FORMAT_MASK		= (0xFF << 8),

			// High byte: Special MD extensions
			ROM_EXT_SSF2		= (1 << 16),	// SSF2 mapper
			ROM_EXT_EVERDRIVE	= (2 << 16),	// Everdrive extensions
			ROM_EXT_MEGAWIFI	= (3 << 16),	// Mega Wifi extensions
			ROM_EXT_TERADRIVE_68k	= (4 << 16),	// Sega Teradrive: Boot from 68000
			ROM_EXT_TERADRIVE_x86	= (5 << 16),	// Sega Teradrive: Boot from x86
			ROM_EXT_MASK		= (0xFF << 16),
		};

		// MIME type table.
		// Ordering matches MD_RomType's system IDs.
		static const char *const mimeType_tbl[];

		int romType;		// ROM type.
		unsigned int md_region;	// MD hexadecimal region code.

		/**
		 * Is this a disc?
		 * Discs don't have a vector table.
		 * @return True if this is a disc; false if not.
		 */
		inline bool isDisc(void) const
		{
			int rfmt = romType & ROM_FORMAT_MASK;
			return (rfmt == ROM_FORMAT_DISC_2048 ||
				rfmt == ROM_FORMAT_DISC_2352);
		}

	public:
		/**
		 * Add fields for the ROM header.
		 *
		 * This function will not create a new tab.
		 * If one is desired, it should be created
		 * before calling this function.
		 *
		 * @param pRomHeader ROM header.
		 * @param bRedetectRegion If true, re-detect the MD region. (i.e. don't use the one parsed in the constructor)
		 */
		void addFields_romHeader(const MD_RomHeader *pRomHeader, bool bRedetectRegion = false);

		/**
		 * Add fields for the vector table.
		 *
		 * This function will not create a new tab.
		 * If one is desired, it should be created
		 * before calling this function.
		 *
		 * @param pVectors Vector table.
		 */
		void addFields_vectorTable(const M68K_VectorTable *pVectors);

	public:
		// ROM header.
		// NOTE: Must be byteswapped on access.
		M68K_VectorTable vectors;	// Interrupt vectors.
		MD_RomHeader romHeader;		// ROM header.

		// Extra headers.
		SMD_Header *pSmdHeader;		// SMD header.
		MD_RomHeader *pRomHeaderLockOn;	// Locked-on ROM header.
};

/** MegaDrivePrivate **/

// MIME type table.
// Ordering matches MD_RomType's system IDs.
const char *const MegaDrivePrivate::mimeType_tbl[] = {
	// Unofficial MIME types from FreeDesktop.org.
	"application/x-genesis-rom",
	"application/x-sega-cd-rom",
	"application/x-genesis-32x-rom",
	"application/x-sega-cd-32x-rom",	// NOTE: Not on fd.o!
	"application/x-sega-pico-rom",

	nullptr
};

MegaDrivePrivate::MegaDrivePrivate(MegaDrive *q, IRpFile *file)
	: super(q, file)
	, romType(ROM_UNKNOWN)
	, md_region(0)
	, pSmdHeader(nullptr)
	, pRomHeaderLockOn(nullptr)
{
	// Clear the various structs.
	memset(&vectors, 0, sizeof(vectors));
	memset(&romHeader, 0, sizeof(romHeader));
}

MegaDrivePrivate::~MegaDrivePrivate()
{
	delete pSmdHeader;
	delete pRomHeaderLockOn;
}

/** Internal ROM data. **/

/**
 * Parse the I/O support field.
 * @param io_support I/O support field.
 * @param size Size of io_support.
 * @return io_support bitfield.
 */
uint32_t MegaDrivePrivate::parseIOSupport(const char *io_support, int size)
{
	uint32_t ret = 0;
	for (int i = size-1; i >= 0; i--) {
		// TODO: Sort by character and use bsearch()?
		#define MD_IO_SUPPORT_ENTRY(entry) {MD_IO_##entry, MD_IOBF_##entry}
		static const char md_io_chr_tbl[] = {
			// This MUST be in the same order as MD_IO_Support_Bitfield!
			MD_IO_JOYPAD_3,
			MD_IO_JOYPAD_6,
			MD_IO_JOYPAD_SMS,
			MD_IO_TEAM_PLAYER,
			MD_IO_KEYBOARD,
			MD_IO_SERIAL,
			MD_IO_PRINTER,
			MD_IO_TABLET,
			MD_IO_TRACKBALL,
			MD_IO_PADDLE,
			MD_IO_FDD,
			MD_IO_CDROM,
			MD_IO_ACTIVATOR,
			MD_IO_MEGA_MOUSE,

			'\0'
		};

		const char io_cur = io_support[i];
		uint32_t io_bit = 1;
		for (const auto *p = md_io_chr_tbl; *p != '\0'; p++, io_bit <<= 1) {
			if (*p == io_cur) {
				ret |= io_bit;
				break;
			}
		}
	}

	return ret;
}

/**
 * Add fields for the ROM header.
 *
 * This function will not create a new tab.
 * If one is desired, it should be created
 * before calling this function.
 *
 * @param pRomHeader ROM header.
 * @param bRedetectRegion If true, re-detect the MD region. (i.e. don't use the one parsed in the constructor)
 */
void MegaDrivePrivate::addFields_romHeader(const MD_RomHeader *pRomHeader, bool bRedetectRegion)
{
	// Read the strings from the header.
	fields->addField_string(C_("MegaDrive", "System"),
		cp1252_sjis_to_utf8(pRomHeader->system, sizeof(pRomHeader->system)),
			RomFields::STRF_TRIM_END);
	fields->addField_string(C_("RomData", "Copyright"),
		cp1252_sjis_to_utf8(pRomHeader->copyright, sizeof(pRomHeader->copyright)),
			RomFields::STRF_TRIM_END);

	// Determine the publisher.
	// Formats in the copyright line:
	// - "(C)SEGA"
	// - "(C)T-xx"
	// - "(C)T-xxx"
	// - "(C)Txxx"
	const char *publisher = nullptr;
	unsigned int t_code = 0;
	if (!memcmp(pRomHeader->copyright, "(C)SEGA", 7)) {
		// Sega first-party game.
		publisher = "Sega";
	} else if (!memcmp(pRomHeader->copyright, "(C)T", 4)) {
		// Third-party game.
		int start = 4;
		if (pRomHeader->copyright[4] == '-')
			start++;
		char *endptr;
		t_code = strtoul(&pRomHeader->copyright[start], &endptr, 10);
		if (t_code != 0 &&
		    endptr > &pRomHeader->copyright[start] &&
		    endptr < &pRomHeader->copyright[start+3])
		{
			// Valid T-code. Look up the publisher.
			publisher = SegaPublishers::lookup(t_code);
		}
	}

	const char *const publisher_title = C_("RomData", "Publisher");
	if (publisher) {
		// Publisher identified.
		fields->addField_string(publisher_title, publisher);
	} else if (t_code > 0) {
		// Unknown publisher, but there is a valid T code.
		fields->addField_string(publisher_title, rp_sprintf("T-%u", t_code));
	} else {
		// Unknown publisher.
		fields->addField_string(C_("RomData", "Publisher"),
			C_("RomData", "Unknown"));
	}

	// Titles, serial number, and checksum.
	fields->addField_string(C_("MegaDrive", "Domestic Title"),
		cp1252_sjis_to_utf8(pRomHeader->title_domestic, sizeof(pRomHeader->title_domestic)),
			RomFields::STRF_TRIM_END);
	fields->addField_string(C_("MegaDrive", "Export Title"),
		cp1252_sjis_to_utf8(pRomHeader->title_export, sizeof(pRomHeader->title_export)),
			RomFields::STRF_TRIM_END);
	fields->addField_string(C_("MegaDrive", "Serial Number"),
		cp1252_sjis_to_utf8(pRomHeader->serial, sizeof(pRomHeader->serial)),
			RomFields::STRF_TRIM_END);
	if (!isDisc()) {
		// Checksum. (MD only; not valid for Mega CD.)
		fields->addField_string_numeric(C_("RomData", "Checksum"),
			be16_to_cpu(pRomHeader->checksum), RomFields::Base::Hex, 4,
			RomFields::STRF_MONOSPACE);
	}

	// I/O support bitfield.
	static const char *const io_bitfield_names[] = {
		NOP_C_("MegaDrive|I/O", "Joypad"),
		NOP_C_("MegaDrive|I/O", "6-button"),
		// tr: Use a locale-specific abbreviation for Sega Master System, e.g. MK3 or similar.
		NOP_C_("MegaDrive|I/O", "SMS Joypad"),
		NOP_C_("MegaDrive|I/O", "Team Player"),
		NOP_C_("MegaDrive|I/O", "Keyboard"),
		NOP_C_("MegaDrive|I/O", "Serial I/O"),
		NOP_C_("MegaDrive|I/O", "Printer"),
		NOP_C_("MegaDrive|I/O", "Tablet"),
		NOP_C_("MegaDrive|I/O", "Trackball"),
		NOP_C_("MegaDrive|I/O", "Paddle"),
		NOP_C_("MegaDrive|I/O", "Floppy Drive"),
		NOP_C_("MegaDrive|I/O", "CD-ROM"),
		// tr: Brand name; only translate if the name was changed in your region.
		NOP_C_("MegaDrive|I/O", "Activator"),
		NOP_C_("MegaDrive|I/O", "Mega Mouse"),
	};
	vector<string> *const v_io_bitfield_names = RomFields::strArrayToVector_i18n(
		"MegaDrive|I/O", io_bitfield_names, ARRAY_SIZE(io_bitfield_names));
	// Parse I/O support.
	uint32_t io_support = parseIOSupport(pRomHeader->io_support, sizeof(pRomHeader->io_support));
	fields->addField_bitfield(C_("MegaDrive", "I/O Support"),
		v_io_bitfield_names, 3, io_support);

	if (!isDisc()) {
		// ROM range.
		fields->addField_string_address_range(C_("MegaDrive", "ROM Range"),
				be32_to_cpu(pRomHeader->rom_start),
				be32_to_cpu(pRomHeader->rom_end), 8,
				RomFields::STRF_MONOSPACE);

		// RAM range.
		fields->addField_string_address_range(C_("MegaDrive", "RAM Range"),
				be32_to_cpu(pRomHeader->ram_start),
				be32_to_cpu(pRomHeader->ram_end), 8,
				RomFields::STRF_MONOSPACE);

		// Check for external memory.
		const uint32_t sram_info = be32_to_cpu(pRomHeader->sram_info);
		const char *const sram_range_title = C_("MegaDrive", "SRAM Range");
		if ((sram_info & 0xFFFFA7FF) == 0x5241A020) {
			// SRAM is present.
			// Format: 'R', 'A', %1x1yz000, 0x20
			// x == 1 for backup (SRAM), 0 for not backup
			// yz == 10 for even addresses, 11 for odd addresses
			// TODO: Print the 'x' bit.
			const char *suffix;
			switch ((sram_info >> (8+3)) & 0x03) {
				case 2:
					// tr: SRAM format: Even bytes only.
					suffix = C_("MegaDrive|SRAM", "(even only)");
					break;
				case 3:
					// tr: SRAM format: Odd bytes only.
					suffix = C_("MegaDrive|SRAM", "(odd only)");
					break;
				default:
					// TODO: Are both alternates 16-bit?
					// tr: SRAM format: 16-bit.
					suffix = C_("MegaDrive|SRAM", "(16-bit)");
					break;
			}

			fields->addField_string_address_range(sram_range_title,
				be32_to_cpu(pRomHeader->sram_start),
				be32_to_cpu(pRomHeader->sram_end),
				suffix, 8, RomFields::STRF_MONOSPACE);
		} else {
			fields->addField_string(sram_range_title, C_("MegaDrive", "None"));
		}

		// Check for an extra ROM chip.
		if (pRomHeader->extrom.info == cpu_to_be32(0x524F2020)) {
			// Extra ROM chip. (Sonic & Knuckles)
			// Format: 'R', 'O', 0x20, 0x20
			// Start and End locations are listed twice, in 24-bit format.
			// Not sure if there's any difference between the two...
			const uint32_t extrom_start = (pRomHeader->extrom.data[0] << 16) |
						      (pRomHeader->extrom.data[1] <<  8) |
						       pRomHeader->extrom.data[2];
			const uint32_t extrom_end   = (pRomHeader->extrom.data[3] << 16) |
						      (pRomHeader->extrom.data[4] <<  8) |
						       pRomHeader->extrom.data[5];
			fields->addField_string_address_range(C_("MegaDrive", "ExtROM Range"),
				extrom_start, extrom_end, nullptr, 8,
				RomFields::STRF_MONOSPACE);
		}
	}

	// Region code.
	// TODO: Validate the Mega CD security program?
	uint32_t md_region_check;
	if (unlikely(bRedetectRegion)) {
		md_region_check = MegaDriveRegions::parseRegionCodes(
			pRomHeader->region_codes, sizeof(pRomHeader->region_codes));
	} else {
		md_region_check = this->md_region;
	}

	static const char *const region_code_bitfield_names[] = {
		NOP_C_("Region", "Japan"),
		NOP_C_("Region", "Asia"),
		NOP_C_("Region", "USA"),
		NOP_C_("Region", "Europe"),
	};
	vector<string> *const v_region_code_bitfield_names = RomFields::strArrayToVector_i18n(
		"Region", region_code_bitfield_names, ARRAY_SIZE(region_code_bitfield_names));
	fields->addField_bitfield(C_("RomData", "Region Code"),
		v_region_code_bitfield_names, 0, md_region_check);

	// TODO: Extensions, e.g. SSF2, MegaWifi, and TeraDrive boot CPU.
}

/**
 * Add fields for the vector table.
 *
 * This function will not create a new tab.
 * If one is desired, it should be created
 * before calling this function.
 *
 * @param pVectors Vector table.
 */
void MegaDrivePrivate::addFields_vectorTable(const M68K_VectorTable *pVectors)
{
	// Use a LIST_DATA field in order to show all the vectors.
	// TODO:
	// - Make the "#" and "Address" columns monospace.
	// - Increase the height.
	// - Show on a separate line?

	static const char *const vectors_names[] = {
		// $00
		"Initial SP",
		"Entry Point",
		"Bus Error",
		"Address Error",
		// $10
		"Illegal Instruction",
		"Division by Zero",
		"CHK Exception",
		"TRAPV Exception",
		// $20
		"Privilege Violation",
		"TRACE Exception",
		"Line A Emulator",
		"Line F Emulator",
		// $60
		"Spurious Interrupt",
		"IRQ1",
		"IRQ2 (TH)",
		"IRQ3",
		// $70
		"IRQ4 (HBlank)",
		"IRQ5",
		"IRQ6 (VBlank)",
		"IRQ7 (NMI)",

		nullptr
	};

	// Map of displayed vectors to actual vectors.
	// This uses vector indees, *not* byte addresses.
	static const int8_t vectors_map[] = {
		 0,  1,  2,  3,  4,  5,  6,  7,	// $00-$1C
		 8,  9, 10, 11,			// $20-$2C
		24, 25, 26, 27, 28, 29, 30, 31,	// $60-$7C

		-1
	};

	static_assert(ARRAY_SIZE(vectors_names) == ARRAY_SIZE(vectors_map),
		"vectors_names[] and vectors_map[] are out of sync.");

	auto vv_vectors = new RomFields::ListData_t(ARRAY_SIZE(vectors_names)-1);
	auto iter = vv_vectors->begin();
	const auto vv_vectors_end = vv_vectors->end();
	for (size_t i = 0; i < ARRAY_SIZE(vectors_names)-1 && iter != vv_vectors_end; ++i, ++iter) {
		auto &data_row = *iter;
		data_row.reserve(3);

		// Actual vector number.
		const uint8_t vector_index = vectors_map[i];

		// #
		// NOTE: This is the byte address in the vector table.
		data_row.emplace_back(rp_sprintf("$%02X", vector_index*4));

		// Vector name
		data_row.emplace_back(vectors_names[i]);

		// Address
		data_row.emplace_back(rp_sprintf("$%08X", be32_to_cpu(pVectors->vectors[vector_index])));
	}

	static const char *const vectors_headers[] = {
		NOP_C_("RomData|VectorTable", "#"),
		NOP_C_("RomData|VectorTable", "Vector"),
		NOP_C_("RomData|VectorTable", "Address"),
	};
	vector<string> *const v_vectors_headers = RomFields::strArrayToVector_i18n(
		"RomData|VectorTable", vectors_headers, ARRAY_SIZE(vectors_headers));

	RomFields::AFLD_PARAMS params(RomFields::RFT_LISTDATA_SEPARATE_ROW, 8);
	params.headers = v_vectors_headers;
	params.data.single = vv_vectors;
	fields->addField_listData(C_("RomData", "Vector Table"), &params);
}

/** MegaDrive **/

/**
 * Read a Sega Mega Drive ROM.
 *
 * A ROM file must be opened by the caller. The file handle
 * will be ref()'d and must be kept open in order to load
 * data from the ROM.
 *
 * To close the file, either delete this object or call close().
 *
 * NOTE: Check isValid() to determine if this is a valid ROM.
 *
 * @param file Open ROM file.
 */
MegaDrive::MegaDrive(IRpFile *file)
	: super(new MegaDrivePrivate(this, file))
{
	RP_D(MegaDrive);
	d->className = "MegaDrive";

	if (!d->file) {
		// Could not ref() the file handle.
		return;
	}

	// Seek to the beginning of the file.
	d->file->rewind();

	// Read the ROM header. [0x800 bytes; minimum 0x200]
	uint8_t header[0x800];
	size_t size = d->file->read(header, sizeof(header));
	if (size < 0x200) {
		UNREF_AND_NULL_NOCHK(d->file);
		return;
	}

	// Check if this ROM is supported.
	DetectInfo info;
	info.header.addr = 0;
	info.header.size = static_cast<uint32_t>(size);
	info.header.pData = header;
	info.ext = nullptr;	// Not needed for MD.
	info.szFile = 0;	// Not needed for MD.
	d->romType = isRomSupported_static(&info);

	if (d->romType >= 0) {
		// Save the header for later.
		switch (d->romType & MegaDrivePrivate::ROM_FORMAT_MASK) {
			case MegaDrivePrivate::ROM_FORMAT_CART_BIN:
				d->fileType = FileType::ROM_Image;

				// MD header is at 0x100.
				// Vector table is at 0.
				memcpy(&d->vectors,    header,        sizeof(d->vectors));
				memcpy(&d->romHeader, &header[0x100], sizeof(d->romHeader));
				break;

			case MegaDrivePrivate::ROM_FORMAT_CART_SMD: {
				// Save the SMD header.
				d->pSmdHeader = new SMD_Header;
				memcpy(d->pSmdHeader, header, sizeof(*d->pSmdHeader));

				// First bank needs to be deinterleaved.
				auto block = aligned_uptr<uint8_t>(16, SuperMagicDrive::SMD_BLOCK_SIZE * 2);
				uint8_t *const smd_data = block.get();
				uint8_t *const bin_data = smd_data + SuperMagicDrive::SMD_BLOCK_SIZE;
				size = d->file->seekAndRead(512, smd_data, SuperMagicDrive::SMD_BLOCK_SIZE);
				if (size != SuperMagicDrive::SMD_BLOCK_SIZE) {
					// Short read. ROM is invalid.
					d->romType = MegaDrivePrivate::ROM_UNKNOWN;
					break;
				}

				// Decode the SMD block.
				SuperMagicDrive::decodeBlock(bin_data, smd_data);

				// MD header is at 0x100.
				// Vector table is at 0.
				d->fileType = FileType::ROM_Image;
				memcpy(&d->vectors,    bin_data,        sizeof(d->vectors));
				memcpy(&d->romHeader, &bin_data[0x100], sizeof(d->romHeader));
				break;
			}

			case MegaDrivePrivate::ROM_FORMAT_DISC_2048:
				d->fileType = FileType::DiscImage;

				// MCD-specific header is at 0. [TODO]
				// MD-style header is at 0x100.
				// No vector table is present on the disc.
				memcpy(&d->romHeader, &header[0x100], sizeof(d->romHeader));
				break;

			case MegaDrivePrivate::ROM_FORMAT_DISC_2352:
				if (size < 0x210) {
					// Not enough data for a 2352-byte sector disc image.
					d->romType = MegaDrivePrivate::ROM_UNKNOWN;
					UNREF_AND_NULL_NOCHK(d->file);
					return;
				}

				d->fileType = FileType::DiscImage;
				// MCD-specific header is at 0x10. [TODO]
				// MD-style header is at 0x110.
				// No vector table is present on the disc.
				memcpy(&d->romHeader, &header[0x110], sizeof(d->romHeader));
				break;

			case MegaDrivePrivate::ROM_FORMAT_UNKNOWN:
			default:
				d->romType = MegaDrivePrivate::ROM_UNKNOWN;
				break;
		}
	}

	d->isValid = (d->romType >= 0);
	if (!d->isValid) {
		// Not valid. Close the file.
		UNREF_AND_NULL_NOCHK(d->file);
	}

	// Parse the MD region code.
	d->md_region = MegaDriveRegions::parseRegionCodes(
		d->romHeader.region_codes, sizeof(d->romHeader.region_codes));

	// Determine the MIME type.
	const uint8_t sysID = (d->romType & MegaDrivePrivate::ROM_SYSTEM_MASK);
	if (sysID < ARRAY_SIZE(d->mimeType_tbl)-1) {
		d->mimeType = d->mimeType_tbl[sysID];
	}

	// If this is S&K, try reading the locked-on ROM header.
	const int64_t fileSize = d->file->size();
	if (sysID == MegaDrivePrivate::ROM_SYSTEM_MD && fileSize >= ((2*1024*1024)+512) &&
	    !memcmp(d->romHeader.serial, "GM MK-1563 -00", sizeof(d->romHeader.serial)))
	{
		// Check if a locked-on ROM is present.
		if ((d->romType & MegaDrivePrivate::ROM_FORMAT_MASK) == MegaDrivePrivate::ROM_FORMAT_CART_SMD) {
			// Load the 16K block and deinterleave it.
			if (fileSize >= (2*1024*1024)+512+16384) {
				auto block = aligned_uptr<uint8_t>(16, SuperMagicDrive::SMD_BLOCK_SIZE * 2);
				uint8_t *const smd_data = block.get();
				uint8_t *const bin_data = smd_data + SuperMagicDrive::SMD_BLOCK_SIZE;
				size_t size = d->file->seekAndRead(512 + (2*1024*1024), smd_data, SuperMagicDrive::SMD_BLOCK_SIZE);
				if (size == SuperMagicDrive::SMD_BLOCK_SIZE) {
					// Deinterleave the block.
					SuperMagicDrive::decodeBlock(bin_data, smd_data);
					d->pRomHeaderLockOn = new MD_RomHeader;
					memcpy(d->pRomHeaderLockOn, &bin_data[0x100], sizeof(*d->pRomHeaderLockOn));
				}
			}
		} else {
			// Load the header directly.
			d->pRomHeaderLockOn = new MD_RomHeader;
			size_t size = d->file->seekAndRead((2*1024*1024)+0x100, d->pRomHeaderLockOn, sizeof(*d->pRomHeaderLockOn));
			if (size != sizeof(*d->pRomHeaderLockOn)) {
				// Error loading the ROM header.
				delete d->pRomHeaderLockOn;
				d->pRomHeaderLockOn = nullptr;
			}
		}

		if (d->pRomHeaderLockOn) {
			// Verify the "SEGA" magic.
			static const char sega_magic[4] = {'S','E','G','A'};
			if (!memcmp(&d->pRomHeaderLockOn->system[0], sega_magic, sizeof(sega_magic)) ||
			    !memcmp(&d->pRomHeaderLockOn->system[1], sega_magic, sizeof(sega_magic)))
			{
				// Found the "SEGA" magic.
			}
			else
			{
				// "SEGA" magic not found.
				// Assume this is invalid.
				delete d->pRomHeaderLockOn;
				d->pRomHeaderLockOn = nullptr;
			}
		}
	}
}

/** ROM detection functions. **/

/**
 * Is a ROM image supported by this class?
 * @param info DetectInfo containing ROM detection information.
 * @return Class-specific system ID (>= 0) if supported; -1 if not.
 */
int MegaDrive::isRomSupported_static(const DetectInfo *info)
{
	assert(info != nullptr);
	assert(info->header.pData != nullptr);
	assert(info->header.addr == 0);
	if (!info || !info->header.pData ||
	    info->header.addr != 0 ||
	    info->header.size < 0x200)
	{
		// Either no detection information was specified,
		// or the header is too small.
		return -1;
	}

	// ROM header.
	const uint8_t *const pHeader = info->header.pData;

	// Magic strings.
	static const char sega_magic[4+1] = "SEGA";
	static const char segacd_magic[16+1] = "SEGADISCSYSTEM  ";

	// Extra system types from:
	// - https://www.plutiedev.com/rom-header#system
	static const struct {
		const char sys_name[20];
		uint8_t sys_name_len_100h;	// Length to check at $100
		uint8_t sys_name_len_101h;	// Length to check at $101
		uint32_t system_id;
	} cart_magic[] = {
		{"SEGA PICO       ", 16, 15, MegaDrivePrivate::ROM_SYSTEM_PICO},
		{"SEGA 32X        ", 16, 15, MegaDrivePrivate::ROM_SYSTEM_32X},
		{"SEGA SSF        ", 16, 15, MegaDrivePrivate::ROM_SYSTEM_MD |
		                             MegaDrivePrivate::ROM_EXT_SSF2},
		{"SEGA EVERDRIVE  ", 16, 15, MegaDrivePrivate::ROM_SYSTEM_MD |
		                             MegaDrivePrivate::ROM_EXT_EVERDRIVE},
		{"SEGA MEGAWIFI   ", 16, 15, MegaDrivePrivate::ROM_SYSTEM_MD |
		                             MegaDrivePrivate::ROM_EXT_MEGAWIFI},
		{"SEGA TERA68K    ", 16, 15, MegaDrivePrivate::ROM_SYSTEM_TERADRIVE |
		                             MegaDrivePrivate::ROM_EXT_TERADRIVE_68k},
		{"SEGA TERA286    ", 16, 15, MegaDrivePrivate::ROM_SYSTEM_TERADRIVE |
		                             MegaDrivePrivate::ROM_EXT_TERADRIVE_x86},

		// NOTE: Previously, we were checking for
		// "SEGA MEGA DRIVE" and "SEGA GENESIS".
		// For broader compatibility with unlicensed ROMs,
		// just check for "SEGA". (Required for TMSS.)
		{"SEGA",              4,  4, MegaDrivePrivate::ROM_SYSTEM_MD},
	};

	// Check for Sega CD.
	// TODO: Gens/GS II lists "ISO/2048", "ISO/2352",
	// "BIN/2048", and "BIN/2352". I don't think that's
	// right; there should only be 2048 and 2352.
	// TODO: Detect Sega CD 32X.
	// TODO: Use a struct instead of raw bytes?
	if (!memcmp(&pHeader[0x0010], segacd_magic, sizeof(segacd_magic)-1)) {
		// Found a Sega CD disc image. (2352-byte sectors)
		return MegaDrivePrivate::ROM_SYSTEM_MCD |
		       MegaDrivePrivate::ROM_FORMAT_DISC_2352;
	} else if (!memcmp(&pHeader[0x0000], segacd_magic, sizeof(segacd_magic)-1)) {
		// Found a Sega CD disc image. (2048-byte sectors)
		return MegaDrivePrivate::ROM_SYSTEM_MCD |
		       MegaDrivePrivate::ROM_FORMAT_DISC_2048;
	}

	// Check for SMD format. (Mega Drive only)
	if (info->header.size >= 0x300) {
		// Check if "SEGA" is in the header in the correct place
		// for a plain binary ROM.
		if (memcmp(&pHeader[0x100], sega_magic, sizeof(sega_magic)) != 0 &&
		    memcmp(&pHeader[0x101], sega_magic, sizeof(sega_magic)) != 0)
		{
			// "SEGA" is not in the header. This might be SMD.
			const SMD_Header *pSmdHeader = reinterpret_cast<const SMD_Header*>(pHeader);
			if (pSmdHeader->id[0] == 0xAA && pSmdHeader->id[1] == 0xBB &&
			    pSmdHeader->smd.file_data_type == SMD_FDT_68K_PROGRAM &&
			    pSmdHeader->file_type == SMD_FT_SMD_GAME_FILE)
			{
				// This is an SMD-format ROM.
				// TODO: Show extended information from the SMD header,
				// including "split" and other stuff?
				return MegaDrivePrivate::ROM_SYSTEM_MD |
				       MegaDrivePrivate::ROM_FORMAT_CART_SMD;
			}
		}
	}

	// Check for other MD-based cartridge formats.
	int sysId = MegaDrivePrivate::ROM_UNKNOWN;
	for (int i = 0; i < ARRAY_SIZE(cart_magic); i++) {
		if (!memcmp(&pHeader[0x100], cart_magic[i].sys_name, cart_magic[i].sys_name_len_100h) ||
		    !memcmp(&pHeader[0x101], cart_magic[i].sys_name, cart_magic[i].sys_name_len_101h))
		{
			// Found a matching system name.
			sysId = cart_magic[i].system_id;
			break;
		}
	}

	uint32_t sysIdOnly = (sysId & MegaDrivePrivate::ROM_SYSTEM_MASK);
	if (sysIdOnly == MegaDrivePrivate::ROM_SYSTEM_MD ||
	    sysIdOnly == MegaDrivePrivate::ROM_SYSTEM_32X)
	{
		// Verify the 32X security program if possible.
		static const uint32_t secprgaddr = 0x512;
		static const char secprgdesc[] = "MARS Initial & Security Program";
		if (info->header.size >= secprgaddr + sizeof(secprgdesc) - 1) {
			// TODO: Check other parts of the security program?
			if (!memcmp(&pHeader[secprgaddr], secprgdesc, sizeof(secprgdesc)-1)) {
				// Module name is correct.
				// TODO: Does the ROM header have to say "SEGA 32X"?
				sysId = MegaDrivePrivate::ROM_SYSTEM_32X;
			} else {
				// Module name is incorrect.
				// This ROM cannot activate 32X mode.
				sysId = MegaDrivePrivate::ROM_SYSTEM_MD;
			}
			sysId &= ~MegaDrivePrivate::ROM_SYSTEM_MASK;
			sysId |= sysIdOnly;
		}
	}

	return MegaDrivePrivate::ROM_FORMAT_CART_BIN | sysId;
}

/**
 * Get the name of the system the loaded ROM is designed for.
 * @param type System name type. (See the SystemName enum.)
 * @return System name, or nullptr if type is invalid.
 */
const char *MegaDrive::systemName(unsigned int type) const
{
	RP_D(const MegaDrive);
	if (!d->isValid || !isSystemNameTypeValid(type))
		return nullptr;

	// FIXME: Lots of system names and regions to check.
	// Also, games can be region-free, so we need to check
	// against the host system's locale.
	// For now, just use the generic "Mega Drive".

	static_assert(SYSNAME_TYPE_MASK == 3,
		"MegaDrive::systemName() array index optimization needs to be updated.");

	unsigned int romSys = (d->romType & MegaDrivePrivate::ROM_SYSTEM_MASK);
	if (romSys > MegaDrivePrivate::ROM_SYSTEM_MAX) {
		// Invalid system type. Default to MD.
		romSys = MegaDrivePrivate::ROM_SYSTEM_MD;
	}

	// sysNames[] bitfield:
	// - Bits 0-1: Type. (long, short, abbreviation)
	// - Bits 2-4: System type.

	static_assert(SYSNAME_REGION_MASK == (1U << 2),
		"MegaDrive::systemName() region type optimization needs to be updated.");
	const unsigned int idx = (type & SYSNAME_TYPE_MASK);
	if ((type & SYSNAME_REGION_MASK) == SYSNAME_REGION_GENERIC) {
		// Generic system name.
		static const char *const sysNames[6][4] = {
			{"Sega Mega Drive", "Mega Drive", "MD", nullptr},
			{"Sega Mega CD", "Mega CD", "MCD", nullptr},
			{"Sega 32X", "Sega 32X", "32X", nullptr},
			{"Sega Mega CD 32X", "Mega CD 32X", "MCD32X", nullptr},
			{"Sega Pico", "Pico", "Pico", nullptr},
			{"Sega Teradrive", "Teradrive", "Teradrive", nullptr}
		};
		return sysNames[romSys][idx];
	}

	// Get the system branding region.
	const MegaDriveRegions::MD_BrandingRegion md_bregion =
		MegaDriveRegions::getBrandingRegion(d->md_region);
	switch (md_bregion) {
		case MegaDriveRegions::MD_BrandingRegion::Japan:
		default: {
			static const char *const sysNames_JP[6][4] = {
				{"Sega Mega Drive", "Mega Drive", "MD", nullptr},
				{"Sega Mega CD", "Mega CD", "MCD", nullptr},
				{"Sega Super 32X", "Super 32X", "32X", nullptr},
				{"Sega Mega CD 32X", "Mega CD 32X", "MCD32X", nullptr},
				{"Sega Kids Computer Pico", "Kids Computer Pico", "Pico", nullptr},
				{"Sega Teradrive", "Teradrive", "Teradrive", nullptr}
			};
			return sysNames_JP[romSys][idx];
		}

		case MegaDriveRegions::MD_BrandingRegion::USA: {
			static const char *const sysNames_US[6][4] = {
				// TODO: "MD" or "Gen"?
				{"Sega Genesis", "Genesis", "MD", nullptr},
				{"Sega CD", "Sega CD", "MCD", nullptr},
				{"Sega 32X", "Sega 32X", "32X", nullptr},
				{"Sega CD 32X", "Sega CD 32X", "MCD32X", nullptr},
				{"Sega Pico", "Pico", "Pico", nullptr},
				{"Sega Teradrive", "Teradrive", "Teradrive", nullptr}
			};
			return sysNames_US[romSys][idx];
		}

		case MegaDriveRegions::MD_BrandingRegion::Europe: {
			static const char *const sysNames_EU[6][4] = {
				{"Sega Mega Drive", "Mega Drive", "MD", nullptr},
				{"Sega Mega CD", "Mega CD", "MCD", nullptr},
				{"Sega Mega Drive 32X", "Mega Drive 32X", "32X", nullptr},
				{"Sega Mega CD 32X", "Sega Mega CD 32X", "MCD32X", nullptr},
				{"Sega Pico", "Pico", "Pico", nullptr},
				{"Sega Teradrive", "Teradrive", "Teradrive", nullptr}
			};
			return sysNames_EU[romSys][idx];
		}

		case MegaDriveRegions::MD_BrandingRegion::South_Korea: {
			static const char *const sysNames_KR[6][4] = {
				// TODO: "MD" or something else?
				{"Samsung Super Aladdin Boy", "Super Aladdin Boy", "MD", nullptr},
				{"Samsung CD Aladdin Boy", "CD Aladdin Boy", "MCD", nullptr},
				{"Samsung Super 32X", "Super 32X", "32X", nullptr},
				{"Sega Mega CD 32X", "Sega Mega CD 32X", "MCD32X", nullptr},
				{"Sega Pico", "Pico", "Pico", nullptr},
				{"Sega Teradrive", "Teradrive", "Teradrive", nullptr}
			};
			return sysNames_KR[romSys][idx];
		}

		case MegaDriveRegions::MD_BrandingRegion::Brazil: {
			static const char *const sysNames_BR[6][4] = {
				{"Sega Mega Drive", "Mega Drive", "MD", nullptr},
				{"Sega CD", "Sega CD", "MCD", nullptr},
				{"Sega Mega 32X", "Mega 32X", "32X", nullptr},
				{"Sega CD 32X", "Sega CD 32X", "MCD32X", nullptr},
				{"Sega Pico", "Pico", "Pico", nullptr},
				{"Sega Teradrive", "Teradrive", "Teradrive", nullptr}
			};
			return sysNames_BR[romSys][idx];
		}
	}

	// Should not get here...
	return nullptr;
}

/**
 * Get a list of all supported file extensions.
 * This is to be used for file type registration;
 * subclasses don't explicitly check the extension.
 *
 * NOTE: The extensions include the leading dot,
 * e.g. ".bin" instead of "bin".
 *
 * NOTE 2: The array and the strings in the array should
 * *not* be freed by the caller.
 *
 * @return NULL-terminated array of all supported file extensions, or nullptr on error.
 */
const char *const *MegaDrive::supportedFileExtensions_static(void)
{
	static const char *const exts[] = {
		".gen", ".smd",
		".32x", ".pco",
		".sgd",	".68k", // Official extensions

		// NOTE: These extensions may cause conflicts on
		// Windows if fallback handling isn't working.
		".md",	// conflicts with Markdown
		".bin",	// too generic
		".iso",	// too generic

		nullptr
	};
	return exts;
}

/**
 * Get a list of all supported MIME types.
 * This is to be used for metadata extractors that
 * must indicate which MIME types they support.
 *
 * NOTE: The array and the strings in the array should
 * *not* be freed by the caller.
 *
 * @return NULL-terminated array of all supported file extensions, or nullptr on error.
 */
const char *const *MegaDrive::supportedMimeTypes_static(void)
{
	return MegaDrivePrivate::mimeType_tbl;
}

/**
 * Load field data.
 * Called by RomData::fields() if the field data hasn't been loaded yet.
 * @return Number of fields read on success; negative POSIX error code on error.
 */
int MegaDrive::loadFieldData(void)
{
	RP_D(MegaDrive);
	if (!d->fields->empty()) {
		// Field data *has* been loaded...
		return 0;
	} else if (!d->file || !d->file->isOpen()) {
		// File isn't open.
		// NOTE: We already loaded the header,
		// so *maybe* this is okay?
		return -EBADF;
	} else if (!d->isValid || d->romType < 0) {
		// Unknown ROM image type.
		return -EIO;
	}

	// Maximum number of fields:
	// - ROM Header: 13
	// - Vector table: 1 (LIST_DATA)
	d->fields->reserve(14);

	// Reserve at least 2 tabs.
	d->fields->reserveTabs(2);

	// ROM Header.
	d->fields->setTabName(0, C_("MegaDrive", "ROM Header"));
	d->addFields_romHeader(&d->romHeader);

	if (!d->isDisc()) {
		// Vector table. (MD only; not valid for Mega CD.)
		d->fields->addTab(C_("MegaDrive", "Vector Table"));
		d->addFields_vectorTable(&d->vectors);
	}

	// Check for a locked-on ROM image.
	if (d->pRomHeaderLockOn) {
		// Locked-on ROM is present.
		d->fields->addTab(C_("MegaDrive", "Locked-On ROM Header"));
		d->addFields_romHeader(d->pRomHeaderLockOn, true);
	}

	// Try to open the ISO-9660 object.
	// NOTE: Only done here because the ISO-9660 fields
	// are used for field info only.
	if (d->isDisc()) {
		ISO *const isoData = new ISO(d->file);
		if (isoData->isOpen()) {
			// Add the fields.
			const RomFields *const isoFields = isoData->fields();
			assert(isoFields != nullptr);
			if (isoFields) {
				d->fields->addFields_romFields(isoFields,
					RomFields::TabOffset_AddTabs);
			}
		}
		isoData->unref();
	}

	// Finished reading the field data.
	return static_cast<int>(d->fields->count());
}

/**
 * Check for "viewed" achievements.
 *
 * @return Number of achievements unlocked.
 */
int MegaDrive::checkViewedAchievements(void) const
{
	RP_D(const MegaDrive);
	if (!d->isValid) {
		// ROM is not valid.
		return 0;
	}

	Achievements *const pAch = Achievements::instance();
	int ret = 0;

	if (d->pRomHeaderLockOn) {
		// Is it S&K locked on to S&K?
		if (!memcmp(d->pRomHeaderLockOn->serial, "GM MK-1563 -00", sizeof(d->pRomHeaderLockOn->serial))) {
			// It is!
			pAch->unlock(Achievements::ID::ViewedMegaDriveSKwithSK);
			ret++;
		}
	}

	return ret;
}

}
