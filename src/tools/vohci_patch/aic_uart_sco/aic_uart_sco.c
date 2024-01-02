#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/skbuff.h>
#include <linux/usb.h>
#include <linux/poll.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/version.h>

#include <linux/usb/audio.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

#define USB_OR_UART         0//if driver is usb ,value is 1. if driver is uart ,value is 0
#define AIC_SCO_PRINT       0

#define AIC_SCO_ID "snd_sco_aic"
#define IOCTL_CHAR_DEVICE_NAME "aic_uart_sco_dev"

#define SET_SCO_OPEN	_IOR('E', 181, int)
#define SET_SCO_CLOSE	_IOR('E', 182, int)
#define SET_UART_SCO_OPEN    0x50
#define SET_UART_SCO_CLOSE   0x51
#define SET_SELECT_MSBC      0x52

enum CODEC_TYPE{
    CODEC_CVSD,
    CODEC_MSBC,
};

enum {
	USB_CAPTURE_RUNNING,
	USB_PLAYBACK_RUNNING,
	ALSA_CAPTURE_OPEN,
	ALSA_PLAYBACK_OPEN,
	ALSA_CAPTURE_RUNNING,
	ALSA_PLAYBACK_RUNNING,
	CAPTURE_URB_COMPLETED,
	PLAYBACK_URB_COMPLETED,
	DISCONNECTED,
};

// AIC sound card
typedef struct AIC_sco_card {
    struct snd_card *card;
    struct snd_pcm *pcm;
    unsigned long states;
	struct uart_sco_data *snd_data;
    struct aic_sco_stream {
		    struct snd_pcm_substream *substream;
		    unsigned int sco_packet_bytes;
		    snd_pcm_uframes_t buffer_pos;
            unsigned int channels;
	  } capture, playback;
    spinlock_t capture_lock;
    spinlock_t playback_lock;
    struct work_struct send_sco_work;
} AIC_sco_card_t;


struct uart_sco_data {
    AIC_sco_card_t  *pSCOSnd;

};

struct snd_sco_cap_timer {
	struct timer_list cap_timer;
	struct timer_list play_timer;
	struct uart_sco_data *snd_data;
	int snd_sco_length;
};
static struct snd_sco_cap_timer snd_cap_timer;
static struct uart_sco_data p_uart_sco;
static dev_t ioctl_devid; /* bt char device number */
static struct cdev ioctl_char_dev; /* bt character device structure */
static struct class *ioctl_char_class; /* device class for usb char driver */
static enum CODEC_TYPE codec_type = CODEC_CVSD;
extern struct file_operations ioctl_chrdev_ops;

struct device *g_dev;
static void set_select_msbc(enum CODEC_TYPE type)
{
    printk("%s, codec type %d",__FUNCTION__,(int)type);
    codec_type = type;
}

static enum CODEC_TYPE check_select_msbc(void)
{
    return codec_type;
}

static int snd_copy_send_sco_data( AIC_sco_card_t *pSCOSnd,u8 *buffer)
{
    struct snd_pcm_runtime *runtime = pSCOSnd->playback.substream->runtime;
  	unsigned int frame_bytes = 2, frames1;
    const u8 *source;

    snd_pcm_uframes_t period_size = runtime->period_size;
    int count;
    //u8 buffer[period_size * 3];
    int sco_packet_bytes = pSCOSnd->playback.sco_packet_bytes;
    //struct sk_buff *skb;
	switch(runtime->channels){
		case 1:
			frame_bytes = 2;
			break;
		case 2:
			frame_bytes = 4;
			break;
		default:
			break;
	}

    count = frames_to_bytes(runtime, period_size)/sco_packet_bytes;
#if AIC_SCO_PRINT
    printk("%s, buffer_pos:%d sco_packet_bytes:%d count:%d", __FUNCTION__, (int)pSCOSnd->playback.buffer_pos,
    sco_packet_bytes, count);
#endif
    source = runtime->dma_area + pSCOSnd->playback.buffer_pos * frame_bytes;

    if (pSCOSnd->playback.buffer_pos + period_size <= runtime->buffer_size) {
      memcpy(buffer, source, period_size * frame_bytes);
    } else {
      /* wrap around at end of ring buffer */
      frames1 = runtime->buffer_size - pSCOSnd->playback.buffer_pos;
      memcpy(buffer, source, frames1 * frame_bytes);
      memcpy(&buffer[frames1 * frame_bytes],
             runtime->dma_area, (period_size - frames1) * frame_bytes);
    }

    pSCOSnd->playback.buffer_pos += period_size;
    if ( pSCOSnd->playback.buffer_pos >= runtime->buffer_size)
       pSCOSnd->playback.buffer_pos -= runtime->buffer_size;


    if(test_bit(ALSA_PLAYBACK_RUNNING, &pSCOSnd->states)) {
        snd_pcm_period_elapsed(pSCOSnd->playback.substream);
    }
    return count;
}


