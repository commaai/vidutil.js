#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include <sys/time.h>

#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#ifndef min
#define min(a,b) \
 ({ __typeof__ (a) _a = (a); \
     __typeof__ (b) _b = (b); \
   _a < _b ? _a : _b; })
#endif

#ifndef max
#define max(a,b) \
 ({ __typeof__ (a) _a = (a); \
     __typeof__ (b) _b = (b); \
   _a > _b ? _a : _b; })
#endif


#define IOBUFFER_SIZE (8*1024)

typedef struct BufCtx {
  const uint8_t *buf;
  size_t size;
  size_t pos;
} BufCtx;

static int buf_read_packet(void *opaque, uint8_t *buf, int buf_size) {
  BufCtx *ctx = (BufCtx*)opaque;
  size_t endpos = min(ctx->pos+buf_size, ctx->size);
  size_t readlen = max(0, endpos - ctx->pos);
  memcpy(buf, ctx->buf+ctx->pos, readlen);

  ctx->pos += readlen;
  return readlen;
}

// static int64_t buf_seek(void *opaque, int64_t offset, int whence) {
//   BufCtx *ctx = (BufCtx*)opaque;
//   if (whence == 0) {
//     ctx->pos = offset;
//   } else if (whence == 1) {
//     ctx->pos += offset;
//   } else if (whence == 2) {
//     ctx->pos = ctx->size + offset;
//   } else {
//     assert(false);
//   }
//   ctx->pos = clamp(ctx->pos, 0, ctx->size);
//   return ctx->pos;
// }


static double millis(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0);
}

void viddecode_init(void) {
  av_register_all();
}

int viddecode_decode(const uint8_t* rawdat, size_t datlen, const char* vid_fmt,
                     int (*frame_cb)(int w, int h, uint8_t **data, int *linesize)) {
  int err;

  BufCtx ctx = (BufCtx){
    .buf = rawdat,
    .size = datlen,
    .pos = 0,
  };

  void* iobuffer = av_malloc(IOBUFFER_SIZE);
  assert(iobuffer);
  AVIOContext *ioctx = avio_alloc_context(iobuffer, IOBUFFER_SIZE, 0,
                                          (void*)&ctx, buf_read_packet, NULL, NULL /*, buf_seek*/);
  assert(ioctx);

  AVFormatContext *fctx = avformat_alloc_context();
  assert(fctx);
  fctx->pb = ioctx;

  AVInputFormat *format = av_find_input_format(vid_fmt);
  assert(format);

  err = avformat_open_input(&fctx, "", format, NULL);
  assert(err == 0);

  assert(fctx->streams && fctx->streams[0]);
  AVStream *st = fctx->streams[0];
  
  AVCodec *dec = avcodec_find_decoder(st->codecpar->codec_id);
  assert(dec);

  AVCodecContext *decctx = avcodec_alloc_context3(dec);
  assert(decctx);

  err = avcodec_parameters_to_context(decctx, st->codecpar);
  assert(err == 0);

  err = avcodec_open2(decctx, dec, NULL);
  assert(err == 0);


  AVFrame *frame = av_frame_alloc();
  assert(frame);

  AVPacket pkt;
  av_init_packet(&pkt);
  pkt.data = NULL;
  pkt.size = 0;

  double t1 = millis();

  int running = 1;
  while (av_read_frame(fctx, &pkt) >= 0 && running) {
    // printf("read pkt\n");
    AVPacket orig_pkt = pkt;
    do {
      assert(pkt.stream_index == 0);
      int got_frame = 0;
      err = avcodec_decode_video2(decctx, frame, &got_frame, &pkt);
      assert(err >= 0);

      if (got_frame) {
        running = frame_cb(frame->width, frame->height, frame->data, frame->linesize);
      }

      pkt.data += err;
      pkt.size -= err;
    } while (pkt.size > 0 && running);

    av_packet_unref(&orig_pkt);
  }

  // flush
  pkt.data = NULL;
  pkt.size = 0;
  if (running) {
    int got_frame = 0;
    do {
      err = avcodec_decode_video2(decctx, frame, &got_frame, &pkt);
      assert(err >= 0);

      if (got_frame) {
        running = frame_cb(frame->width, frame->height, frame->data, frame->linesize);
      }

    } while (got_frame && running);
  }

  double t2 = millis();

  printf("time %f\n", (t2-t1));

  avformat_close_input(&fctx);

  av_freep(&ioctx->buffer);
  av_freep(&ioctx);

  return 0;
}
