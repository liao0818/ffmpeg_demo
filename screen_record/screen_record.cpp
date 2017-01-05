// ScreenRecord.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "screen_record.h"

//#define USE_FILTER
#define VIDEO_FRAME_RATE 20 /* 25 images/s */
#define VIDEO_FRAME_COUNT 500
#define SEMAPHORE_SIZE 5

InputFile *video_input = NULL, *pc_audio_input = NULL, *micphone_input = NULL;
InputFile* cur_audio_input = NULL;
int nb_audio_input = 0, nb_video_input = 0;
OutputFile* out_put = NULL;
FilterContext audio_filter, video_filter;
HANDLE hReadSemaphore = NULL, hWriteSemaphore = NULL;
AVFrame* frame_fifo[SEMAPHORE_SIZE];

const char* outfile = "e:\\video1.mp4";
bool bCap = false;

std::string utf8_to_char(char* str)
{
	wchar_t* pElementText;
	int iTextLen;
	iTextLen = MultiByteToWideChar( CP_UTF8,0,str,-1,NULL,0);
	pElementText = new wchar_t[iTextLen + 1];
	memset( ( void* )pElementText, 0, sizeof( wchar_t ) * ( iTextLen + 1 ) );
	::MultiByteToWideChar( CP_UTF8,0,str,-1,pElementText,iTextLen);

	int n = WideCharToMultiByte(CP_ACP, 0,pElementText, iTextLen, NULL, 0, NULL, NULL);
	char* p = new char[n+1];
	memset(p,0,n+1);
	WideCharToMultiByte(CP_ACP,0,pElementText,iTextLen,p,n,NULL, NULL);

	std::string strText;
	strText = p;
	delete[] pElementText;
	delete[] p;
	return strText;
}

std::string char_to_utf8(char* str)
{
	wchar_t* pElementText;
	int iTextLen;
	iTextLen = MultiByteToWideChar( CP_ACP,0,str,-1,NULL,0);
	pElementText = new wchar_t[iTextLen + 1];
	memset( ( void* )pElementText, 0, sizeof( wchar_t ) * ( iTextLen + 1 ) );
	::MultiByteToWideChar( CP_ACP,0,str,-1,pElementText,iTextLen);

	int n = WideCharToMultiByte(CP_UTF8, 0,pElementText, iTextLen, NULL, 0, NULL, NULL);
	char* p = new char[n+1];
	memset(p,0,n+1);
	WideCharToMultiByte(CP_UTF8,0,pElementText,iTextLen,p,n,NULL, NULL);

	std::string strText;
	strText = p;
	delete[] pElementText;
	delete[] p;
	return strText;
}

AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
{
	AVFrame *picture;
	int ret;

	picture = av_frame_alloc();
	if (!picture)
		return NULL;

	picture->format = pix_fmt;
	picture->width  = width;
	picture->height = height;

	/* allocate the buffers for the frame data */
	ret = av_frame_get_buffer(picture, 32);
	if (ret < 0)
	{
		fprintf(stderr, "Could not allocate frame data.\n");
		exit(1);
	}

	return picture;
}

AVFrame* alloc_audio_frame(AVSampleFormat sample_fmt, uint64_t channel_layout, int sample_rate, int nb_samples)
{
	int ret = 0;
	AVFrame* frame = av_frame_alloc();
	if(!frame)
	{
		fprintf(stderr, "Error allocating an audio frame\n");
		return NULL;
	}

	frame->format = sample_fmt;
	frame->channel_layout = channel_layout;
	frame->sample_rate = sample_rate;
	frame->nb_samples = nb_samples;

	ret = av_frame_get_buffer(frame, 0);
	if(ret < 0)
	{
		fprintf(stderr, "Error allocating an audio buffer.\n");
		exit(1);
	}

	return frame;
}