static void playback_work(struct work_struct *work)
{
    //AIC_sco_card_t *pSCOSnd = container_of(work, AIC_sco_card_t, send_sco_work);


}

/* copy data from the URB buffer into the ALSA ring buffer */
static bool sco_copy_capture_data_to_alsa(struct uart_sco_data *data, uint8_t* p_data, unsigned int frames)
{
  	struct snd_pcm_runtime *runtime;
  	unsigned int frame_bytes, frames1;
  	u8 *dest;
    AIC_sco_card_t  *pSCOSnd = data->pSCOSnd;

  	runtime = pSCOSnd->capture.substream->runtime;
  	frame_bytes = 2;
	switch(runtime->channels){
		case 1:
			frame_bytes = 2;
			break;
		case 2:
			frame_bytes = 4;
			break;
		default:
			break;
	}
  	dest = runtime->dma_area + pSCOSnd->capture.buffer_pos * frame_bytes;
  	if (pSCOSnd->capture.buffer_pos + frames <= runtime->buffer_size) {
  		memcpy(dest, p_data, frames * frame_bytes);
  	} else {
  		/* wrap around at end of ring buffer */
  		frames1 = runtime->buffer_size - pSCOSnd->capture.buffer_pos;
  		memcpy(dest, p_data, frames1 * frame_bytes);
  		memcpy(runtime->dma_area,
  		       p_data + frames1 * frame_bytes,
  		       (frames - frames1) * frame_bytes);
  	}

  	pSCOSnd->capture.buffer_pos += frames;
  	if (pSCOSnd->capture.buffer_pos >= runtime->buffer_size) {
  		pSCOSnd->capture.buffer_pos -= runtime->buffer_size;
  	}
#if AIC_SCO_PRINT
	printk("frames:%d,pos:%d,buf_size:%d,period_size:%d",frames,
		(int)pSCOSnd->capture.buffer_pos,
		(int)runtime->buffer_size,
		(int)runtime->period_size);
#endif
    if((pSCOSnd->capture.buffer_pos%runtime->period_size) == 0) {
        snd_pcm_period_elapsed(pSCOSnd->capture.substream);
    }

  	return false;
}


static void sco_send_to_alsa_ringbuffer(uint8_t* p_data, int sco_length)
{
    AIC_sco_card_t  *pSCOSnd = p_uart_sco.pSCOSnd;
    int input_frames_num;
#if AIC_SCO_PRINT
    printk("%s, alsa sco len %d\n", __func__,sco_length);
#endif

    if (!test_bit(ALSA_CAPTURE_RUNNING, &pSCOSnd->states)) {
#if AIC_SCO_PRINT
        printk("%s: ALSA is not running", __func__);
#endif
        return;
    }
    switch(pSCOSnd->capture.channels){
        case 1:
            input_frames_num = sco_length/2;
            break;
        case 2:
            input_frames_num = sco_length/4;
            break;
        default:
            break;
    }
	snd_cap_timer.snd_sco_length = sco_length;
    sco_copy_capture_data_to_alsa(&p_uart_sco, p_data, input_frames_num);
}


