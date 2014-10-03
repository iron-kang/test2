#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <jpeglib.h>
#include <stdio.h>
#include <sys/stat.h>

void saveAsJpeg(AVFrame *pFrameRGB, int width, int height, int framenum)
{
      char fname[128];
      // AVPicture my_pic ;
      struct jpeg_compress_struct cinfo;
      struct jpeg_error_mgr jerr;
      JSAMPROW row_pointer[1];
      int row_stride;
      uint8_t *buffer;
      FILE *fp = NULL;

      buffer = pFrameRGB->data[0];

      //int size = sizeof(buffer);

      cinfo.err = jpeg_std_error(&jerr);
      jpeg_create_compress(&cinfo);

      //_snprintf(fname, sizeof(fname), "frames%d.jpg", framenum);
      sprintf(fname, "./frame/frames%d.jpg", framenum);
      fp = fopen(fname, "wb");

      if (fp == NULL)
            return;
      jpeg_stdio_dest(&cinfo, fp);

      cinfo.image_width = width;
      cinfo.image_height = height;
      cinfo.input_components = 3;
      cinfo.in_color_space = JCS_RGB;

      jpeg_set_defaults(&cinfo);
      jpeg_set_quality(&cinfo, 80, TRUE);
      jpeg_start_compress(&cinfo, TRUE);

      row_stride = width * 3;

      while (cinfo.next_scanline < height)
      {
            /* jpeg_write_scanlines expects an array of pointers to scanlines.
            * Here the array is only one element long, but you could pass
            * more than one scanline at a time if that's more convenient.
            */
            row_pointer[0] = &buffer[cinfo.next_scanline * row_stride];
            jpeg_write_scanlines(&cinfo, row_pointer, 1);
      }

      jpeg_finish_compress(&cinfo);
      fclose(fp);
      jpeg_destroy_compress(&cinfo);
      printf("compress %d frame finished!\n",framenum) ;
      return ;

}

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
  FILE *pFile;
  char szFilename[32];
  int  y;
  
  // Open file
  sprintf(szFilename, "frame%d.jpg", iFrame);
  pFile=fopen(szFilename, "wb");
  if(pFile==NULL)
    return;
  
  // Write header
  fprintf(pFile, "P6\n%d %d\n255\n", width, height);
  
  // Write pixel data
  for(y=0; y<height; y++)
    fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, width*3, pFile);
  
  // Close file
  fclose(pFile);
}

