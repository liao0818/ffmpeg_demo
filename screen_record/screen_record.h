#ifndef ScreenRecord_h__
#define ScreenRecord_h__

#define MAX_FRIENDLY_NAME_LENGTH    128

#define RECORD_PC_SOUND 0x00000001L
#define RECORD_MICPHONE 0x00000010L

typedef struct _FilterContext
{
	AVFilterContext* sink;
	AVFilterGraph* graph;
}FilterContext;

typedef struct _InputStream
{
	AVCodecContext* dec_ctx;
	AVCodec* dec;
	AVStream* stream;
}InputStream;

typedef struct _OutputStream
{
	AVCodecContext* enc_ctx;
	AVCodec* enc;
	AVStream* stream;

	AVBitStreamFilterContext* bitstream_filters;

	int64_t next_pts;

}OutputStream;

typedef struct _InputFile
{
	AVFormatContext* ctx;
	char device[20];
	char filename[MAX_FRIENDLY_NAME_LENGTH];
	AVDictionary *opts;
	AVMediaType type;
	InputStream* st;
	AVFilterContext* buffersrc_ctx;
	SwsContext* sws_ctx;
}InputFile;

typedef struct _OutputFile
{
	AVFormatContext *ctx;
	AVDictionary *opts;

	OutputStream* video_st;
	OutputStream* audio_st;

}OutputFile;

#endif // ScreenRecord_h__
