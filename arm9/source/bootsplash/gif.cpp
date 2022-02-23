// From TWiLight Menu++
// https://github.com/DS-Homebrew/TWiLightMenu/blob/a7e61109f8736d24074f70933ed172d540dc921b/title/arm9/source/graphics/gif.cpp

#include "gif.h"
// #include "myDSiMode.h"
#include "tonccpy.h"
#include "lzw.h"

#include <stdio.h>

std::vector<Gif *> Gif::_animating;

void Gif::timerHandler(void) {
	for (auto gif : _animating) {
		gif->displayFrame();
	}
}

void Gif::displayFrame(void) {
	if (_paused || ++_currentDelayProgress < _currentDelay)
		return;

	_currentDelayProgress = 0;
	_waitingForInput = false;

	if (_currentFrame >= _frames.size()) {
		_currentFrame = 0;
		_currentLoop++;
	}

	if (_currentLoop > _loopCount) {
		_finished = true;
		_paused = true;
		_currentLoop = 0;
		return;
	}

	Frame &frame = _frames[_currentFrame++];

	if (frame.hasGCE) {
		_currentDelay = frame.gce.delay;
		if (frame.gce.delay == 0) {
			_finished = true;
			_paused = true;
		} else if (frame.gce.userInputFlag) {
			_waitingForInput = true;
		}
	}

	std::vector<u16> &colorTable = frame.descriptor.lctFlag ? frame.lct : _gct;

	tonccpy(_top ? BG_PALETTE : BG_PALETTE_SUB, colorTable.data(), colorTable.size() * 2);
	tonccpy(_top ? BG_PALETTE : BG_PALETTE_SUB, colorTable.data(), colorTable.size() * 2);

	// Disposal method 2 = fill with bg color
	if (frame.gce.disposalMethod == 2)
		toncset(_top ? BG_GFX : BG_GFX_SUB, header.bgColor, 256 * 192);

	if(_compressed) { // Was left compressed to be able to fit
		int x = 0, y = 0;
		u8 *dst = (u8*)(_top ? BG_GFX : BG_GFX_SUB) + (frame.descriptor.y + y + (192 - header.height) / 2) * 256 + frame.descriptor.x + (256 - header.width) / 2;
		u8 row[frame.descriptor.w];
		auto flush_fn = [&dst, &row, &x, &y, &frame](std::vector<u8>::const_iterator begin, std::vector<u8>::const_iterator end) {
			for (; begin != end; ++begin) {
				if (!frame.gce.transparentColorFlag || *begin != frame.gce.transparentColor)
					row[x] = *begin;
				else
					row[x] = *(dst + x);
				x++;
				if (x >= frame.descriptor.w) {
					tonccpy(dst, row, frame.descriptor.w);
					y++;
					x = 0;
					dst += 256;
				}
			}
		};

		LZWReader reader(frame.image.lzwMinimumCodeSize, flush_fn);
		reader.decode(frame.image.imageData.begin(), frame.image.imageData.end());
	} else { // Already decompressed, just copy
		auto it = frame.image.imageData.begin();
		for(int y = 0; y < frame.descriptor.h; y++) {
			u8 *dst = (u8*)(_top ? BG_GFX : BG_GFX_SUB) + (frame.descriptor.y + y + (192 - header.height) / 2) * 256 + frame.descriptor.x + (256 - header.width) / 2;
			u8 row[frame.descriptor.w];
			for(int x = 0; x < frame.descriptor.w; x++, it++) {
				if (!frame.gce.transparentColorFlag || *it != frame.gce.transparentColor)
					row[x] = *it;
				else
					row[x] = *(dst + x);
			}
			tonccpy(dst, row, frame.descriptor.w);
		}
	}
}