//#ifdef CONFIG_SCO_OVER_HCI
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
void aic_snd_capture_timeout(ulong data)
#else
void aic_snd_capture_timeout(struct timer_list *t)
#endif
{
	uint8_t null_data[480];
	struct uart_sco_data *p_data; 
	AIC_sco_card_t  *pSCOSnd = p_uart_sco.pSCOSnd;
	int input_frames_num;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
    p_data = (struct uart_sco_data *)data;
#else
    p_data = snd_cap_timer.snd_data;
#endif
    switch(pSCOSnd->capture.channels){
        case 1:
            input_frames_num = snd_cap_timer.snd_sco_length/2;
            break;
        case 2:
            input_frames_num = snd_cap_timer.snd_sco_length/4;
            break;
        default:
            break;
    }
    sco_copy_capture_data_to_alsa(p_data, null_data, input_frames_num);
#if AIC_SCO_PRINT
	printk("%s enter\r\n", __func__);
#endif
#if USB_OR_UART
	mod_timer(&snd_cap_timer.cap_timer,jiffies + msecs_to_jiffies(3));
#else
	mod_timer(&snd_cap_timer.cap_timer,jiffies + usecs_to_jiffies(7500));
#endif
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
void aic_snd_play_timeout(ulong data)
#else
void aic_snd_play_timeout(struct timer_list *t)
#endif
{
	AIC_sco_card_t *pSCOSnd;
	struct snd_pcm_runtime *runtime;
	snd_pcm_uframes_t period_size;
    int count;
	struct uart_sco_data *p_data;
	int sco_packet_bytes;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
    p_data = (struct uart_sco_data *)data;
#else
    p_data = snd_cap_timer.snd_data;
#endif
	pSCOSnd = p_data->pSCOSnd;

	if(test_bit(USB_PLAYBACK_RUNNING, &pSCOSnd->states)) {
		return;
	}

	if(!test_bit(ALSA_PLAYBACK_RUNNING, &pSCOSnd->states)) {
		return;
	}

	runtime = pSCOSnd->playback.substream->runtime;
	period_size = runtime->period_size;
    sco_packet_bytes = pSCOSnd->playback.sco_packet_bytes;
    count = frames_to_bytes(runtime, period_size)/sco_packet_bytes;

    pSCOSnd->playback.buffer_pos += period_size;
    if ( pSCOSnd->playback.buffer_pos >= runtime->buffer_size)
       pSCOSnd->playback.buffer_pos -= runtime->buffer_size;

    if(test_bit(ALSA_PLAYBACK_RUNNING, &pSCOSnd->states)) {
        snd_pcm_period_elapsed(pSCOSnd->playback.substream);
    }
    //AICBT_DBG("%s,play_timer restart buffer_pos:%d sco_handle:%d sco_packet_bytes:%d count:%d", __FUNCTION__, pSCOSnd->playback.buffer_pos, pSCOSnd->usb_data->sco_handle,
    //sco_packet_bytes, count);
#if USB_OR_UART
    mod_timer(&snd_cap_timer.play_timer,jiffies + msecs_to_jiffies(3*count));
#else
	mod_timer(&snd_cap_timer.play_timer,jiffies + usecs_to_jiffies(7500*count));
#endif
}

static const struct snd_pcm_hardware snd_card_sco_capture_default =
{
    .info               = (SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_ACCESS_RW_INTERLEAVED | SNDRV_PCM_INFO_FIFO_IN_FRAMES),//SNDRV_PCM_INFO_NONINTERLEAVED |
    .formats            = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S8,
    .rates              = (SNDRV_PCM_RATE_8000),
    .rate_min           = 8000,
    .rate_max           = 8000,
    .channels_min       = 1,//1,
    .channels_max       = 2,//1,
#if USB_OR_UART
	.buffer_bytes_max	= 2 * 8 * 768,//8 * 768,
	.period_bytes_min	= 2 * 48,//48,
	.period_bytes_max	= 2 * 768,//768,
#else
	.buffer_bytes_max	= 2 * 8 * 960,//8 * 960,
	.period_bytes_min	= 2 * 120,//120,
	.period_bytes_max	= 2 * 960,//960,
#endif
    .periods_min        = 1,
    .periods_max        = 8,
    .fifo_size          = 8,

};

static int snd_sco_capture_pcm_open(struct snd_pcm_substream * substream)
{
    AIC_sco_card_t  *pSCOSnd = substream->private_data;

    printk("%s", __FUNCTION__);
    pSCOSnd->capture.substream = substream;

    memcpy(&substream->runtime->hw, &snd_card_sco_capture_default, sizeof(struct snd_pcm_hardware));
	pSCOSnd->capture.buffer_pos = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
	init_timer(&snd_cap_timer.cap_timer);
	snd_cap_timer.cap_timer.data = (unsigned long)pSCOSnd->snd_data;
	snd_cap_timer.cap_timer.function = aic_snd_capture_timeout;
#else
	timer_setup(&snd_cap_timer.cap_timer, aic_snd_capture_timeout, 0);
	snd_cap_timer.snd_data = pSCOSnd->snd_data;
#endif
    if(check_select_msbc() == CODEC_MSBC ) {
        substream->runtime->hw.rates |= SNDRV_PCM_RATE_16000;
        substream->runtime->hw.rate_max = 16000;
        substream->runtime->hw.period_bytes_min = 2 * 240;//240;
        substream->runtime->hw.period_bytes_max = 2 * 8 * 240;//8 * 240;
        substream->runtime->hw.buffer_bytes_max = 2 * 8 * 8 * 240;//8 * 8 * 240;
    }

    set_bit(ALSA_CAPTURE_OPEN, &pSCOSnd->states);
    return 0;
}

static int snd_sco_capture_pcm_close(struct snd_pcm_substream *substream)
{
	AIC_sco_card_t *pSCOSnd = substream->private_data;

	del_timer(&snd_cap_timer.cap_timer);
	clear_bit(ALSA_CAPTURE_OPEN, &pSCOSnd->states);
	return 0;
}

static int snd_sco_capture_ioctl(struct snd_pcm_substream *substream,  unsigned int cmd, void *arg)
{
    printk("%s, cmd = %d", __FUNCTION__, cmd);
    switch (cmd)
    {
        default:
            return snd_pcm_lib_ioctl(substream, cmd, arg);
    }
    return 0;
}

static int snd_sco_capture_pcm_hw_params(struct snd_pcm_substream * substream, struct snd_pcm_hw_params * hw_params)
{

    int err;
    struct snd_pcm_runtime *runtime = substream->runtime;
    err = snd_pcm_lib_alloc_vmalloc_buffer(substream, params_buffer_bytes(hw_params));
    printk("%s,err : %d,  runtime state : %d", __FUNCTION__, err, runtime->status->state);
    return err;
}

static int snd_sco_capture_pcm_hw_free(struct snd_pcm_substream * substream)
{
    printk("%s", __FUNCTION__);
    return snd_pcm_lib_free_vmalloc_buffer(substream);;
}

static int snd_sco_capture_pcm_prepare(struct snd_pcm_substream *substream)
{
    AIC_sco_card_t *pSCOSnd = substream->private_data;
    struct snd_pcm_runtime *runtime = substream->runtime;

    printk("%s %d\n", __FUNCTION__, (int)runtime->period_size);
    if (test_bit(DISCONNECTED, &pSCOSnd->states))
		    return -ENODEV;
	  if (!test_bit(USB_CAPTURE_RUNNING, &pSCOSnd->states))
		    return -EIO;

    if(runtime->rate == 8000) {
#if USB_OR_UART
        pSCOSnd->capture.sco_packet_bytes = 2 * 48;//120;//48;//120;
#else
		pSCOSnd->capture.sco_packet_bytes = 2 * 120;//120;
#endif
    }
    else if(runtime->rate == 16000 && check_select_msbc() == CODEC_MSBC ) {
#if USB_OR_UART
        pSCOSnd->capture.sco_packet_bytes = 2 * 240;//240;
#else
		pSCOSnd->capture.sco_packet_bytes = 2 * 240;//240;
#endif
    }
    else {
        return -ENOEXEC;
    }

    return 0;
}

static int snd_sco_capture_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
    AIC_sco_card_t *pSCOSnd = substream->private_data;
    struct snd_pcm_runtime *runtime = substream->runtime;

    printk("%s, cmd : %d, channels : %d\n", __FUNCTION__, cmd,(int)runtime->channels);

	  switch (cmd) {
	    case SNDRV_PCM_TRIGGER_START:
		      if (!test_bit(USB_CAPTURE_RUNNING, &pSCOSnd->states))
			      return -EIO;
              pSCOSnd->capture.channels = runtime->channels;
		      set_bit(ALSA_CAPTURE_RUNNING, &pSCOSnd->states);
		      return 0;
	    case SNDRV_PCM_TRIGGER_STOP:
		      clear_bit(ALSA_CAPTURE_RUNNING, &pSCOSnd->states);
              pSCOSnd->capture.channels = 0;
		      return 0;
	    default:
		      return -EINVAL;
	  }
}