/*
int init_resampler(AVCodecContext* input_codec_context, AVCodecContext* output_codec_context)
{
	int error = 0;

	resample_ctx = swr_alloc_set_opts(NULL,
		av_get_default_channel_layout(output_codec_context->channels),
		output_codec_context->sample_fmt,
		output_codec_context->sample_rate,
		av_get_default_channel_layout(input_codec_context->channels),
		input_codec_context->sample_fmt,
		input_codec_context->sample_rate,
		0, NULL);

	if(!resample_ctx)
	{
		fprintf(stderr, "Could not allocate resample context\n");
		return -1;
	}

	if ((error = swr_init(resample_ctx)) < 0) 
	{
		fprintf(stderr, "Could not open resample context\n");
		swr_free(&resample_ctx);
		return error;
	}

	return 0;
}
*/
void get_audiodevice_name(char* dev)
{
	/*
	UINT n = waveInGetNumDevs();

	for(int i = 0; i < n; i++)
	{
		WAVEINCAPSA wic = {0};
		waveInGetDevCapsA(i,&wic,sizeof(wic));

		std::string str = char_to_utf8(wic.szPname);

		strncpy(dev, str.c_str(), MAXPNAMELEN);
	}
	*/
	/*
	AVFormatContext *pFormatCtx = NULL;
	AVDictionary* options = NULL;
	av_dict_set(&options,"list_devices","true",0);
	AVInputFormat *iformat = av_find_input_format("dshow");
	avformat_open_input(&pFormatCtx,"dummy",iformat,&options);
	
	int ret = 0;
	AVInputFormat* ifmt = av_find_input_format("dshow");
	AVDeviceInfoList* dev_list;

	ret = avdevice_list_input_sources(ifmt, NULL, NULL, &dev_list);
	if(ret < 0)
	{

	}*/
	setlocale(LC_ALL, "chs");

	HRESULT hr;
	hr = CoInitialize(NULL);
	if (FAILED(hr))
	{
		return;
	}

	ICreateDevEnum *pSysDevEnum = NULL;
	hr = CoCreateInstance( CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void **)&pSysDevEnum );
	if (FAILED(hr))
	{
		CoUninitialize();
		return;
	}

	IEnumMoniker *pEnumCat = NULL;
	hr = pSysDevEnum->CreateClassEnumerator(CLSID_AudioInputDeviceCategory, &pEnumCat, 0 );
	if (hr == S_OK)
	{
		IMoniker *pMoniker = NULL;
		ULONG cFetched;
		while(pEnumCat->Next( 1, &pMoniker, &cFetched ) == S_OK)
		{
			IPropertyBag *pPropBag;
			hr = pMoniker->BindToStorage( NULL, NULL, IID_IPropertyBag, (void **)&pPropBag );
			if (SUCCEEDED(hr))
			{
				VARIANT varName;
				VariantInit( &varName );

				hr = pPropBag->Read( L"FriendlyName", &varName, NULL );
				if (SUCCEEDED(hr))
				{
					wprintf(L"%ls\n", varName.bstrVal);

					if(wcscmp(L"virtual-audio-capturer", varName.bstrVal)!=0)
					//StringCchCopy( FriendlyName, MAX_FRIENDLY_NAME_LENGTH, varName.bstrVal );
					{
						int l = WideCharToMultiByte(CP_UTF8, 0, varName.bstrVal, -1, 0, 0, 0, 0);
						WideCharToMultiByte(CP_UTF8, 0, varName.bstrVal, -1, dev, l, 0, 0);
					}
				}

				VariantClear( &varName );
				pPropBag->Release();
			}
			pMoniker->Release();
		}
		pEnumCat->Release();
	}
	pSysDevEnum->Release();

	CoUninitialize();

	//system("pause");
}

int flush_encoder(AVFormatContext* fmt_ctx, AVStream* st)
{  
	int ret;  
	int got_frame;  
	
	if (!(st->codec->codec->capabilities &  CODEC_CAP_DELAY))  
	{
		return 0;  
	}

	AVPacket pkt;  
	av_init_packet(&pkt);

	while (1) 
	{  
		pkt.data = NULL;  
		pkt.size = 0;  

		if(st->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			ret = avcodec_encode_video2(st->codec, &pkt,  NULL, &got_frame); 
		}
		else
		{
			ret = avcodec_encode_audio2(st->codec, &pkt,  NULL, &got_frame);
		}
		av_frame_free(NULL);  
		if (ret < 0)  
		{
			break;  
		}

		if (!got_frame)
		{  
			ret=0;  
			break;  
		}

		printf("Flush Encoder: Succeed to encode 1 frame!\tsize:%5d\n",pkt.size);  
		/* mux encoded frame */ 
		av_packet_rescale_ts(&pkt, st->codec->time_base, st->time_base);

		ret = av_write_frame(fmt_ctx, &pkt);  
		if (ret < 0)  
		{
			break;  
		}
	}  

	return ret;  
}  