int main(int argc, char *argv[]) {
  AVFormatContext *pFormatCtx = NULL;
  int             i, videoStream;
  AVCodecContext  *pCodecCtx = NULL;
  AVCodec         *pCodec = NULL;
  AVFrame         *pFrame = NULL; 
  AVFrame         *pFrameRGB = NULL;
  AVPacket        packet;
  int             frameFinished;
  int             numBytes;
  uint8_t         *buffer = NULL;
  int 		  frame_num = 0;
  int64_t	  duration = 0;
  struct stat     st = {0};	 

  AVDictionary    *optionsDict = NULL;
  struct SwsContext      *sws_ctx = NULL;
 
  if (stat("./frame", &st) == -1) {
    mkdir("./frame", 0777);
  }
  else
  	system("rm ./frame/*.*");
  if(argc < 2) {
    printf("Please provide a movie file\n");
    return -1;
  }
  // Register all formats and codecs
  //Initializes libavformat and registers all the muxers, demuxers and protocols
  av_register_all();
  
  // Open video file
  //使用 avformat_open_input 打開檔案，並建立 AVFormatContext 以供後續 decode 使用。這裡的filename也可以直接使用RTSP URL，播放串流。
  if(avformat_open_input(&pFormatCtx, argv[1], NULL, NULL)!=0)
    return -1; // Couldn't open file
  
  // Retrieve stream information
  //判斷剛剛打開的檔案內是否帶有 stream 相關資訊，若有的話，會將相關資訊存放在 AVFormatContext 資料結構內，並且返回非零值
  if(avformat_find_stream_info(pFormatCtx, NULL)<0)
    return -1; // Couldn't find stream information
  
  // Dump information about file onto standard error
  av_dump_format(pFormatCtx, 0, argv[1], 0);
  
  // Find the first video stream
  videoStream=-1;
  for(i=0; i<pFormatCtx->nb_streams; i++)
    if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
      videoStream=i;
      break;
    }
  if(videoStream==-1)
    return -1; // Didn't find a video stream
  
  // Get a pointer to the codec context for the video stream
  pCodecCtx=pFormatCtx->streams[videoStream]->codec;
  duration = pFormatCtx->duration;
  frame_num = pFormatCtx->streams[videoStream]->nb_frames;
  printf("duration: %"PRIu64"\n", duration);
  printf("frame num: %d\n", frame_num); 
  // Find the decoder for the video stream
  // 根據 codec id 找到對應的 codec
  //pCodecCtx->codec_id = CODEC_ID_MJPEG;
  pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
  if(pCodec==NULL) {
    fprintf(stderr, "Unsupported codec!\n");
    return -1; // Codec not found
  }
  // Open codec
  if(avcodec_open2(pCodecCtx, pCodec, &optionsDict)<0)
    return -1; // Could not open codec
  
  // Allocate video frame
  // 配置一個 AVFrame，準備用來放置待會解開的影像資料
  pFrame=av_frame_alloc();
  
  // Allocate an AVFrame structure
  pFrameRGB=av_frame_alloc();
  if(pFrameRGB==NULL)
    return -1;
  
  
  // Determine required buffer size and allocate buffer
  numBytes=avpicture_get_size(PIX_FMT_RGB24, pCodecCtx->width,
			      pCodecCtx->height);
  buffer=(uint8_t *)av_malloc(numBytes*sizeof(uint8_t));

  sws_ctx =
    sws_getContext
    (
        pCodecCtx->width,
        pCodecCtx->height,
        pCodecCtx->pix_fmt,
        pCodecCtx->width,
        pCodecCtx->height,
        PIX_FMT_RGB24,
        SWS_BILINEAR,
        NULL,
        NULL,
        NULL
    );
  
  // Assign appropriate parts of buffer to image planes in pFrameRGB
  // Note that pFrameRGB is an AVFrame, but AVFrame is a superset
  // of AVPicture
  avpicture_fill((AVPicture *)pFrameRGB, buffer, PIX_FMT_RGB24,
		 pCodecCtx->width, pCodecCtx->height);
  
  // Read frames and save first five frames to disk
  i=0;
  while(av_read_frame(pFormatCtx, &packet)>=0) {
    // Is this a packet from the video stream?
    // 雖然函數名稱是 read_frame, 但每次呼叫時讀出的卻是 AVPacket，原文說明如下：
    //For video, the packet contains exactly one frame. For audio, it contains an integer number of frames if each frame has a known fixed size (e.g. PCM or ADPCM data). If the audio frames have a variable size (e.g. MPEG audio), then it contains one frame.
    if(packet.stream_index==videoStream) {
      // Decode video frame
      // 將 AVPacket 丟給 ffmpeg 進行解碼，當 *frameFinished == 1，表示已經解成一個 frame，並且將此 frame 放在先前已配置好的 AVFrame。後續便可以開始畫圖的動作。
      avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, 
			   &packet);
      
      // Did we get a video frame?
      if(frameFinished) {
	// Convert the image from its native format to RGB
        // 若圖片需要改變尺寸，或做YUV->RGB的轉換，便需要呼叫 sws_scale()，若需要用到 YUV->RGB 轉換，可能會造成效能低落，此時可以考慮改成使用 OPENGL ES 的 shader 加速此過程
        sws_scale
        (
            sws_ctx,
            (uint8_t const * const *)pFrame->data,
            pFrame->linesize,
            0,
            pCodecCtx->height,
            pFrameRGB->data,
            pFrameRGB->linesize
        );
	//WriteJPEG(pCodecCtx, pFrame, 30);
	// Save the frame to disk
	if(++i<=frame_num)
	  saveAsJpeg(pFrameRGB, pCodecCtx->width, pCodecCtx->height, i);
	  //SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, i);
      }
    }
    
    // Free the packet that was allocated by av_read_frame
    av_free_packet(&packet);
  }
  
  // Free the RGB image
  av_free(buffer);
  av_free(pFrameRGB);
  
  // Free the YUV frame
  av_free(pFrame);
  
  // Close the codec
  avcodec_close(pCodecCtx);
  
  // Close the video file
  avformat_close_input(&pFormatCtx);
  
  return 0;
}
