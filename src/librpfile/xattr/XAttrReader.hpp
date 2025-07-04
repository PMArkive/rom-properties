/***************************************************************************
 * ROM Properties Page shell extension. (librpfile)                        *
 * XAttrReader.hpp: Extended Attribute reader                              *
 *                                                                         *
 * Copyright (c) 2016-2025 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#pragma once

// Common macros
#include "common.h"
#include "dll-macros.h"	// for RP_LIBROMDATA_PUBLIC

// C includes (C++ namespace)
#include <cstdint>

// C++ includes
#include <map>
#include <string>
#include <utility>

namespace LibRpFile {

class XAttrReaderPrivate;
class RP_LIBROMDATA_PUBLIC XAttrReader
{
public:
	explicit XAttrReader(const char *filename);
#if defined(_WIN32) && defined(_UNICODE)
	explicit XAttrReader(const wchar_t *filename);
#endif /* _WIN32 && _UNICODE */
public:
	~XAttrReader();

private:
	RP_DISABLE_COPY(XAttrReader)
protected:
	friend class XAttrReaderPrivate;
	XAttrReaderPrivate *const d_ptr;

public:
	/**
	 * Get the last error number.
	 * @return Last error number (POSIX error code)
	 */
	int lastError(void) const;

public:
	/**
	 * Known compression algorithms.
	 * TODO: Expand for Linux file systems, e.g. ZFS, Btrfs, bcachefs
	 */
	enum class ZAlgorithm {
		None,

		// NTFS-specific compression algorithms
		LZNT1,
		XPRESS4K,
		LZX,
		XPRESS8K,
		XPRESS16K,

		// btrfs compression algorithms
		ZLIB,
		LZO,
		ZSTD,

		Max
	};

	/**
	 * Get a string representation of a compression algorithm.
	 * @return String representation, or nullptr if the value is invalid.
	 */
	static const char *zAlgorithmToString(ZAlgorithm zAlgorithm);

public:
	/**
	 * Does this file have Ext2 attributes?
	 * @return True if it does; false if not.
	 */
	bool hasExt2Attributes(void) const;

	/**
	 * Get this file's Ext2 attributes.
	 * @return Ext2 attributes
	 */
	int ext2Attributes(void) const;

	/**
	 * Does this file have XFS attributes?
	 * @return True if it does; false if not.
	 */
	bool hasXfsAttributes(void) const;

	/**
	 * Get this file's XFS xflags.
	 * @return XFS xflags
	 */
	uint32_t xfsXFlags(void) const;

	/**
	 * Get this file's XFS project ID.
	 * @return XFS project ID
	 */
	uint32_t xfsProjectId(void) const;

	/**
	 * Does this file have MS-DOS attributes?
	 * @return True if it does; false if not.
	 */
	bool hasDosAttributes(void) const;

	/**
	 * Get this file's MS-DOS attributes.
	 * @return MS-DOS attributes
	 */
	unsigned int dosAttributes(void) const;

	/**
	 * Get the valid MS-DOS attributes for this file.
	 * Compressed and Encrypted are available on NTFS but not FAT.
	 * @return Valid MS-DOS attributes
	 */
	unsigned int validDosAttributes(void) const;

	/**
	 * Get the compression algoirthm used for this file.
	 * @return Compression algorithm
	 */
	ZAlgorithm zAlgorithm(void) const;

	/**
	 * Does this file have a compression algorithm specified?
	 * @return True if it does; false if not.
	 */
	bool hasZAlgorithm(void) const;

	/**
	 * Get the compression level used for this file.
	 * @return Compression level (0 for not specified)
	 */
	int zLevel(void) const;

	/**
	 * Does this file have a compression level specified?
	 * @return True if it does; false if not.
	 */
	bool hasZLevel(void) const;

	/**
	 * Does this file have generic extended attributes?
	 * (POSIX xattr on Linux; ADS on Windows)
	 * @return True if it does; false if not.
	 */
	bool hasGenericXAttrs(void) const;

	/**
	 * Extended attribute map (UTF-8)
	 * - Key: Name
	 * - Value: Value
	 */
	typedef std::map<std::string, std::string> XAttrList;

	/**
	 * Get the list of extended attributes.
	 * @return Extended attributes
	 */
	const XAttrList &genericXAttrs(void) const;
};

}
