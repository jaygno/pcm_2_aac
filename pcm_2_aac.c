#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <libavcodec/avcodec.h>

#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include <libavutil/frame.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>


static int write_aac_header(FILE* fp, AVPacket* pkt);

/* check that a given sample format is supported by the encoder */
static int check_sample_fmt(const AVCodec *codec, enum AVSampleFormat sample_fmt)
{
    const enum AVSampleFormat *p = codec->sample_fmts;

    while (*p != AV_SAMPLE_FMT_NONE) {
        if (*p == sample_fmt)
            return 1;
        p++;
    }
    return 0;
}

static void encode(AVCodecContext *ctx, AVFrame *frame, AVPacket *pkt,
                   FILE *output)
{
    int ret;

    /* send the frame for encoding */
    ret = avcodec_send_frame(ctx, frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending the frame to the encoder\n");
        exit(1);
    }
    
    fprintf(stdout, "Sending the frame to the encoder\n");

    /* read all the available output packets (in general there may be any
     * number of them */
    while (ret >= 0) {
        ret = avcodec_receive_packet(ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            fprintf(stderr, "Error encoding audio frame\n");
            exit(1);
        }

        write_aac_header(output, pkt);
        fwrite(pkt->data, 1, pkt->size, output);
        fprintf(stdout, "write %d\n", pkt->size);
        av_packet_unref(pkt);
    }

    return;
}


//AAC有两种封装格式，分别是ADIF ADTS，多与流媒体一般使用ADTS格式。见：
//http://www.jianshu.com/p/839b11e0638b aac freqIdx

char aac_adts_header[7] = {0};
int chanCfg = 1;            //MPEG-4 Audio Channel Configuration. 1 Channel front-center  

static int init_aac_header() {
    int profile = 2;   //AAC LC
    int freqIdx = 11;   //8000HZ

    aac_adts_header[0] = (char)0xFF;      // 11111111     = syncword  
    aac_adts_header[1] = (char)0xF1;      // 1111 1 00 1  = syncword MPEG-2 Layer CRC  
    aac_adts_header[2] = (char)(((profile - 1) << 6) + (freqIdx << 2) + (chanCfg >> 2));  
    aac_adts_header[6] = (char)0xFC;  

    return 0;
}

static int write_aac_header(FILE* fp, AVPacket* pkt) {
    aac_adts_header[3] = (char)(((chanCfg & 3) << 6) + ((7 + pkt->size) >> 11));  
    aac_adts_header[4] = (char)(((7 + pkt->size) & 0x7FF) >> 3);  
    aac_adts_header[5] = (char)((((7 + pkt->size) & 7) << 5) + 0x1F);  

    fwrite(aac_adts_header, 7, 1, fp);  

    return 0;
}

int main(int argc, char **argv){
    AVFrame *frame;
    AVCodec *codec = NULL;
    AVPacket *pkt;
    AVCodecContext *codecContext;
    int readSize=0;
    FILE * fileIn,*fileOut;
    int frameCount=0;
    /* register all the codecs */
    av_register_all();

    init_aac_header();

    if(argc!=3){
        fprintf(stdout,"usage:./a.out xxx.pcm xxx.aac\n");
        return -1;
    }

    fileIn =fopen(argv[1],"r+");
    if (fileIn == NULL) {
        fprintf(stderr, "fopen failed %s\n", argv[1]);
        exit(1);
    }
    
    //3.读出来的数据，我们需要编码，因此需要编码器
    //下面的函数找到h.264类型的编码器
    /* find the mpeg1 video encoder */
    codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!codec){
        fprintf(stderr, "Codec not found\n");
        exit(1);
    }

    //有了编码器，我们还需要编码器的上下文环境，用来控制编码的过程
    codecContext = avcodec_alloc_context3(codec);//分配AVCodecContext实例
    if (!codecContext){
        fprintf(stderr, "Could not allocate video codec context\n");
        return -1;
    }

    /* put sample parameters */  
    codecContext->bit_rate = 64000;

    /* check that the encoder supports s16 pcm input */
    codecContext->sample_fmt = AV_SAMPLE_FMT_S16;
    if (!check_sample_fmt(codec, codecContext->sample_fmt)) {
        fprintf(stderr, "Encoder does not support sample format %s",
                av_get_sample_fmt_name(codecContext->sample_fmt));
        exit(1);
    }

    codecContext->sample_rate = 8000;  
    codecContext->channel_layout = AV_CH_LAYOUT_MONO;
    codecContext->channels = av_get_channel_layout_nb_channels(codecContext->channel_layout);  

    /* select other audio parameters supported by the encoder */  
    //准备好了编码器和编码器上下文环境，现在可以打开编码器了
    //根据编码器上下文打开编码器
    if (avcodec_open2(codecContext, codec, NULL) < 0){
        fprintf(stderr, "Could not open codec\n");
        return -1;
    }

    /* packet for holding encoded output */
    pkt = av_packet_alloc();
    if (!pkt) {
        fprintf(stderr, "could not allocate the packet\n");
        exit(1);
    }

    //读出的一帧数据保存在AVFrame中。
    frame  = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate audio frame\n");
        exit(1);
    }

    frame->nb_samples = codecContext->frame_size;;
    frame->format = codecContext->sample_fmt;
    frame->channel_layout = codecContext->channel_layout;

    /* allocate the data buffers */
    int ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate audio data buffers\n");
        exit(1);
    }

    //4.准备输出文件
    fileOut= fopen(argv[2],"w+");
    //下面开始编码
    while(1){
        //读一帧数据出来
        //frame->pts = 1;
        readSize = fread(frame->data[0], 1,1024*2,fileIn);
        if(readSize == 0){
            fprintf(stdout,"end of file\n");
            frameCount++;
            break;
        }

        encode(codecContext, frame, pkt, fileOut);
    }

    //flush
    encode(codecContext, NULL, pkt, fileOut);

    fclose(fileIn);
    fclose(fileOut);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&codecContext);
    return 0;
}
