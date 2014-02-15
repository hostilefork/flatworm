// flv.h
//
// A Flash Video file (.FLV file extension) consists of a short header, and 
// then interleaved audio, video, and metadata packets. The audio and video 
// packets are stored very similarly to those in SWF, and the metadata packets
// consist of AMF data.

// Note: field comments are // example // description

// Unlike SWF files, FLV files store multibyte integers in big-endian byte order. For example, as 
// a UI16 in SWF file format, the byte sequence that represents the number300 (0x12C) is 
// 0x2C0x01; as a UI16 in FLV file format, the byte sequence that represents the number300 is 
// 0x010x2C. Also, FLV files use a 3-byte integer type, UI24, that is not used in SWF files.

#include <windows.h>

typedef BYTE uint32_be[4];
typedef BYTE uint24_be[3];
typedef BYTE uint16_be[2];
typedef BYTE uint8;

inline unsigned int DecodeEndian(const uint24_be& x)
{
	return x[0] | (static_cast<unsigned int>(x[1]) << 8) | (static_cast<unsigned int>(x[2]) << 16); 
}

inline unsigned int DecodeEndian(const uint16_be& x)
{
	return x[0] | (static_cast<unsigned int>(x[1]) << 8);
}

inline unsigned int DecodeEndian(const uint32_be& x)
{
	return x[0] | (static_cast<unsigned int>(x[1]) << 8) | (static_cast<unsigned int>(x[2]) << 16) | (static_cast<unsigned int>(x[3]) << 24);
}

struct FLV_HEADER {
	BYTE Signature[3]; //	"FLV" // 	Always “FLV”
	uint8 Version; // "\x01" (1) //	Currently 1 for known FLV files
	uint8 bitmask; // 	"\x05" (5, audio+video) //	Bitmask: 4 is audio, 1 is video
	uint32_be Offset; //	 	"\x00\x00\x00\x09" (9) // 	Total size of header (always 9 for known FLV files)
};

struct FLV_PREVIOUS_TAG_SIZE {
	uint32_be PreviousTagSize; // "\x00\x00\x00\x00" (0) //	Total size of previous tag, or 0 for first tag
};

// Then a sequence of tags followed by their size until EOF.
struct FLV_TAG_HEADER {
	uint8 Type; // "\x12" (0×12, META) // 	Determines the layout of Body, see below for tag types
	uint24_be BodyLength; // 	"\x00\x00\xe0" (224) //	Size of Body (total tag size - 11)
	uint24_be Timestamp; // 	 "\x00\x00\x00" (0) //	Timestamp of tag (in milliseconds)
	uint8 TimestampExtended; // "\x00" (0) // 	Timestamp extension to form a uint32_be. This field has the upper 8 bits.
	uint24_be StreamId; // "\x00\x00\x00" // (0) 	Always 0
	// byte[BodyLength] Body; //	 	... // Dependent on the value of Type
	// FLV_PREVIOUS_TAG_SIZE .
}; 

// FLV Tag Types

#define TAG_TYPE_AUDIO 0×08 // Contains an audio packet similar to a SWF SoundStreamBlock plus codec information
#define TAG_TYPE_VIDEO 0×09 //	 	Contains a video packet similar to a SWF VideoFrame plus codec information
#define TAG_TYPE_META 0×12 // Contains two AMF packets, the name of the event and the data to go with it

// FLV Tag 0x08: AUDIO
// 
// The first byte of an audio packet contains bitflags that describe the codec used, with the following layout:

#define soundType(byte) 	((byte) & 0×01) >> 0 // 0: mono, 1: stereo
#define soundSize(byte) ((byte) & 0×02) >> 1 // 0: 8-bit, 1: 16-bit
#define soundRate(byte) 	((byte) & 0x0C) >> 2 // 	0: 5.5 kHz, 1: 11 kHz, 2: 22 kHz, 3: 44 kHz
#define soundFormat(byte) 	((byte) & 0xf0) >> 4 //	0: Uncompressed, 1: ADPCM, 2: MP3, 5: Nellymoser 8kHz mono, 6: Nellymoser

// The rest of the audio packet is simply the relevant data for that format, as per a SWF SoundStreamBlock.

// FLV Tag 0x09: VIDEO
//
// The first byte of a video packet describes contains bitflags that describe the codec used, and the type of frame

#define codecID(byte) ((byte) & 0x0f) >> 0 //	2: Sorensen H.263, 3: Screen video, 4: On2 VP6, 5: On2 VP6 Alpha, 6: ScreenVideo 2
#define frameType(byte)	((byte) & 0xf0) >> 4 //	1: keyframe, 2: inter frame, 3: disposable inter frame

// In some cases it is also useful to decode some of the body of the video packet, such as to
// acquire its resolution (if the initial onMetaData META tag is missing, for example).

// TODO: Describe the techniques for acquiring this information. Until then, you can consult the flashticle sources.

// FLV Tag 0x12: META
//
// The contents of a meta packet are two AMF packets. The first is almost always a short uint16_be length-prefixed 
// UTF-8 string (AMF type 0×02), and the second is typically a mixed array (AMF type 0×08). However, the 
// second chunk typically contains a variety of types, so a full AMF parser should be used.