bool Gif::load(const u8 *data, u32 size, bool top, bool animate) {
	_top = top;

	if (!data)
		return false;

	const u8 *ptr = data;

	_compressed = size > ((isDSiMode() || REG_SCFG_EXT != 0) ? 1 << 20 : 1 << 18); // Decompress files bigger than 1MiB (256KiB in DS Mode) while drawing

	// Reserve space for 2,000 frames
	_frames.reserve(2000);

	// Read header
	tonccpy(&header, ptr, sizeof(header));
	ptr += sizeof(header);

	// Check that this is a GIF
	if (memcmp(header.signature, "GIF87a", sizeof(header.signature)) != 0 && memcmp(header.signature, "GIF89a", sizeof(header.signature)) != 0) {
		return false;
	}

	// Load global color table
	if (header.gctFlag) {
		int numColors = (2 << header.gctSize);

		_gct = std::vector<u16>(numColors);
		for (int i = 0; i < numColors; i++) {
			_gct[i] = ptr[0] >> 3 | (ptr[1] >> 3) << 5 | (ptr[2] >> 3) << 10 | BIT(15);
			ptr += 3;
		}
	}

	// Set default loop count to 0, uninitialized default is 0xFFFF so it's infinite
	_loopCount = 0;

	Frame frame;
	while (1) {
		switch (*(ptr++)) {
			case 0x21: { // Extension
				switch (*(ptr++)) {
					case 0xF9: { // Graphics Control
						frame.hasGCE = true;
						tonccpy(&frame.gce, ptr + 1, *ptr);
						ptr += *ptr + 1;
						if(frame.gce.delay < 2) // If delay is less then 2, change it to 10
							frame.gce.delay = 10;
						ptr++; // Terminator
						break;
					} case 0x01: { // Plain text
						// Unsupported for now, I can't even find a text GIF to test with
						// frame.hasText = true;
						// fread(&frame.textDescriptor, 1, sizeof(frame.textDescriptor), file);
						ptr += 12;
						while (*ptr) {
							// char temp[size + 1];
							// fread(temp, 1, size, file);
							// frame.text += temp;
							ptr += *ptr + 1;
						}
						// _frames.push_back(frame);
						// frame = Frame();
						break;
					} case 0xFF: { // Application extension
						if (*(ptr++) == 0xB) {
							if (strcmp((const char *)ptr, "NETSCAPE2.0") == 0) { // Check for Netscape loop count
								ptr += 0xB + 2;
								tonccpy(&_loopCount, ptr, sizeof(_loopCount));
								ptr += sizeof(_loopCount);
								if(_loopCount == 0) // If loop count 0 is specified, loop forever
									_loopCount = 0xFFFF;
								ptr++; //terminator
								break;
							} else {
								ptr += 0xB;
							}
						}
					} case 0xFE: { // Comment
						// Skip comments and unsupported application extionsions
						while (*ptr) {
							ptr += *ptr + 1;
						}
						break;
					}
				}
				break;
			} case 0x2C: { // Image desriptor
				frame.hasImage = true;
				tonccpy(&frame.descriptor, ptr, sizeof(frame.descriptor));
				ptr += sizeof(frame.descriptor);
				if (frame.descriptor.lctFlag) {
					int numColors = 2 << frame.descriptor.lctSize;
					frame.lct = std::vector<u16>(numColors);
					for (int i = 0; i < numColors; i++) {
						frame.lct[i] = ptr[0] >> 3 | (ptr[1] >> 3) << 5 | (ptr[2] >> 3) << 10 | BIT(15);
						ptr += 3;
					}
				}

				frame.image.lzwMinimumCodeSize = *(ptr++);
				if(_compressed) { // Leave compressed to fit more in RAM
					while (*ptr) {
						size_t end = frame.image.imageData.size();
						frame.image.imageData.resize(end + *ptr);
						tonccpy(frame.image.imageData.data() + end, ptr + 1, *ptr);
						ptr += *ptr + 1;
					}
				} else { // Decompress now for faster draw
					frame.image.imageData = std::vector<u8>(frame.descriptor.w * frame.descriptor.h);
					auto it = frame.image.imageData.begin();
					auto flush_fn = [&it, &frame](std::vector<u8>::const_iterator begin, std::vector<u8>::const_iterator end) {
						std::copy(begin, end, it);
						it += std::distance(begin, end);
					};
					LZWReader reader(frame.image.lzwMinimumCodeSize, flush_fn);

					while (*ptr) {
						std::vector<u8> buffer(*ptr);
						tonccpy(buffer.data(), ptr + 1, *ptr);
						ptr += *ptr + 1;
						reader.decode(buffer.begin(), buffer.end());
					}
				}

				_frames.push_back(frame);
				frame = Frame();
				break;
			} case 0x3B: { // Trailer
				goto breakWhile;
			}
		}
	}
	breakWhile:

	_paused = false;
	_finished = loopForever();
	_frames.shrink_to_fit();
	if(animate)
		_animating.push_back(this);

	return true;
}