int add_out_stream(OutputFile* out_put, AVCodecID codec_id, AVCodecContext* input_ctx)
{
	int ret = 0;

	AVCodec* codec = avcodec_find_encoder(codec_id);
	if(codec == NULL)
	{
		return -1;
	}

	AVStream* stream = avformat_new_stream(out_put->ctx, codec);
	if(stream == NULL)
	{
		return -1;
	}

	AVCodecContext *codec_ctx = stream->codec;

	switch(codec->type)
	{
	case AVMEDIA_TYPE_VIDEO:
		{
			out_put->video_st = (OutputStream*)av_mallocz(sizeof(OutputStream));

			codec_ctx->width = input_ctx->width;
			codec_ctx->height = input_ctx->height;
			codec_ctx->sample_aspect_ratio = input_ctx->sample_aspect_ratio;
			codec_ctx->time_base = input_ctx->time_base;
			stream->time_base = av_make_q(1, 30000);
			codec_ctx->pix_fmt = codec->pix_fmts[0];

			//codec_ctx->bit_rate = 400000;
			//codec_ctx->rc_max_rate = 400000;
			//codec_ctx->rc_min_rate = 400000;
			//codec_ctx->gop_size = 20;
			//codec_ctx->max_b_frames = 2;
			//codec_ctx->profile = FF_PROFILE_H264_MAIN;
			//codec_ctx->qcompress = 0.6f;
			//codec_ctx->qmax = 30;
			//codec_ctx->qmin = 18;
			//codec_ctx->max_qdiff = 4;

			out_put->video_st->stream = stream;
			out_put->video_st->enc = codec;
			out_put->video_st->enc_ctx = codec_ctx;
		}
		break;
	case AVMEDIA_TYPE_AUDIO:
		{
			out_put->audio_st = (OutputStream*)av_mallocz(sizeof(OutputStream));

			codec_ctx->sample_fmt = codec->sample_fmts[0];
			codec_ctx->sample_rate = input_ctx->sample_rate;
			codec_ctx->channels = 2;
			codec_ctx->channel_layout = av_get_default_channel_layout(codec_ctx->channels);
			if(codec_ctx->channel_layout == 0)
			{
				codec_ctx->channel_layout = AV_CH_LAYOUT_STEREO;
				codec_ctx->channels = av_get_channel_layout_nb_channels(codec_ctx->channel_layout);
			}
			stream->time_base = av_make_q(1, codec_ctx->sample_rate);
			codec_ctx->profile = FF_PROFILE_AAC_LOW;
			codec_ctx->codec_tag = 0;

			out_put->audio_st->stream = stream;
			out_put->audio_st->enc = codec;
			out_put->audio_st->enc_ctx = codec_ctx;
		}
		break;
	default:
		return NULL;
	}

	AVDictionary* opt = NULL;
	if(codec_id == AV_CODEC_ID_H264)
	{
		//av_dict_set(&opt, "crf", "20", 0); //0-51
		//av_dict_set(&opt, "preset", "slow", 0);
		av_dict_set(&opt, "tune", "zerolatency", 0);
	}

	ret = avcodec_open2(codec_ctx, codec, &opt);
	if ( ret < 0)
	{
		fprintf(stderr, "Could not open codec\n");
		return NULL;
	}

	if(out_put->ctx->oformat->flags & AVFMT_GLOBALHEADER)
	{
		codec_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}

	return ret;
}

int open_output_file(const char* filename)
{
	int ret = -1;
	AVOutputFormat* ofmt = NULL;
	out_put = (OutputFile*)av_mallocz(sizeof(OutputFile));

	ret = avformat_alloc_output_context2(&out_put->ctx, NULL, NULL, filename);
	if(ret < 0 || out_put->ctx == 0)
	{
		return -1;
	}

	ofmt = out_put->ctx->oformat;

	if(ofmt->video_codec != AV_CODEC_ID_NONE)
	{
		ret = add_out_stream(out_put, ofmt->video_codec, video_input->st->dec_ctx);
		if(ret < 0)
		{
			return -1;
		}
	}

	if(ofmt->audio_codec != AV_CODEC_ID_NONE)
	{
		ret = add_out_stream(out_put, ofmt->audio_codec, pc_audio_input->st->dec_ctx);
		if(ret < 0)
		{
			return -1;
		}
	}

	out_put->audio_st->bitstream_filters = av_bitstream_filter_init("aac_adtstoasc");

	av_dump_format(out_put->ctx, 0, filename, 1);

	if (!(ofmt->flags & AVFMT_NOFILE))
	{
		ret = avio_open(&out_put->ctx->pb, filename, AVIO_FLAG_WRITE);
		if(ret < 0 )
		{
			return -1;
		}
	}

	ret = avformat_write_header(out_put->ctx, NULL);
	if(ret < 0 )
	{
		return -1;
	}

	return 0;
}