static snd_pcm_uframes_t snd_sco_capture_pcm_pointer(struct snd_pcm_substream *substream)
{
	  AIC_sco_card_t *pSCOSnd = substream->private_data;

	  return pSCOSnd->capture.buffer_pos;
}


static struct snd_pcm_ops snd_sco_capture_pcm_ops = {
	.open =         snd_sco_capture_pcm_open,
	.close =        snd_sco_capture_pcm_close,
	.ioctl =        snd_sco_capture_ioctl,
	.hw_params =    snd_sco_capture_pcm_hw_params,
	.hw_free =      snd_sco_capture_pcm_hw_free,
	.prepare =      snd_sco_capture_pcm_prepare,
	.trigger =      snd_sco_capture_pcm_trigger,
	.pointer =      snd_sco_capture_pcm_pointer,
};


static const struct snd_pcm_hardware snd_card_sco_playback_default =
{
    .info               = (SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_ACCESS_RW_INTERLEAVED | SNDRV_PCM_INFO_FIFO_IN_FRAMES),//SNDRV_PCM_INFO_NONINTERLEAVED |
    .formats            = SNDRV_PCM_FMTBIT_S16_LE,
    .rates              = (SNDRV_PCM_RATE_8000),
    .rate_min           = 8000,
    .rate_max           = 8000,
    .channels_min       = 1,//1,
    .channels_max       = 2,//1,
#if USB_OR_UART
    .buffer_bytes_max   = 2 * 8 * 768,//8 * 768,
    .period_bytes_min   = 2 * 48,//48,
    .period_bytes_max   = 2 * 768,//768,
#else
	.buffer_bytes_max	= 2 * 8 * 960,//8 * 960,
	.period_bytes_min	= 2 * 120,//120,
	.period_bytes_max	= 2 * 960,//960,
#endif
    .periods_min        = 1,
    .periods_max        = 8,
    .fifo_size          = 8,
};

