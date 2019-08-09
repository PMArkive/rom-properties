/***************************************************************************
 * ROM Properties Page shell extension. (librptexture)                     *
 * FileFormat.hpp: Texture file format base class.                         *
 *                                                                         *
 * Copyright (c) 2016-2019 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#ifndef __ROMPROPERTIES_LIBRPTEXTURE_FILEFORMAT_FILEFORMAT_HPP__
#define __ROMPROPERTIES_LIBRPTEXTURE_FILEFORMAT_FILEFORMAT_HPP__

// TODO: Move to librpfile or similar?
#include "librpbase/common.h"

namespace LibRpTexture {

class rp_image;

class FileFormatPrivate;
class FileFormat
{
	protected:
		explicit FileFormat(FileFormatPrivate *d);

	protected:
		/**
		 * FileFormat destructor is protected.
		 * Use unref() instead.
		 */
		virtual ~FileFormat();

	private:
		RP_DISABLE_COPY(FileFormat)
	protected:
		friend class FileFormatPrivate;
		FileFormatPrivate *const d_ptr;

	public:
		/**
		 * Take a reference to this FileFormat* object.
		 * @return this
		 */
		FileFormat *ref(void);

		/**
		 * Unreference this FileFormat* object.
		 * If ref_cnt reaches 0, the FileFormat* object is deleted.
		 */
		void unref(void);

	public:
		/**
		 * Is the texture file valid?
		 * @return True if valid; false if not.
		 */
		bool isValid(void) const;

		/**
		 * Is the texture file open?
		 * @return True if open; false if not.
		 */
		bool isOpen(void) const;

		/**
		 * Close the opened file.
		 */
		virtual void close(void);

	public:
		/** Propety accessors **/

		/**
		 * Get the texture format name.
		 * @return Texture format name, or nullptr on error.
		 */
		virtual const char *textureFormatName(void) const = 0;

		// TODO: Supported file extensions and MIME types.

		// TODO: Move the dimensions code up to the FileFormat base class.

		/**
		 * Get the image width.
		 * @return Image width.
		 */
		virtual int width(void) const = 0;

		/**
		 * Get the image height.
		 * @return Image height.
		 */
		virtual int height(void) const = 0;

		/**
		 * Get the image dimensions.
		 * If the image is 2D, then 'z' will be set to zero.
		 * @param pBuf Three-element array for [x, y, z].
		 * @return 0 on success; negative POSIX error code on error.
		 */
		virtual int getDimensions(int *pBuf) const = 0;

		/**
		 * Get the pixel format, e.g. "RGB888" or "DXT1".
		 * @return Pixel format, or nullptr if unavailable.
		 */
		virtual const char *pixelFormat(void) const = 0;

		/**
		 * Get the mipmap count.
		 * @return Number of mipmaps. (0 if none; -1 if format doesn't support mipmaps)
		 */
		virtual int mipmapCount(void) const = 0;

	public:
		/** Image accessors **/

		/**
		 * Get the image.
		 * For textures with mipmaps, this is the largest mipmap.
		 * The image is owned by this object.
		 * @return Image, or nullptr on error.
		 */
		virtual const rp_image *image(void) const = 0;

		/**
		 * Get the image for the specified mipmap.
		 * Mipmap 0 is the largest image.
		 * @param num Mipmap number.
		 * @return Image, or nullptr on error.
		 */
		virtual const rp_image *mipmap(int num) const = 0;
};

}

#endif /* __ROMPROPERTIES_LIBRPTEXTURE_FILEFORMAT_FILEFORMAT_HPP__ */