int write_video_frame(AVFormatContext* ctx, OutputStream* ost, AVFrame* frame)
{
	int got_picture = 0;
	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	if(frame != NULL)
	{
		frame->pts = ost->next_pts++;
	}

	int ret = avcodec_encode_video2(ost->enc_ctx, &pkt, frame, &got_picture);
	if(ret < 0)
	{
		return -1;
	}

	if (!got_picture)
	{
		printf("encode video frame can't get one frame\n");
		av_packet_unref(&pkt);
		return 0;
	}

	pkt.stream_index = ost->stream->index;
	av_packet_rescale_ts(&pkt, ost->enc_ctx->time_base, ost->stream->time_base);

	ret = av_interleaved_write_frame(ctx, &pkt);

	av_packet_unref(&pkt);

	return ret;
}

int write_audio_frame(AVFormatContext* ctx, OutputStream* ost, AVFrame* frame)
{
	int ret = -1;
	int got_picture = 0;
	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	if(frame)
	{
		frame->pts = ost->next_pts;
		ost->next_pts += frame->nb_samples;
	}

	ret = avcodec_encode_audio2(ost->enc_ctx, &pkt, frame, &got_picture);
	if(ret < 0)
	{
		return -1;
	}

	if(!got_picture)
	{
		return 1;
	}

	av_bitstream_filter_filter(ost->bitstream_filters, ost->enc_ctx, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);

	pkt.stream_index = ost->stream->index;
	av_packet_rescale_ts(&pkt, ost->enc_ctx->time_base, ost->stream->time_base);
	ret = av_interleaved_write_frame(ctx, &pkt);

	av_packet_unref(&pkt);

	return ret;
}

unsigned int __stdcall captrue_video(void* param)
{
	InputFile* infile = (InputFile*)param;

	int got_picture = 0;
	int ret = -1, index = 0;
	int cur_write = 0;
	AVPacket packet;
	av_init_packet(&packet);
	AVFrame *frame = av_frame_alloc();
	AVCodecContext* v_codec_ctx = infile->st->dec_ctx;
	DWORD dwRet = 0;

	int64_t ta = 0, tb = 0, tc = 0, td = 0;

	while(bCap)
	{
#ifndef USE_FILTER
		dwRet = WaitForSingleObject(hWriteSemaphore, 1000 / VIDEO_FRAME_RATE);
		if (dwRet != WAIT_OBJECT_0)
		{
			printf("can't write frame fito\n");
			continue;
		}
#endif
		ta = av_gettime();
		packet.data = NULL;
		packet.size = 0;

		ret = av_read_frame(infile->ctx, &packet);
		if (ret < 0)
		{
			printf("read video frame failed\n");
			break;
		}

		tb = av_gettime();
		tc = tb-ta;
		td+=tc;
		printf("capture one video frame time -> %ld, ", tc);
		ta = tb;

		if(packet.stream_index == infile->st->stream->index)
		{
			ret = avcodec_decode_video2(v_codec_ctx, frame, &got_picture, &packet);

			if (ret < 0)
			{
				printf("decode video failed\n");
				break;
			}

			if(!got_picture)
			{
				printf("decode video can't get frame, continue...\n");
				continue;
			}

			tb = av_gettime();
			printf("decode time -> %ld, ", tb-ta);
			ta = tb;

#ifdef USE_FILTER

			//frame->pts = av_frame_get_best_effort_timestamp(frame);
			ret = av_buffersrc_add_frame_flags(infile->buffersrc_ctx, frame, 0);
			if (ret < 0)
			{
				break;
			}

			tb = av_gettime();
			printf("add buffer time -> %ld\n", tb-ta);
			ta = tb;
#else
			ret = sws_scale(infile->sws_ctx, (const uint8_t* const*)frame->data, frame->linesize, 0, v_codec_ctx->height, frame_fifo[cur_write]->data, frame_fifo[cur_write]->linesize);

			tb = av_gettime();
			printf("transcode time -> %ld\n", tb - ta);
			ta = tb;

			cur_write++;
			cur_write %= SEMAPHORE_SIZE;
			ReleaseSemaphore(hReadSemaphore, 1, NULL);
#endif
			index++;

		}

		av_packet_unref(&packet);
	}
	
	av_frame_free(&frame);

	avformat_close_input(&infile->ctx);

	printf("average video frame capture time -> %.4f ms\n", td/((float)index*1000));
	printf("stop capture video!\n");

	return 0;
}