static int snd_sco_playback_pcm_open(struct snd_pcm_substream * substream)
{
    AIC_sco_card_t *pSCOSnd = substream->private_data;
    int err = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
	init_timer(&snd_cap_timer.play_timer);
	snd_cap_timer.play_timer.data = (unsigned long)pSCOSnd->snd_data;
	snd_cap_timer.play_timer.function = aic_snd_play_timeout;
#else
	timer_setup(&snd_cap_timer.play_timer, aic_snd_play_timeout, 0);
	snd_cap_timer.snd_data = pSCOSnd->snd_data;
#endif
	pSCOSnd->playback.buffer_pos = 0;

    printk("%s, rate : %d", __FUNCTION__, substream->runtime->rate);
    memcpy(&substream->runtime->hw, &snd_card_sco_playback_default, sizeof(struct snd_pcm_hardware));

    if(check_select_msbc()) {
        substream->runtime->hw.rates |= SNDRV_PCM_RATE_16000;
        substream->runtime->hw.rate_max = 16000;
        substream->runtime->hw.period_bytes_min = 2 * 240;//240;
        substream->runtime->hw.period_bytes_max = 2 * 8 * 240;//8 * 240;
        substream->runtime->hw.buffer_bytes_max = 2 * 8 * 8 * 240;//8 * 8 * 240;
    }
    pSCOSnd->playback.substream = substream;
    set_bit(ALSA_PLAYBACK_OPEN, &pSCOSnd->states);

    return err;
}

static int snd_sco_playback_pcm_close(struct snd_pcm_substream *substream)
{
    AIC_sco_card_t *pSCOSnd = substream->private_data;

	del_timer(&snd_cap_timer.play_timer);
	printk("%s: play_timer delete", __func__);
	clear_bit(ALSA_PLAYBACK_OPEN, &pSCOSnd->states);
    cancel_work_sync(&pSCOSnd->send_sco_work);
	  return 0;
}

static int snd_sco_playback_ioctl(struct snd_pcm_substream *substream,  unsigned int cmd, void *arg)
{
    printk("%s, cmd : %d", __FUNCTION__, cmd);
    switch (cmd)
    {
        default:
            return snd_pcm_lib_ioctl(substream, cmd, arg);
            break;
    }
    return 0;
}

static int snd_sco_playback_pcm_hw_params(struct snd_pcm_substream * substream, struct snd_pcm_hw_params * hw_params)
{
    int err;
    err = snd_pcm_lib_alloc_vmalloc_buffer(substream, params_buffer_bytes(hw_params));
    return err;
}

static int snd_sco_palyback_pcm_hw_free(struct snd_pcm_substream * substream)
{
    printk("%s", __FUNCTION__);
    return snd_pcm_lib_free_vmalloc_buffer(substream);
}

static int snd_sco_playback_pcm_prepare(struct snd_pcm_substream *substream)
{
	  AIC_sco_card_t *pSCOSnd = substream->private_data;
    struct snd_pcm_runtime *runtime = substream->runtime;

    printk("%s, bound_rate = %d", __FUNCTION__, runtime->rate);

	  if (test_bit(DISCONNECTED, &pSCOSnd->states))
		    return -ENODEV;
	  if (!test_bit(USB_PLAYBACK_RUNNING, &pSCOSnd->states))
		    return -EIO;

    if(runtime->rate == 8000) {
#if USB_OR_UART
        pSCOSnd->playback.sco_packet_bytes = 2 * 48;//120;//48;//120;
#else
		pSCOSnd->playback.sco_packet_bytes = 2 * 120;//120;
#endif
    }
    else if(runtime->rate == 16000) {
#if USB_OR_UART
		pSCOSnd->playback.sco_packet_bytes = 2 * 240;//240;
#else
		pSCOSnd->playback.sco_packet_bytes = 2 * 240;//240;
#endif
    }

  	return 0;
}

static int snd_sco_playback_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
    AIC_sco_card_t *pSCOSnd = substream->private_data;
    struct snd_pcm_runtime *runtime = substream->runtime;

    printk("%s, cmd = %d ,channels = %d\n", __FUNCTION__, cmd,runtime->channels);
  	switch (cmd) {
        case SNDRV_PCM_TRIGGER_START:
            if (!test_bit(USB_PLAYBACK_RUNNING, &pSCOSnd->states))
                return -EIO;
            pSCOSnd->playback.channels = runtime->channels;
            set_bit(ALSA_PLAYBACK_RUNNING, &pSCOSnd->states);
            schedule_work(&pSCOSnd->send_sco_work);
          if (!test_bit(USB_PLAYBACK_RUNNING, &pSCOSnd->states)) {
              printk("%s: play_timer cmd 1 start ", __func__);
#if USB_OR_UART
              mod_timer(&snd_cap_timer.play_timer,jiffies + msecs_to_jiffies(3));
#else
              mod_timer(&snd_cap_timer.play_timer,jiffies + usecs_to_jiffies(7500));
#endif
          }
            return 0;
      	case SNDRV_PCM_TRIGGER_STOP:
            pSCOSnd->playback.channels = 0;
            clear_bit(ALSA_PLAYBACK_RUNNING, &pSCOSnd->states);
            return 0;
      	default:
            return -EINVAL;
    }
}

static snd_pcm_uframes_t snd_sco_playback_pcm_pointer(struct snd_pcm_substream *substream)
{
  	AIC_sco_card_t *pSCOSnd = substream->private_data;

  	return pSCOSnd->playback.buffer_pos;
}


static struct snd_pcm_ops snd_sco_playback_pcm_ops = {
	.open =         snd_sco_playback_pcm_open,
	.close =        snd_sco_playback_pcm_close,
	.ioctl =        snd_sco_playback_ioctl,
	.hw_params =    snd_sco_playback_pcm_hw_params,
	.hw_free =      snd_sco_palyback_pcm_hw_free,
	.prepare =      snd_sco_playback_pcm_prepare,
	.trigger =      snd_sco_playback_pcm_trigger,
	.pointer =      snd_sco_playback_pcm_pointer,
};


static AIC_sco_card_t* aic_snd_init(void)
{
    struct snd_card *card;
    AIC_sco_card_t  *pSCOSnd;
    int err=0;
	struct device *dev;

	dev = g_dev;

    printk("%s", __func__);
    err = snd_card_new(dev,
     -1, AIC_SCO_ID, THIS_MODULE,
     sizeof(AIC_sco_card_t), &card);
    if (err < 0) {
        printk("%s: sco snd card create fail", __func__);
        return NULL;
    }
    // private data
    pSCOSnd = (AIC_sco_card_t *)card->private_data;
    pSCOSnd->card = card;
	pSCOSnd->snd_data = &p_uart_sco;

    strcpy(card->driver, AIC_SCO_ID);
    strcpy(card->shortname, "Aicsemi sco snd");
    sprintf(card->longname, "Aicsemi sco over hci");

    err = snd_pcm_new(card, AIC_SCO_ID, 0, 1, 1, &pSCOSnd->pcm);
    if (err < 0) {
        printk("%s: sco snd card new pcm fail", __func__);
        return NULL;
    }
    pSCOSnd->pcm->private_data = pSCOSnd;
    sprintf(pSCOSnd->pcm->name, "sco_pcm");

    snd_pcm_set_ops(pSCOSnd->pcm, SNDRV_PCM_STREAM_PLAYBACK, &snd_sco_playback_pcm_ops);
    snd_pcm_set_ops(pSCOSnd->pcm, SNDRV_PCM_STREAM_CAPTURE, &snd_sco_capture_pcm_ops);

    err = snd_card_register(card);
    if (err < 0) {
        printk("%s: sco snd card register card fail", __func__);
        return NULL;
    }

    spin_lock_init(&pSCOSnd->capture_lock);
    spin_lock_init(&pSCOSnd->playback_lock);
    INIT_WORK(&pSCOSnd->send_sco_work, playback_work);
    return pSCOSnd;
}

static int ioctl_open(struct inode *inode_p, struct file  *file_p)
{

    printk("%s: BT sco char device is opening", __func__);
	p_uart_sco.pSCOSnd = aic_snd_init();

    /*
     * As bt device is not re-opened when hotplugged out, we cannot
     * trust on file's private data(may be null) when other file ops
     * are invoked.
     */
    file_p->private_data = &p_uart_sco;

    return nonseekable_open(inode_p, file_p);
}

static int ioctl_close(struct inode  *inode_p, struct file   *file_p)
{
	struct uart_sco_data *data; 
	AIC_sco_card_t *pSCOSnd;

    printk("%s: BT sco char device is closing", __func__);
    /* Not open unless wanna tracing log */
    /* trace_printk("%s: close....\n", __func__); */

    data = file_p->private_data;
    file_p->private_data = NULL;

	pSCOSnd = data->pSCOSnd;
	if(!pSCOSnd) {
		printk("%s: sco private data is null", __func__);
		return -1;
	}
	set_bit(DISCONNECTED, &pSCOSnd->states);
	snd_card_disconnect(pSCOSnd->card);
	snd_card_free_when_closed(pSCOSnd->card);

    return 0;
}