unsigned int __stdcall captrue_audio(void* param)
{
	InputFile* infile = (InputFile*)param;

	AVCodecContext* codec_ctx = infile->st->dec_ctx;
	int ret = -1;
	int got_picture = 0;
	AVPacket packet;
	AVFrame *frame = av_frame_alloc();
	av_init_packet(&packet);

	while(bCap)
	{
		packet.data = NULL;
		packet.size = 0;

		ret = av_read_frame(infile->ctx, &packet);
		if (ret < 0)
		{
			printf("read audio frame failed\n");
			break;
		}

		if(packet.stream_index == infile->st->stream->index)
		{
			ret = avcodec_decode_audio4(codec_ctx, frame, &got_picture, &packet);

			if(ret < 0)
			{
				printf("decode audio frame failed\n");
				break;
			}

			if (!got_picture)
			{
				continue;
			}
			
			ret = av_buffersrc_add_frame_flags(infile->buffersrc_ctx, frame, 0);
			if (ret < 0)
			{
				printf("av_buffersrc failed\n");
				break;
			}

		}

		av_packet_unref(&packet);
	}

	av_frame_free(&frame);

	avformat_close_input(&infile->ctx);

	printf("stop record %s!\n", infile->filename);
	
	return ret;
}

int init_filter(InputFile* input_file, AVFilterInOut *inputs)
{
	int ret = -1;
	char args[512] = {0};
	AVCodecContext* code_ctx = input_file->st->dec_ctx;

	AVFilter* abuffersrc = avfilter_get_by_name("abuffer");

	if (!code_ctx->channel_layout)
	{
		code_ctx->channel_layout = av_get_default_channel_layout(code_ctx->channels);
	}

	_snprintf_s(args, sizeof(args), "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%I64x",
		code_ctx->time_base.num, code_ctx->time_base.den, code_ctx->sample_rate,
		av_get_sample_fmt_name(code_ctx->sample_fmt), code_ctx->channel_layout);

	ret = avfilter_graph_create_filter(&input_file->buffersrc_ctx, abuffersrc, input_file->filename, args, NULL, audio_filter.graph);
	if(ret < 0)
		return-1;

	//static int idx = 0;
	//char* str = (char*)av_mallocz(MAX_FRIENDLY_NAME_LENGTH);
	//memcpy(str,input_file->filename,MAX_FRIENDLY_NAME_LENGTH);
	////(*inputs) = avfilter_inout_alloc();
	//(*inputs)->name = str;
	//(*inputs)->filter_ctx = input_file->buffersrc_ctx;
	//(*inputs)->pad_idx = idx++;
	//(*inputs)->next = NULL;

	if ((ret = avfilter_link(input_file->buffersrc_ctx, 0, inputs->filter_ctx, inputs->pad_idx)) < 0)
		return ret;

	return 0;
}

int init_audio_filters(int nb_inputs)
{
	int ret = -1;

	AVCodecContext* enc_ctx = out_put->audio_st->enc_ctx;

	AVFilterInOut *inputs = NULL, *outputs = NULL;
	audio_filter.graph = avfilter_graph_alloc();
	//inputs = avfilter_inout_alloc();

	if(!audio_filter.graph)
	{
		return -1;
	}

	char filter_descr[512]={0};
	sprintf_s(filter_descr, "amix=inputs=%d,asetnsamples=n=%d", nb_inputs, enc_ctx->frame_size);

	if ((ret = avfilter_graph_parse2(audio_filter.graph, filter_descr, &inputs, &outputs)) < 0)
		return ret;

	if(nb_inputs > 1)
	{
		init_filter(pc_audio_input, inputs);
		init_filter(micphone_input, inputs->next);
	}
	else
	{
		init_filter(cur_audio_input, outputs);
	}
	
	AVFilter *abuffersink = avfilter_get_by_name("abuffersink");
	ret = avfilter_graph_create_filter(&audio_filter.sink, abuffersink, "out", NULL, NULL, audio_filter.graph);
	if (ret < 0)
	{
		return -1;
	}

	ret = av_opt_set_bin(audio_filter.sink, "sample_fmts",
		(uint8_t*)&enc_ctx->sample_fmt, sizeof(enc_ctx->sample_fmt),
		AV_OPT_SEARCH_CHILDREN);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot set output sample format\n");
		goto end;
	}

	ret = av_opt_set_bin(audio_filter.sink, "channel_layouts",
		(uint8_t*)&enc_ctx->channel_layout,
		sizeof(enc_ctx->channel_layout), AV_OPT_SEARCH_CHILDREN);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot set output channel layout\n");
		goto end;
	}

	ret = av_opt_set_bin(audio_filter.sink, "sample_rates",
		(uint8_t*)&enc_ctx->sample_rate, sizeof(enc_ctx->sample_rate),
		AV_OPT_SEARCH_CHILDREN);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot set output sample rate\n");
		goto end;
	}

	/*
	char args[512] = {0};
	AVFilterContext *format;
	//sprintf_s(args, sizeof(args), "sample_fmts=s16:sample_rates=%d:channel_layouts=0x3", 44100);
	_snprintf_s(args, sizeof(args), "sample_rates=%d:sample_fmts=%s:channel_layouts=0x%I64x",
		 enc_ctx->sample_rate,
		av_get_sample_fmt_name(enc_ctx->sample_fmt), enc_ctx->channel_layout);

	ret = avfilter_graph_create_filter(&format,	avfilter_get_by_name("aformat"),"out audio stream format", args, NULL, audio_filter.graph);
	if (ret < 0)
		return ret;
		*/

	/*inputs->name       = av_strdup("out");
	inputs->filter_ctx = audio_filter.sink;
	inputs->pad_idx    = 0;
	inputs->next       = NULL;

	if ((ret = avfilter_graph_parse_ptr(audio_filter.graph, "anull", &inputs, &outputs, NULL)) < 0)
	{
		goto end;
	}*/

	if (ret = avfilter_link(outputs->filter_ctx, outputs->pad_idx, audio_filter.sink, 0) < 0)
		goto end;

	if ((ret = avfilter_graph_config(audio_filter.graph, NULL)) < 0)
	{
		goto end;
	}

	//char* temp = avfilter_graph_dump(audio_filter.graph, NULL);
	//printf("%s\n", temp);

end:
	avfilter_inout_free(&inputs);
	avfilter_inout_free(&outputs);

	return ret;
}

int init_video_filters(AVCodecContext* dec_ctx, AVCodecContext* enc_ctx)
{
	int ret = 0;

#ifdef USE_FILTER
	char args[512];
	AVFilter *buffersrc  = avfilter_get_by_name("buffer");
	AVFilter *buffersink = avfilter_get_by_name("buffersink");
	AVFilterInOut *outputs = avfilter_inout_alloc();
	AVFilterInOut *inputs  = avfilter_inout_alloc();

	video_filter.graph = avfilter_graph_alloc();
	if (!outputs || !inputs || !video_filter.graph) {
		ret = AVERROR(ENOMEM);
		goto end;
	}

	_snprintf_s(args, sizeof(args),
            "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
            dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
            dec_ctx->time_base.num, dec_ctx->time_base.den,
            dec_ctx->sample_aspect_ratio.num, dec_ctx->sample_aspect_ratio.den);

    ret = avfilter_graph_create_filter(&video_input->buffersrc_ctx, buffersrc, "in", args, NULL, video_filter.graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
        goto end;
    }

    ret = avfilter_graph_create_filter(&video_filter.sink, buffersink, "out", NULL, NULL, video_filter.graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
        goto end;
    }

    ret = av_opt_set_bin(video_filter.sink, "pix_fmts", (const uint8_t*)&enc_ctx->pix_fmt, sizeof(enc_ctx->pix_fmt), AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
        goto end;
    }

    outputs->name       = av_strdup("in");
    outputs->filter_ctx = video_input->buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    inputs->name       = av_strdup("out");
    inputs->filter_ctx = video_filter.sink;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    if ((ret = avfilter_graph_parse_ptr(video_filter.graph, "null", &inputs, &outputs, NULL)) < 0)
        goto end;

    if ((ret = avfilter_graph_config(video_filter.graph, NULL)) < 0)
        goto end;

end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);


#else
	video_input->sws_ctx = sws_getContext(dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt, enc_ctx->width, enc_ctx->height, enc_ctx->pix_fmt, SWS_BICUBIC, NULL, NULL, NULL);
#endif

    return ret;
}