static ssize_t ioctl_read(struct file *file_p,
        char __user *buf_p,
        size_t count,
        loff_t *pos_p)
{
    ssize_t ret = 0;
    unsigned short temp_data[240];
    unsigned short *temp_ptr = NULL;

	struct uart_sco_data *data = file_p->private_data;
    struct snd_pcm_runtime *runtime;
	snd_pcm_uframes_t period_size;
	u8 *buffer;
    char __user *ptr = buf_p;
	int len,sco_count=0;
	int sco_packet_bytes = data->pSCOSnd->playback.sco_packet_bytes;
	int i = 0;
    AIC_sco_card_t  *pSCOSnd = data->pSCOSnd;

    if(test_bit(ALSA_PLAYBACK_RUNNING, &data->pSCOSnd->states)){
		runtime = data->pSCOSnd->playback.substream->runtime;
		period_size = runtime->period_size;
		buffer = (u8 *)vmalloc((3*period_size)+1);
        sco_count = snd_copy_send_sco_data(data->pSCOSnd,&buffer[1]);
#if AIC_SCO_PRINT
		printk("%s sco_count %d,sco_packet_bytes %d\r\n", __func__,sco_count,sco_packet_bytes);
#endif
		if (data->pSCOSnd->capture.sco_packet_bytes != snd_cap_timer.snd_sco_length) {
			if (data->pSCOSnd->capture.sco_packet_bytes > snd_cap_timer.snd_sco_length) {
				buffer[0] = sco_count * (data->pSCOSnd->capture.sco_packet_bytes/snd_cap_timer.snd_sco_length);
			} else {
				buffer[0] = sco_count / (snd_cap_timer.snd_sco_length/data->pSCOSnd->capture.sco_packet_bytes);
			}
		}else{
			buffer[0] = (u8)sco_count;
		}
#if AIC_SCO_PRINT
		printk("%s buffer[0] %d channels %d\r\n", __func__,buffer[0],pSCOSnd->playback.channels);
#endif
        switch(pSCOSnd->playback.channels){
            case 1:
                len = min_t(unsigned int, (sco_count*sco_packet_bytes+1), count);
                break;
            case 2:
                {
                    temp_ptr = (unsigned short *)(buffer+1);
                    for(i = 0; i< (sco_count*sco_packet_bytes/2); i++){
                        temp_data[i] = temp_ptr[i*2];
                    }
                    memcpy(&buffer[1],(u8 *)temp_data,(sco_count*sco_packet_bytes/2));
                    len = min_t(unsigned int, ((sco_count*sco_packet_bytes/2)+1), count);
                }
                break;
            default:
                break;
        }
#if AIC_SCO_PRINT
		printk("%s,len:%d\n", __func__, len);
#endif
		if (copy_to_user(ptr, buffer, len))
			return -EFAULT;
		ret = len;
		vfree(buffer);
    }
    return ret;
}

static ssize_t ioctl_write(struct file *file_p,
        const char __user *buf_p,
        size_t count,
        loff_t *pos_p)
{
	char p_data[1024];
    unsigned short temp_data[240];
    unsigned short *temp_ptr = NULL;
	int ret = 0;
	struct uart_sco_data *data = file_p->private_data;
    AIC_sco_card_t  *pSCOSnd = data->pSCOSnd;

#if AIC_SCO_PRINT
	printk("%s enter ,%d\r\n", __func__,(int)count);
#endif
	memset(p_data, 0, 1024);

	ret = copy_from_user(p_data, (int __user *)buf_p, count);
    if(count == 1){
        switch(p_data[0]){
			case SET_UART_SCO_OPEN:
				printk("sco_uart_open\r\n");
				//ret = copy_from_user(data, (int __user *)arg, 1024);
				//btchr_external_write(&data[1], (int)data[0]);
				set_bit(USB_CAPTURE_RUNNING, &data->pSCOSnd->states);
				set_bit(USB_PLAYBACK_RUNNING, &data->pSCOSnd->states);
				break;
			case SET_UART_SCO_CLOSE:
				printk("sco_uart_close\r\n");
				clear_bit(USB_CAPTURE_RUNNING, &data->pSCOSnd->states);
				clear_bit(USB_PLAYBACK_RUNNING, &data->pSCOSnd->states);
				//AIC_sco_card_t	*pSCOSnd = data->pSCOSnd;
				if (test_bit(ALSA_PLAYBACK_RUNNING, &data->pSCOSnd->states)) {
					mod_timer(&snd_cap_timer.play_timer,jiffies + msecs_to_jiffies(30));
					printk("%s: play_timer start", __func__);
				}
				if (test_bit(ALSA_CAPTURE_RUNNING, &data->pSCOSnd->states)) {
					printk("%s: cap_timer start", __func__);
#if USB_OR_UART
					mod_timer(&snd_cap_timer.cap_timer,jiffies + msecs_to_jiffies(3));
#else
					mod_timer(&snd_cap_timer.cap_timer,jiffies + usecs_to_jiffies(7500));
#endif
				}
				break;
            case SET_SELECT_MSBC:
                set_select_msbc(CODEC_MSBC);
			default:
				break;
		}
	}else{
#if AIC_SCO_PRINT
        printk("%s channels %d\r\n", __func__,pSCOSnd->capture.channels);
#endif
        switch(pSCOSnd->capture.channels){
            case 1:
                sco_send_to_alsa_ringbuffer(p_data,count);
                break;
            case 2:
                {
					int i = 0;
					temp_ptr = (unsigned short *)p_data;
					for(i = 0; i< count/2; i++){
						temp_data[i*2] = temp_ptr[i];
						temp_data[i*2+1] = temp_ptr[i];
					}
					sco_send_to_alsa_ringbuffer((unsigned char *)temp_data,count*2);
                }
                break;
            default:
                break;
        }
	}

    return count;
}