int init_filters()
{
	int ret = 0;

	if(nb_video_input==0 && nb_audio_input == 0)
	{
		printf("no input stream opened, exit\n");
		return -1;
	}

	if(nb_video_input > 0)
	{
		ret = init_video_filters(video_input->st->dec_ctx, out_put->video_st->enc_ctx);
	}

	if(nb_audio_input > 0)
	{
		ret = init_audio_filters(nb_audio_input);
	}

	return ret;
}

int open_input_file(InputFile* input_file)
{
	int ret = 0;
	input_file->st = (InputStream*)av_mallocz(sizeof(InputStream));

	AVInputFormat* ifmt = av_find_input_format(input_file->device);
	ret = avformat_open_input(&input_file->ctx, input_file->filename, ifmt, &input_file->opts);
	if(ret < 0)
	{
		printf("Couldn't open input stream.\n");
		return -1;
	}

	ret = avformat_find_stream_info(input_file->ctx, NULL);
	if(ret < 0)
	{
		fprintf(stderr, "Cannot find stream information.\n");
		return -1;
	}

	ret = av_find_best_stream(input_file->ctx, input_file->type, -1, -1, &input_file->st->dec, 0);

	if(ret < 0)
	{
		fprintf(stderr, "Cannot find audio stream.\n");
		return -1;
	}

	input_file->st->stream = input_file->ctx->streams[ret];
	input_file->st->dec_ctx = input_file->ctx->streams[ret]->codec;

	ret = avcodec_open2(input_file->st->dec_ctx, input_file->st->dec, NULL);
	if(ret < 0)
	{
		fprintf(stderr, "Could not open codec\n");  
		return -1;
	}

	return 0;
}

void open_input_files()
{
	int ret = 0;

	if(micphone_input != NULL)
	{
		ret = open_input_file(micphone_input);
		if(ret < 0)
		{
			printf("can't open micphone input\n");
		}
		else
		{
			cur_audio_input = micphone_input;
			nb_audio_input++;
		}
	}

	if(pc_audio_input != NULL)
	{
		ret = open_input_file(pc_audio_input);
		if(ret < 0)
		{
			printf("can't open pc_audio input\n");
		}
		else
		{
			cur_audio_input = pc_audio_input;
			nb_audio_input++;
		}
	}

	if(video_input != NULL)
	{
		ret = open_input_file(video_input);
		if(ret < 0)
		{
			printf("can't open video input\n");
		}
		else
		{
			nb_video_input++;
		}
	}
}

int init_audio_input(UINT type)
{
	if(type & RECORD_MICPHONE)
	{
		micphone_input = (InputFile*)av_mallocz(sizeof(InputFile));
		sprintf_s(micphone_input->device, "%s", "dshow");
		micphone_input->type = AVMEDIA_TYPE_AUDIO;

		char audio_device[MAX_FRIENDLY_NAME_LENGTH]={0};
		get_audiodevice_name(audio_device);

		sprintf_s(micphone_input->filename, "audio=%s", audio_device);
	}

	if(type & RECORD_PC_SOUND)
	{
		pc_audio_input = (InputFile*)av_mallocz(sizeof(InputFile));
		sprintf_s(pc_audio_input->device, "%s", "dshow");
		pc_audio_input->type = AVMEDIA_TYPE_AUDIO;
		sprintf_s(pc_audio_input->filename, "audio=%s", "virtual-audio-capturer");
	}

	return 0;
}

int init_video_input(int x, int y, int w, int h, int fps)
{
	video_input = (InputFile*)av_mallocz(sizeof(InputFile));
	char temp[50] = {0};
	sprintf_s(temp, "%d", fps);
	av_dict_set(&video_input->opts, "framerate", temp, 0);
	sprintf_s(temp, "%d", x);
	av_dict_set(&video_input->opts,"offset_x", temp, 0);
	sprintf_s(temp, "%d", y);
	av_dict_set(&video_input->opts,"offset_y", temp, 0);
	sprintf_s(temp, "%d*%d", w, h);
	av_dict_set(&video_input->opts,"video_size",temp,0);
	sprintf_s(video_input->device, "%s", "gdigrab");
	sprintf_s(video_input->filename, "%s", "desktop");
	video_input->type = AVMEDIA_TYPE_VIDEO;

	return 0;
}