static long ioctl_ioctl(struct file *file_p,unsigned int cmd, unsigned long arg)
{
	struct uart_sco_data *data; 
	data = file_p->private_data;

	printk("%s enter\r\n", __func__);
    switch(cmd) 
    {
        case SET_SCO_OPEN:
			printk("sco_uart_open\r\n");
        	//ret = copy_from_user(data, (int __user *)arg, 1024);
			//btchr_external_write(&data[1], (int)data[0]);
            set_bit(USB_CAPTURE_RUNNING, &data->pSCOSnd->states);
            set_bit(USB_PLAYBACK_RUNNING, &data->pSCOSnd->states);
        break;
		case SET_SCO_CLOSE:
			printk("sco_uart_close\r\n");
			clear_bit(USB_CAPTURE_RUNNING, &data->pSCOSnd->states);
			clear_bit(USB_PLAYBACK_RUNNING, &data->pSCOSnd->states);
			//AIC_sco_card_t	*pSCOSnd = data->pSCOSnd;
			if (test_bit(ALSA_PLAYBACK_RUNNING, &data->pSCOSnd->states)) {
				mod_timer(&snd_cap_timer.play_timer,jiffies + msecs_to_jiffies(30));
				printk("%s: play_timer start", __func__);
			}
		break;
        default:
			printk("unknow cmdr\r\n");
			break;
    }
    return 0;
}


#ifdef CONFIG_COMPAT
static long compat_ioctlchr_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{
    return ioctl_ioctl(filp, cmd, (unsigned long) compat_ptr(arg));
}
#endif


struct file_operations ioctl_chrdev_ops  = {
    open     :           ioctl_open,
    release  :           ioctl_close,
    read     :           ioctl_read,
    write    :           ioctl_write,
    unlocked_ioctl   :   ioctl_ioctl,
#ifdef CONFIG_COMPAT
	compat_ioctl :	compat_ioctlchr_ioctl,
#endif

};

static int __init init_aic_uart_sco(void){
	int res = 0;
	struct device *dev;

#if USB_OR_UART
	printk("%s for USB enter\r\n", __func__);
#else
	printk("%s for UART enter\r\n", __func__);
#endif
		ioctl_char_class = class_create(THIS_MODULE, IOCTL_CHAR_DEVICE_NAME);
		if (IS_ERR(ioctl_char_class)) {
			printk("Failed to create ioctl char class");
		}
	
		res = alloc_chrdev_region(&ioctl_devid, 0, 1, IOCTL_CHAR_DEVICE_NAME);
		if (res < 0) {
			printk("Failed to allocate ioctl char device");
			goto err_alloc;
		}
	
		dev = device_create(ioctl_char_class, NULL, ioctl_devid, NULL, IOCTL_CHAR_DEVICE_NAME);
		if (IS_ERR(dev)) {
			printk("Failed to create ioctl char device");
			res = PTR_ERR(dev);
			goto err_create;
		}
	
		cdev_init(&ioctl_char_dev, &ioctl_chrdev_ops);
		res = cdev_add(&ioctl_char_dev, ioctl_devid, 1);
		if (res < 0) {
			printk("Failed to add ioctl char device");
			goto err_add;
		}

		g_dev = dev;

		return res;
	
err_add:
		device_destroy(ioctl_char_class, ioctl_devid);
err_create:
		unregister_chrdev_region(ioctl_devid, 1);
err_alloc:
		class_destroy(ioctl_char_class);

		return res;

}
static void __exit deinit_aic_uart_sco(void){
	printk("%s enter\r\n", __func__);
    device_destroy(ioctl_char_class, ioctl_devid);
    cdev_del(&ioctl_char_dev);
    unregister_chrdev_region(ioctl_devid, 1);
    class_destroy(ioctl_char_class);
}

module_init(init_aic_uart_sco);
module_exit(deinit_aic_uart_sco);


MODULE_AUTHOR("AicSemi Corporation");
MODULE_DESCRIPTION("AicSemi Bluetooth UART SCO driver version");
MODULE_LICENSE("GPL");