int start_capture()
{
#ifndef USE_FILTER
	hReadSemaphore = CreateSemaphore(NULL, 0, SEMAPHORE_SIZE, NULL);
	hWriteSemaphore = CreateSemaphore(NULL, SEMAPHORE_SIZE, SEMAPHORE_SIZE, NULL);
	for (int i = 0; i<SEMAPHORE_SIZE; i++)
	{
		frame_fifo[i] = alloc_picture(out_put->video_st->enc_ctx->pix_fmt, out_put->video_st->enc_ctx->width, out_put->video_st->enc_ctx->height);
	}
#endif

	bCap = true;

	if(nb_video_input > 0 && video_input)
	{
		_beginthreadex(NULL, 0, captrue_video, video_input, 0, NULL);
	}

	if(nb_audio_input > 0)
	{
		if(pc_audio_input)
		{
			_beginthreadex(NULL, 0, captrue_audio, pc_audio_input, 0, NULL);
		}

		if(micphone_input)
		{
			_beginthreadex(NULL, 0, captrue_audio, micphone_input, 0, NULL);
		}
	}

	int index = 0;
	int ret = 0;
	int cur_read = 0;
	DWORD dwRet = 0;
	AVFrame* frame = NULL;

	int64_t ta = 0, tb = 0, tc = 0, td = 0;

	while(index<VIDEO_FRAME_COUNT)
	{
		frame = av_frame_alloc();

		if(av_compare_ts(out_put->video_st->next_pts, out_put->video_st->enc_ctx->time_base, out_put->audio_st->next_pts, out_put->audio_st->enc_ctx->time_base) <= 0)
		{
			ta = av_gettime();
#ifdef USE_FILTER
			do 
			{
				ret = av_buffersink_get_frame(video_filter.sink, frame);

			} while (ret == AVERROR(EAGAIN));

			if (ret < 0)
			{
				break;
			}

			if(write_video_frame(out_put->ctx, out_put->video_st, frame) < 0)
			{
				printf("Could not write video frame\n");
				break;
			}

			index++;
			tb = av_gettime();
			tc = tb-ta;
			td+=tc;
			printf("write video frame -> %d, time -> %ld\n", index, tc);
#else
			dwRet = WaitForSingleObject(hReadSemaphore, 1000 / VIDEO_FRAME_RATE);
			if (dwRet == WAIT_OBJECT_0)
			{

				if (write_video_frame(out_put->ctx, out_put->video_st, frame_fifo[cur_read]) < 0)
				{
					printf("Could not write video frame\n");
					break;
				}

				index++;
				tb = av_gettime();
				tc = tb - ta;
				td += tc;
				printf("write video frame -> %d, current index = %d, time -> %ld\n", index, cur_read, tc);
				ta = tb;

				cur_read++;
				cur_read %= SEMAPHORE_SIZE;
				ReleaseSemaphore(hWriteSemaphore, 1, NULL);
			}
			else
			{
				printf("wait for one frame failed\n");
			}
#endif
		}
		else
		{
			do 
			{
				ret = av_buffersink_get_frame(audio_filter.sink, frame);

			} while (ret == AVERROR(EAGAIN));

			if (ret < 0)
			{
				break;
			}

			if(write_audio_frame(out_put->ctx, out_put->audio_st, frame) < 0)
			{
				printf("Could not write audio frame\n");
				break;
			}
		}

		av_frame_free(&frame);
	}


	printf("average frame write time -> %.4f ms\n", td/((float)index*1000));

	return 0;
}

int pause_capture()
{
	bCap = false;
	Sleep(1000);
	return 0;
}

int stop_capture()
{
	bCap = false;
	Sleep(1000);

	flush_encoder(out_put->ctx, out_put->video_st->stream);
	//flush_encoder(out_put->ctx, out_put->audio_st->stream);

	av_write_trailer(out_put->ctx);

	if (out_put->ctx && !(out_put->ctx->oformat->flags & AVFMT_NOFILE))
	{
		avio_closep(&out_put->ctx->pb);
	}

	if(video_filter.graph)
	avfilter_graph_free(&video_filter.graph);
	if(audio_filter.graph)
	avfilter_graph_free(&audio_filter.graph);

	avformat_free_context(out_put->ctx);
	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
	av_register_all();
	avdevice_register_all();
	avfilter_register_all();

	init_video_input(0,0,1366,768,VIDEO_FRAME_RATE);
	init_audio_input(RECORD_PC_SOUND|RECORD_MICPHONE);

	open_input_files();
	open_output_file(outfile);

	if(init_filters() < 0)
	{
		return 0;
	}

	start_capture();

	stop_capture();
	
	printf("finish\n");

	system("pause");

	return 0;
}

